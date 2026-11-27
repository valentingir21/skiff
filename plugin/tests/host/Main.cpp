// SPDX-License-Identifier: GPL-3.0-only
// girhost: the plugin's test host. No DAW, no FM-1: it loads Gir Editor (AU or VST3), opens its window, runs its audio,
// and offers a virtual FM-1 on CoreMIDI ("Gir Test FM-1": the firmware, natively). The page runs a scenario (its
// autotest, web/gir-editor/src/lib/plugin/autotest.ts) and writes its results, one JSON line each, in
// <out>/results-<instance>.jsonl; a line {step: …} asks the host for something (automate a parameter, close or open
// the window, unplug the virtual FM-1), a line {done: true, ok} ends that instance.
//   girhost --vst3 <path.vst3> | --au   [--instances N] [--emu] [--scenario name] [--out dir] [--timeout s]
#include <juce_audio_utils/juce_audio_utils.h>
#include <thread>

using namespace juce;

extern "C" {
void web_nor_erase (void); void web_master (uint32_t); void web_boot (void); void web_render (uint32_t);
void link_service (void); uint8_t* link_in_buf (void); int32_t link_in (uint32_t); uint8_t* link_out_buf (void);
uint32_t link_out_len (void); void link_out_clear (void); uint32_t link_boot_ms (void); uint32_t web_now_ms (void);
}

static const char* const testPort = "Gir Test FM-1";

// ------------------------------------------------------------------ the virtual FM-1 on CoreMIDI ---
class VirtualFm1 : private Thread, private MidiInputCallback
{
public:
    VirtualFm1() : Thread ("virtual FM-1") {}
    ~VirtualFm1() override { unplug(); stopThread (2000); }

    void start()
    {
        web_nor_erase();
        web_master (820);
        web_boot();
        for (auto at = link_boot_ms(); at && web_now_ms() + 1 < at;)
            web_render (1);
        plug();
        startThread();
    }
    void plug()
    {
        const ScopedLock sl (portLock);
        out = MidiOutput::createNewDevice (testPort);
        in = MidiInput::createNewDevice (testPort, this);
        if (in) in->start();
        std::cout << "virtual FM-1: " << (out && in ? "on " : "FAILED ") << testPort << std::endl;
    }
    void unplug()
    {
        const ScopedLock sl (portLock);
        if (in) in->stop();
        in.reset();
        out.reset();
    }
    std::atomic<int> framesIn { 0 }, framesOut { 0 };

private:
    void handleIncomingMidiMessage (MidiInput*, const MidiMessage& m) override
    {
        if (! m.isSysEx()) return;
        const ScopedLock sl (qLock);
        auto* d = m.getRawData();
        q.emplace_back (d, d + m.getRawDataSize());
    }
    void run() override
    {
        auto next = Time::getMillisecondCounterHiRes();
        while (! threadShouldExit())
        {
            std::vector<uint8> f;
            {
                const ScopedLock sl (qLock);
                if (! q.empty()) { f = std::move (q.front()); q.erase (q.begin()); }
            }
            if (! f.empty() && f.size() <= 702)
            {
                std::memcpy (link_in_buf(), f.data(), f.size());
                link_in ((uint32_t) f.size());
                ++framesIn;
            }
            web_render (128);
            link_service();
            drain();
            next += 128.0 * 1000.0 / 44100.0;
            auto wait = next - Time::getMillisecondCounterHiRes();
            if (wait > 0) Thread::sleep ((int) wait);
            else if (wait < -100) next = Time::getMillisecondCounterHiRes();
        }
    }
    void drain()
    {
        auto n = link_out_len();
        if (n == 0) return;
        auto* b = link_out_buf();
        const ScopedLock sl (portLock);
        for (uint32_t i = 0; i < n; ++i)
        {
            if (b[i] != 0xF0) continue;
            uint32_t j = i + 1;
            while (j < n && b[j] != 0xF7) ++j;
            if (j >= n) break;
            if (out) { out->sendMessageNow (MidiMessage (b + i, (int) (j - i + 1))); ++framesOut; }
            i = j;
        }
        link_out_clear();
    }
    CriticalSection qLock, portLock;
    std::vector<std::vector<uint8>> q;
    std::unique_ptr<MidiOutput> out;
    std::unique_ptr<MidiInput> in;
};

// ------------------------------------------------------------------ the host ---
struct Instance
{
    int n = 0;
    std::unique_ptr<AudioPluginInstance> plugin;
    std::unique_ptr<DocumentWindow> window;
    int lines = 0;
    bool done = false, ok = false;
    File file;
};

class EditorWindow : public DocumentWindow
{
public:
    EditorWindow (const String& title, AudioProcessorEditor* e, int x)
        : DocumentWindow (title, Colours::black, DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (e, true);
        setResizable (true, false);
        setTopLeftPosition (40 + x, 40 + x);
        setVisible (true);
    }
    void closeButtonPressed() override {}
};

class GirHost : public JUCEApplication, private Timer
{
public:
    const String getApplicationName() override { return "girhost"; }
    const String getApplicationVersion() override { return "1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const String&) override
    {
        auto a = getCommandLineParameterArray();
        auto opt = [&] (const String& k, const String& d = {}) { auto i = a.indexOf (k); return i >= 0 && i + 1 < a.size() ? a[i + 1] : d; };
        vst3 = opt ("--vst3");
        useAu = a.contains ("--au");
        count = jmax (1, opt ("--instances", "1").getIntValue());
        out = File (opt ("--out", File::getSpecialLocation (File::tempDirectory).getChildFile ("girhost").getFullPathName()));
        timeout = opt ("--timeout", "120").getDoubleValue();
        scenario = opt ("--scenario", "demo");
        out.createDirectory();
        for (auto& f : out.findChildFiles (File::findFiles, false, "results-*.jsonl"))
            f.deleteFile();
        setenv ("GIR_PLUGIN_TEST_DIR", out.getFullPathName().toRawUTF8(), 1);
        setenv ("GIR_PLUGIN_AUTOTEST", scenario.toRawUTF8(), 1);
        setenv ("GIR_PLUGIN_TEST_PORT", testPort, 1);
        if (std::getenv ("GIR_LOCK_DIR") == nullptr)
            setenv ("GIR_LOCK_DIR", out.getFullPathName().toRawUTF8(), 1);
        out.getChildFile ("port-lock.json").deleteFile();
        if (a.contains ("--emu"))
        {
            emu = std::make_unique<VirtualFm1>();
            emu->start();
        }
        started = Time::getMillisecondCounterHiRes();
        formats.addFormat (std::make_unique<AudioUnitPluginFormat>());
        formats.addFormat (std::make_unique<VST3PluginFormat>());
        if (! describe())
            return quitWith (2, "no plugin found");
        std::cout << "plugin: " << desc.name << " (" << desc.pluginFormatName << ", " << desc.fileOrIdentifier << ")" << std::endl;
        loadNext();
        startTimer (100);
    }
    void shutdown() override
    {
        for (auto& i : instances) { i->window.reset(); i->plugin.reset(); }
        emu.reset();
    }

private:
    bool describe()
    {
        OwnedArray<PluginDescription> found;
        if (useAu)
        {
            for (auto* f : formats.getFormats())
                if (f->getName() == "AudioUnit")
                    for (auto& id : f->searchPathsForPlugins ({}, false, false))
                        if (id.contains ("GirE"))
                            f->findAllTypesForFile (found, id);
        }
        else
            for (auto* f : formats.getFormats())
                if (f->getName() == "VST3")
                    f->findAllTypesForFile (found, vst3);
        if (found.isEmpty()) return false;
        desc = *found[0];
        return true;
    }
    void loadNext()
    {
        if ((int) instances.size() >= count) return;
        formats.createPluginInstanceAsync (desc, 44100.0, 512, [this] (std::unique_ptr<AudioPluginInstance> p, const String& err)
        {
            if (p == nullptr) return quitWith (3, "could not load: " + err);
            auto inst = std::make_unique<Instance>();
            inst->n = (int) instances.size() + 1;
            p->enableAllBuses();
            p->prepareToPlay (44100.0, 512);
            inst->plugin = std::move (p);
            instances.push_back (std::move (inst));
            openEditor (*instances.back());
            Timer::callAfterDelay (1500, [this] { loadNext(); });
        });
    }
    void openEditor (Instance& i)
    {
        if (i.window) return;
        auto* e = i.plugin->createEditorIfNeeded();
        if (e == nullptr) { std::cout << "no editor" << std::endl; return; }
        i.window = std::make_unique<EditorWindow> ("Gir Editor " + String (i.n), e, (i.n - 1) * 60);
    }
    void quitWith (int code, const String& why)
    {
        std::cout << "girhost: " << why << std::endl;
        out.getChildFile ("host-summary.txt").replaceWithText (why + "\n");
        setApplicationReturnValue (code);
        stopTimer();
        // (a moment for the plugin's lock and ports to close)
        Timer::callAfterDelay (300, [] { JUCEApplication::quit(); });
    }
    void timerCallback() override
    {
        AudioBuffer<float> buf (2, 512);
        MidiBuffer midi;
        for (auto& i : instances)
        {
            buf.clear();
            i->plugin->processBlock (buf, midi);
            poll (*i);
        }
        bool all = ! instances.empty() && (int) instances.size() == count;
        bool ok = true;
        for (auto& i : instances) { all = all && i->done; ok = ok && i->ok; }
        if (all)
            return quitWith (ok ? 0 : 1, ok ? "all passed" : "FAILED");
        if ((Time::getMillisecondCounterHiRes() - started) / 1000.0 > timeout)
            return quitWith (4, "timeout");
    }
    // (the plugin numbers its instances itself, and an AU's scan makes one more: each new results file goes to the
    // next instance without one, in order)
    void poll (Instance& i)
    {
        if (i.file == File())
        {
            auto files = out.findChildFiles (File::findFiles, false, "results-*.jsonl");
            files.sort();
            for (auto& f : files)
            {
                bool taken = false;
                for (auto& o : instances) taken = taken || o->file == f;
                if (! taken) { i.file = f; break; }
            }
            if (i.file == File()) return;
        }
        auto f = i.file;
        if (! f.existsAsFile()) return;
        StringArray lines;
        lines.addLines (f.loadFileAsString());
        lines.removeEmptyStrings();
        for (; i.lines < lines.size(); ++i.lines)
        {
            auto v = JSON::parse (lines[i.lines]);
            std::cout << "[" << i.n << "] " << lines[i.lines].substring (0, 300) << std::endl;
            auto step = v["step"].toString();
            if (step == "automate") automate (i, v["id"].toString(), (float) (double) v["value"]);
            else if (step == "close-editor") i.window.reset();
            else if (step == "open-editor") openEditor (i);
            else if (step == "unplug" && emu) emu->unplug();
            else if (step == "replug" && emu) emu->plug();
            if ((bool) v["done"]) { i.done = true; i.ok = (bool) v["ok"]; }
        }
    }
    // the plugin's IDs (depth1, kill2low, bpm) by their names, as a DAW lists them ("T1 DEPTH", "T2 KILL LOW", "BPM"):
    // a hosted VST3's parameter ID is its number
    static String nameOf (const String& id)
    {
        if (id.startsWith ("depth")) return "T" + id.substring (5) + " DEPTH";
        if (id.startsWith ("kill")) return "T" + id.substring (4, 5) + " KILL " + id.substring (5).toUpperCase();
        return id.toUpperCase();
    }
    void automate (Instance& i, const String& id, float value)
    {
        for (auto* p : i.plugin->getParameters())
            if (p->getName (64) == nameOf (id))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (value);
                p->endChangeGesture();
                std::cout << "[" << i.n << "] host: " << id << " = " << value << std::endl;
                return;
            }
        std::cout << "[" << i.n << "] host: no parameter " << id << std::endl;
    }

    AudioPluginFormatManager formats;
    PluginDescription desc;
    String vst3, scenario;
    bool useAu = false;
    int count = 1;
    File out;
    double timeout = 120, started = 0;
    std::vector<std::unique_ptr<Instance>> instances;
    std::unique_ptr<VirtualFm1> emu;
};

START_JUCE_APPLICATION (GirHost)
