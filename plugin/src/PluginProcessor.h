// SPDX-License-Identifier: GPL-3.0-only
// Gir Editor in a DAW: an audio effect that leaves the audio as it is (it loads on any track, in Live and in Luna),
// with the editor's page, the FM-1's port and a few parameters the DAW can automate: the six tracks' DEPTH, their
// KILL LOW / MID / HIGH, and the BPM. They reach the FM-1 through the page, by the editor's own protocol and its own
// rate limit (one frame in flight per parameter, the latest value kept). What the project keeps: the open view, the
// chosen track, the window's size. Not the FM-1's content: that stays on the unit.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MidiLink.h"
#include "PortLock.h"
#include "WebUI.h"
#include <array>
#include <atomic>

class GirProcessor : public juce::AudioProcessor,
                     private juce::AudioProcessorParameter::Listener,
                     private juce::Timer
{
public:
    GirProcessor();
    ~GirProcessor() override;

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Gir Editor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Gir Editor"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    void updateTrackProperties (const TrackProperties&) override;

    WebUI& page() { return *web; }
    int width = 1200, height = 800;
    /** set by the open editor: the page's resize requests (the tests) */
    std::function<void (int, int)> resizeEditor;

    static constexpr int tracks = 6;
    static const char* const bands[3];

private:
    void fromPage (const juce::var& m);
    void reply (const juce::var& m, const juce::var& rid);
    void parameterValueChanged (int index, float value) override;
    void parameterGestureChanged (int, bool) override {}
    void timerCallback() override;
    juce::String where() const;
    juce::var paramValue (int index) const;
    void testLine (const juce::var& m);

    std::shared_ptr<int> token = std::make_shared<int> (0);   // (callbacks after the instance went: dropped)
    std::unique_ptr<WebUI> web;
    MidiLink midi;
    PortLock lock;
    juce::String uiState, trackName;
    const int instance;
    bool live = false;

    std::array<juce::AudioParameterFloat*, tracks> depth {};
    std::array<juce::AudioParameterBool*, tracks * 3> kill {};
    juce::AudioParameterInt* bpm = nullptr;
    juce::Array<juce::RangedAudioParameter*> all;
    std::unique_ptr<std::atomic<bool>[]> dirty;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GirProcessor)
};
