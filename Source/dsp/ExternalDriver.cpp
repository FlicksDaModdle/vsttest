#include "ExternalDriver.h"

#include <algorithm>
#include <cmath>

namespace gamaka
{

void ExternalDriver::add (std::vector<MidiOutEvent>& out, int sample, uint8_t a, uint8_t b, uint8_t c) const
{
    out.push_back ({ sample, { (uint8_t) (a | channel), (uint8_t) (b & 0x7f), (uint8_t) (c & 0x7f) }, 3 });
}

void ExternalDriver::writeSetup (std::vector<MidiOutEvent>& out, int sample) const
{
    add (out, sample, 0xb0, 101, 0);
    add (out, sample, 0xb0, 100, 0);
    add (out, sample, 0xb0, 6, (uint8_t) std::min (127, bendRange));
    add (out, sample, 0xb0, 38, 0);
    add (out, sample, 0xb0, 101, 127); // RPN null
    add (out, sample, 0xb0, 100, 127);
}

void ExternalDriver::writeReset (std::vector<MidiOutEvent>& out, int sample)
{
    if (note >= 0)
        add (out, sample, 0x80, (uint8_t) note, 0);
    note = -1;
    add (out, sample, 0xe0, 0, 64);
    lastBend = 8192;
}

void ExternalDriver::bend (std::vector<MidiOutEvent>& out, int sample, double pitch, bool force)
{
    const double dev = (pitch - anchor) / bendRange;
    const int value = std::clamp ((int) std::lround (8192.0 + dev * 8191.0), 0, 16383);
    if (force || value != lastBend)
    {
        add (out, sample, 0xe0, (uint8_t) (value & 0x7f), (uint8_t) (value >> 7));
        lastBend = value;
    }
}

void ExternalDriver::process (const Synth& synth, const float* midiPitch, int numSamples,
                              std::vector<MidiOutEvent>& out)
{
    int ev = 0;
    const int numEvents = synth.getNumEvents();
    for (int i = 0; i < numSamples; ++i)
    {
        bool articulated = false;
        for (; ev < numEvents && synth.getEvent (ev).sample <= i; ++ev)
        {
            const auto& e = synth.getEvent (ev);
            if (e.type == VoiceEvent::On && (note < 0 || ! e.legato))
            {
                // A fresh articulation: bend first so the attack is already in tune.
                const int previous = note;
                anchor = std::clamp ((int) std::lround (midiPitch[i]), 0, 127);
                lastVelocity = e.velocity;
                bend (out, i, midiPitch[i], true);
                if (previous >= 0)
                    add (out, i, 0x80, (uint8_t) previous, 0);
                add (out, i, 0x90, (uint8_t) anchor, (uint8_t) std::clamp ((int) (e.velocity * 127.0f), 1, 127));
                note = anchor;
                articulated = true;
            }
            else if (e.type == VoiceEvent::Off && note >= 0)
            {
                add (out, i, 0x80, (uint8_t) note, 0);
                note = -1;
            }
        }

        if (articulated || i % bendInterval != 0)
            continue;

        const double pitch = midiPitch[i];
        if (note >= 0 && std::abs (pitch - anchor) > bendRange - 0.5)
        {
            // Out of bend range: hand over to a new note, overlapping for legato.
            const int previous = note;
            anchor = std::clamp ((int) std::lround (pitch), 0, 127);
            bend (out, i, pitch, true);
            add (out, i, 0x90, (uint8_t) anchor, (uint8_t) std::clamp ((int) (lastVelocity * 127.0f), 1, 127));
            add (out, i, 0x80, (uint8_t) previous, 0);
            note = anchor;
        }
        else
            bend (out, i, pitch, false);
    }
}

} // namespace gamaka
