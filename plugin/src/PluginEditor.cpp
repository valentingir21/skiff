// SPDX-License-Identifier: GPL-3.0-only
#include "PluginEditor.h"

using namespace juce;

GirEditor::GirEditor (GirProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setResizable (true, true);
    setResizeLimits (900, 600, 2560, 1600);
    addAndMakeVisible (proc.page());
    setSize (proc.width, proc.height);
    proc.resizeEditor = [this] (int w, int h) { setSize (w, h); };
}

GirEditor::~GirEditor()
{
    proc.resizeEditor = nullptr;
    removeChildComponent (&proc.page());     // (the page stays with the processor: closing the window keeps it)
}

void GirEditor::resized()
{
    proc.page().setBounds (getLocalBounds());
    proc.width = getWidth();
    proc.height = getHeight();
}
