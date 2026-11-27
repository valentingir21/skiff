// SPDX-License-Identifier: GPL-3.0-only
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

const char* const GirProcessor::bands[3] = { "LOW", "MID", "HIGH" };
static std::atomic<int> instances { 0 };
static thread_local bool fromPageNow = false;     // (a value the page set: not sent back to it)

static var obj (std::initializer_list<std::pair<const char*, var>> kv)
{
    DynamicObject::Ptr o = new DynamicObject();
    for (auto& [k, v] : kv)
        o->setProperty (k, v);
    return var (o.get());
}

static String hexOf (const uint8* d, size_t n)
{
    static const char* h = "0123456789abcdef";
    String s;
    s.preallocateBytes (n * 2 + 1);
    for (size_t i = 0; i < n; ++i)
    {
        s += h[d[i] >> 4];
        s += h[d[i] & 15];
    }
    return s;
}

GirProcessor::GirProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      instance (++instances)
{
    for (int k = 0; k < tracks; ++k)
    {
        auto id = "depth" + String (k + 1);
        depth[(size_t) k] = new AudioParameterFloat (ParameterID { id, 1 }, "T" + String (k + 1) + " DEPTH",
                                                     NormalisableRange<float> (0.0f, 1.0f), 0.5f);
        addParameter (depth[(size_t) k]);
        all.add (depth[(size_t) k]);
    }
    for (int k = 0; k < tracks; ++k)
        for (int b = 0; b < 3; ++b)
        {
            auto id = "kill" + String (k + 1) + String (bands[b]).toLowerCase();
            auto* p = new AudioParameterBool (ParameterID { id, 1 }, "T" + String (k + 1) + " KILL " + bands[b], false);
            kill[(size_t) (k * 3 + b)] = p;
            addParameter (p);
            all.add (p);
        }
    bpm = new AudioParameterInt (ParameterID { "bpm", 1 }, "BPM", 40, 240, 120);
    addParameter (bpm);
    all.add (bpm);
    dirty.reset (new std::atomic<bool>[(size_t) all.size()]);
    for (int i = 0; i < all.size(); ++i)
    {
        dirty[(size_t) i] = false;
        all[i]->addListener (this);
    }

    auto env = [] (const char* n) { auto* v = std::getenv (n); return v ? String (v) : String(); };
    auto init = obj ({ { "instance", instance }, { "version", GIR_VERSION_LABEL },
                       { "host", PluginHostType().getHostDescription() },
                       { "format", String (getWrapperTypeDescription (wrapperType)) },
                       { "autotest", env ("GIR_PLUGIN_AUTOTEST") } });
    web = std::make_unique<WebUI> (init, [this] (const var& m) { fromPage (m); });

    midi.onFrames = [this] (std::vector<std::vector<uint8>>&& frames)
    {
        Array<var> hex;
        for (auto& f : frames)
            hex.add (hexOf (f.data(), f.size()));
        web->post (obj ({ { "type", "midi" }, { "frames", hex } }));
    };
    midi.onLost = [this] { lock.release(); web->post (obj ({ { "type", "lost" } })); };
    midi.onDevices = [this] { web->post (obj ({ { "type", "devices" }, { "ports", midi.ports() } })); };
    lock.onTaken = [this] (const String& by)
    {
        midi.close();
        live = false;
        web->post (obj ({ { "type", "taken" }, { "by", by } }));
    };
    startTimerHz (30);
}

GirProcessor::~GirProcessor()
{
    stopTimer();
    for (auto* p : all)
        p->removeListener (this);
    midi.onFrames = nullptr;
    midi.onLost = midi.onDevices = nullptr;
    lock.onTaken = nullptr;
    midi.close();
    lock.release();
    web.reset();
}

bool GirProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo());
}

void GirProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    // the audio passes as it is
    for (auto c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());
}

AudioProcessorEditor* GirProcessor::createEditor() { return new GirEditor (*this); }

String GirProcessor::where() const
{
    // (no language here: the page says it, lib/plugin/where.ts) "host|track|instance"
    String host (PluginHostType().getHostDescription());
    if (host == "Unknown")
        host = {};
    return host.replaceCharacter ('|', '/') + "|" + trackName.replaceCharacter ('|', '/') + "|" + String (instance);
}

void GirProcessor::updateTrackProperties (const TrackProperties& p)
{
    auto name = p.name.value_or (String());
    MessageManager::callAsync ([this, name, alive = std::weak_ptr<int> (token)]
    {
        if (alive.expired())
            return;
        trackName = name;
        lock.setLabel (where());
        web->post (obj ({ { "type", "track" }, { "name", name }, { "where", where() } }));
    });
}

void GirProcessor::reply (const var& m, const var& rid)
{
    if (auto* o = m.getDynamicObject())
    {
        o->setProperty ("type", "reply");
        o->setProperty ("rid", rid);
    }
    web->post (m);
}

var GirProcessor::paramValue (int i) const
{
    auto* p = all[i];
    if (p == bpm)
        return bpm->get();
    if (auto* b = dynamic_cast<AudioParameterBool*> (p))
        return b->get() ? 1 : 0;
    return (double) p->getValue();
}

void GirProcessor::fromPage (const var& m)
{
    const auto type = m["type"].toString();
    const auto rid = m["rid"];
    if (type == "ready")
    {
        web->pageReady();
        web->post (obj ({ { "type", "init" }, { "ui", uiState }, { "where", where() }, { "track", trackName }, { "awake", web->awake },
                          { "w", width }, { "h", height } }));
    }
    else if (type == "ports")
        reply (obj ({ { "ports", midi.ports() } }), rid);
    else if (type == "open")
    {
        // (the tests) only the test host's virtual FM-1: never a real unit plugged into the same Mac
        if (auto* only = std::getenv ("GIR_PLUGIN_TEST_PORT"))
        {
            String inName, outName;
            for (auto& d : MidiInput::getAvailableDevices()) if (d.identifier == m["input"].toString()) inName = d.name;
            for (auto& d : MidiOutput::getAvailableDevices()) if (d.identifier == m["output"].toString()) outName = d.name;
            if (inName != only || outName != only)
                return reply (obj ({ { "ok", false }, { "error", "busy" } }), rid);
        }
        String by;
        if (! lock.acquire ((bool) m["force"], by))
            return reply (obj ({ { "ok", false }, { "error", "held" }, { "by", by } }), rid);
        lock.setLabel (where());
        auto err = midi.open (m["input"].toString(), m["output"].toString());
        if (err.isNotEmpty())
        {
            lock.release();
            return reply (obj ({ { "ok", false }, { "error", err } }), rid);
        }
        reply (obj ({ { "ok", true }, { "name", midi.portName() } }), rid);
    }
    else if (type == "close")
    {
        midi.close();
        lock.release();
        live = false;
    }
    else if (type == "send")
    {
        MemoryBlock mb;
        mb.loadFromHexString (m["data"].toString());
        midi.send ((const uint8*) mb.getData(), (int) mb.getSize());
    }
    else if (type == "ui")
        uiState = m["state"].toString();
    else if (type == "live")
    {
        live = (bool) m["on"];
        for (int i = 0; i < all.size(); ++i)
            dirty[(size_t) i] = false;
    }
    else if (type == "params")
    {
        // the unit's values, shown by the DAW: set here without being sent back (and recorded when the DAW records)
        if (auto* o = m["values"].getDynamicObject())
            for (auto& kv : o->getProperties())
                for (auto* p : all)
                    if (p->getParameterID() == kv.name.toString())
                    {
                        float n = p == bpm ? bpm->convertTo0to1 ((float) (double) kv.value)
                                           : jlimit (0.0f, 1.0f, (float) (double) kv.value);
                        if (std::abs (n - p->getValue()) < 1.0e-4f)
                            break;
                        const ScopedValueSetter<bool> s (fromPageNow, true);
                        const bool gesture = (bool) m["gesture"];
                        if (gesture) p->beginChangeGesture();
                        p->setValueNotifyingHost (n);
                        if (gesture) p->endChangeGesture();
                        break;
                    }
    }
    else if (type == "test")
        testLine (m);
    else if (type == "save")
    {
        // WKWebView has no downloads: the editor's files (backups, libraries, .syx) go to ~/Documents/Gir Editor/
        auto* t = std::getenv ("GIR_PLUGIN_TEST_DIR");
        auto dir = t != nullptr ? File (String (t)).getChildFile ("saved")
                                : File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Gir Editor");
        dir.createDirectory();
        auto f = dir.getNonexistentChildFile (File::createLegalFileName (m["name"].toString()).upToLastOccurrenceOf (".", false, false),
                                              "." + m["name"].toString().fromLastOccurrenceOf (".", false, false), false);
        MemoryBlock mb;
        mb.loadFromHexString (m["data"].toString());
        const bool ok = mb.getSize() > 0 && f.replaceWithData (mb.getData(), mb.getSize());
        web->post (obj ({ { "type", "saved" }, { "ok", ok }, { "path", f.getFullPathName().replace (File::getSpecialLocation (File::userHomeDirectory).getFullPathName(), "~") } }));
    }
    else if (type == "snap")
    {
        auto dir = File (String (std::getenv ("GIR_PLUGIN_TEST_DIR") ? std::getenv ("GIR_PLUGIN_TEST_DIR") : "/tmp"));
        dir.createDirectory();
        auto png = dir.getChildFile (File::createLegalFileName (m["name"].toString()) + ".png");
        web->snapshot (png, [this, rid, alive = std::weak_ptr<int> (token)] (bool ok)
        {
            if (! alive.expired())
                reply (obj ({ { "ok", ok } }), rid);
        });
    }
    else if (type == "resize")
    {
        width = (int) m["w"];
        height = (int) m["h"];
        if (resizeEditor)
            resizeEditor (width, height);
        Timer::callAfterDelay (400, [this, rid, alive = std::weak_ptr<int> (token)]
        {
            if (! alive.expired())
                reply (obj ({ { "ok", resizeEditor != nullptr } }), rid);
        });
    }
}

void GirProcessor::testLine (const var& m)
{
    // (the tests: tests/host) the page's results, one JSON line each, results-<instance>.jsonl
    auto* dir = std::getenv ("GIR_PLUGIN_TEST_DIR");
    if (dir == nullptr)
        return;
    File d { String (dir) };
    d.createDirectory();
    d.getChildFile ("results-" + String (instance) + ".jsonl").appendText (JSON::toString (m, true) + "\n");
}

void GirProcessor::parameterValueChanged (int index, float)
{
    if (fromPageNow)
        return;
    for (int i = 0; i < all.size(); ++i)
        if (all[i]->getParameterIndex() == index)
            dirty[(size_t) i] = true;
}

void GirProcessor::timerCallback()
{
    if (! live)
        return;
    for (int i = 0; i < all.size(); ++i)
        if (dirty[(size_t) i].exchange (false))
            web->post (obj ({ { "type", "param" }, { "id", all[i]->getParameterID() }, { "value", paramValue (i) } }));
}

void GirProcessor::getStateInformation (MemoryBlock& dest)
{
    auto s = JSON::toString (obj ({ { "v", 1 }, { "ui", uiState }, { "w", width }, { "h", height } }), true);
    dest.replaceAll (s.toRawUTF8(), s.getNumBytesAsUTF8());
}

void GirProcessor::setStateInformation (const void* data, int size)
{
    auto v = JSON::parse (String::fromUTF8 ((const char*) data, size));
    if (! v.isObject())
        return;
    uiState = v["ui"].toString();
    width = jlimit (900, 2560, (int) v.getProperty ("w", 1200));
    height = jlimit (600, 1600, (int) v.getProperty ("h", 800));
    if (resizeEditor)
        resizeEditor (width, height);
    web->post (obj ({ { "type", "ui" }, { "state", uiState } }));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GirProcessor(); }
