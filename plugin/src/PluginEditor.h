// SPDX-License-Identifier: GPL-3.0-only
// The plugin's window: the processor's page in it, resizable from 900 x 600 (the smallest size the layout lint checks)
// to 2560 x 1600; its size kept with the project.
#pragma once
#include "PluginProcessor.h"

class GirEditor : public juce::AudioProcessorEditor
{
public:
    explicit GirEditor (GirProcessor&);
    ~GirEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff101418)); }

private:
    GirProcessor& proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GirEditor)
};
