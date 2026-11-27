// SPDX-License-Identifier: GPL-3.0-only
#include "MidiLink.h"

using namespace juce;

MidiLink::MidiLink()
{
    devices = MidiDeviceListConnection::make ([this] { devicesChanged(); });
}

MidiLink::~MidiLink()
{
    cancelPendingUpdate();
    close();
}

static var list (const Array<MidiDeviceInfo>& a)
{
    Array<var> r;
    for (auto& d : a)
    {
        DynamicObject::Ptr o = new DynamicObject();
        o->setProperty ("id", d.identifier);
        o->setProperty ("name", d.name);
        r.add (var (o.get()));
    }
    return r;
}

var MidiLink::ports() const
{
    DynamicObject::Ptr o = new DynamicObject();
    o->setProperty ("inputs", list (MidiInput::getAvailableDevices()));
    o->setProperty ("outputs", list (MidiOutput::getAvailableDevices()));
    return var (o.get());
}

String MidiLink::open (const String& inputId, const String& outputId)
{
    close();
    auto i = MidiInput::openDevice (inputId, this);
    auto o = MidiOutput::openDevice (outputId);
    if (i == nullptr || o == nullptr)
        return "busy";
    in = std::move (i);
    out = std::move (o);
    inId = inputId;
    outId = outputId;
    name = in->getName();
    in->start();
    return {};
}

void MidiLink::close()
{
    if (in != nullptr)
        in->stop();
    in.reset();
    out.reset();
    inId = outId = {};
    const ScopedLock sl (qLock);
    queue.clear();
}

void MidiLink::send (const uint8* data, int n)
{
    if (out == nullptr || n <= 0)
        return;
    out->sendMessageNow (MidiMessage (data, n));
}

void MidiLink::handleIncomingMidiMessage (MidiInput*, const MidiMessage& m)
{
    if (! m.isSysEx())
        return;
    auto* d = m.getRawData();
    {
        const ScopedLock sl (qLock);
        queue.emplace_back (d, d + m.getRawDataSize());
    }
    triggerAsyncUpdate();
}

void MidiLink::handleAsyncUpdate()
{
    std::vector<std::vector<uint8>> got;
    {
        const ScopedLock sl (qLock);
        got.swap (queue);
    }
    if (! got.empty() && onFrames)
        onFrames (std::move (got));
}

void MidiLink::devicesChanged()
{
    if (out != nullptr)
    {
        auto has = [] (const Array<MidiDeviceInfo>& a, const String& id)
        {
            for (auto& d : a)
                if (d.identifier == id)
                    return true;
            return false;
        };
        if (! has (MidiInput::getAvailableDevices(), inId) || ! has (MidiOutput::getAvailableDevices(), outId))
        {
            close();
            if (onLost)
                onLost();
        }
    }
    if (onDevices)
        onDevices();
}
