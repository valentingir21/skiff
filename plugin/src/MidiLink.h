// SPDX-License-Identifier: GPL-3.0-only
// The FM-1's port, opened by the plugin itself (CoreMIDI, juce::MidiInput / MidiOutput): WKWebView has no Web MIDI.
// Its SysEx frames go to the page (the editor's PluginTransport) in batches; the rest (notes, clock) stays here: the
// protocol ignores it, and on a Mac the DAW keeps the same port for its notes (CoreMIDI shares ports).
#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <functional>
#include <vector>

class MidiLink : private juce::MidiInputCallback, private juce::AsyncUpdater
{
public:
    MidiLink();
    ~MidiLink() override;

    /** {inputs: [{id, name}], outputs: [{id, name}]} */
    juce::var ports() const;
    /** "" when open, else "busy" */
    juce::String open (const juce::String& inputId, const juce::String& outputId);
    void close();
    bool isOpen() const { return out != nullptr; }
    juce::String portName() const { return name; }
    void send (const juce::uint8* data, int n);

    /** message thread: the SysEx frames that came in since the last call */
    std::function<void (std::vector<std::vector<juce::uint8>>&&)> onFrames;
    /** message thread: the port went (unplugged); the port list changed */
    std::function<void()> onLost, onDevices;

private:
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void handleAsyncUpdate() override;
    void devicesChanged();

    std::unique_ptr<juce::MidiInput> in;
    std::unique_ptr<juce::MidiOutput> out;
    juce::String inId, outId, name;
    juce::CriticalSection qLock;
    std::vector<std::vector<juce::uint8>> queue;
    juce::MidiDeviceListConnection devices;
};
