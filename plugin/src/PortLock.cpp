// SPDX-License-Identifier: GPL-3.0-only
#include "PortLock.h"
#include <signal.h>
#include <errno.h>
#include <unistd.h>

using namespace juce;

File PortLock::lockFile()
{
    if (auto* dir = std::getenv ("GIR_LOCK_DIR"))          // (the tests: a lock of their own)
        return File (String (dir)).getChildFile ("port-lock.json");
    return File::getSpecialLocation (File::userApplicationDataDirectory)
        .getChildFile ("Application Support").getChildFile ("Gir Editor").getChildFile ("port-lock.json");
}

static bool alive (int pid)
{
    return pid > 0 && (kill (pid, 0) == 0 || errno == EPERM);
}

static std::optional<PortLock::Holder> readLock()
{
    auto f = PortLock::lockFile();
    if (! f.existsAsFile())
        return std::nullopt;
    auto v = JSON::parse (f.loadFileAsString());
    if (! v.isObject())
        return std::nullopt;
    PortLock::Holder h;
    h.id = v["id"].toString();
    h.app = v["app"].toString();
    h.label = v["label"].toString();
    h.pid = (int) v["pid"];
    h.t = (int64) v["t"];
    return h;
}

PortLock::PortLock() : id (Uuid().toString()) {}

PortLock::~PortLock() { release(); }

std::optional<PortLock::Holder> PortLock::heldElsewhere() const
{
    auto h = readLock();
    if (! h || h->id == id)
        return std::nullopt;
    if (Time::currentTimeMillis() - h->t > staleMs || ! alive (h->pid))
        return std::nullopt;
    return h;
}

bool PortLock::acquire (bool force, String& heldBy)
{
    if (auto h = heldElsewhere(); h && ! force)
    {
        heldBy = h->label.isNotEmpty() ? h->label : h->app;
        return false;
    }
    isOwner = true;
    write();
    startTimer (1000);
    return true;
}

void PortLock::release()
{
    stopTimer();
    if (! isOwner)
        return;
    isOwner = false;
    if (auto h = readLock(); h && h->id == id)
        lockFile().deleteFile();
}

void PortLock::setLabel (const String& l)
{
    label = l;
    if (isOwner)
        write();
}

void PortLock::write()
{
    auto f = lockFile();
    f.getParentDirectory().createDirectory();
    DynamicObject::Ptr o = new DynamicObject();
    o->setProperty ("id", id);
    o->setProperty ("pid", (int) getpid());
    o->setProperty ("app", "plugin");
    o->setProperty ("label", label);
    o->setProperty ("t", Time::currentTimeMillis());
    f.replaceWithText (JSON::toString (var (o.get()), true));
}

void PortLock::timerCallback()
{
    if (! isOwner)
        return stopTimer();
    auto h = readLock();
    if (h && h->id != id)
    {
        isOwner = false;
        stopTimer();
        if (onTaken)
            onTaken (h->label.isNotEmpty() ? h->label : h->app);
        return;
    }
    write();
}
