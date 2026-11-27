// SPDX-License-Identifier: GPL-3.0-only
// One editor talks to the FM-1 at a time: every plugin instance (in any DAW) and the desktop app share one lock file,
// ~/Library/Application Support/Gir Editor/port-lock.json: {id, pid, app, label, t}. Its owner rewrites t every
// second; a lock older than 4 s, or whose process is gone, is free. Taking over rewrites it: the old owner sees
// another id at its next beat and lets go of the port. The desktop app's electron/main.cjs keeps the same file.
#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <functional>

class PortLock : private juce::Timer
{
public:
    PortLock();
    ~PortLock() override;

    struct Holder { juce::String id, app, label; int pid = 0; juce::int64 t = 0; };
    /** another editor holding the port now (fresh), if any */
    std::optional<Holder> heldElsewhere() const;
    /** take the lock (force: even from another holder); false and who holds it when held and not forced */
    bool acquire (bool force, juce::String& heldBy);
    void release();
    bool owned() const { return isOwner; }
    void setLabel (const juce::String& l);
    /** called (message thread) when another editor took the lock over */
    std::function<void (const juce::String& by)> onTaken;

    static juce::File lockFile();
    static constexpr juce::int64 staleMs = 4000;

private:
    void timerCallback() override;
    void write();
    juce::String id, label;
    bool isOwner = false;
};
