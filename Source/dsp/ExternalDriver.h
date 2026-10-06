#pragma once

#include "Synth.h"

#include <vector>

namespace gamaka
{

struct MidiOutEvent
{
    int sample;
    uint8_t data[3];
    int size;
};

// Drives an external (hosted or downstream) instrument with Gamaka's pitch
// curve: one MIDI note per articulation plus a dense pitch-bend stream.
// Legato moves are pure pitch bend; if a gamaka travels further than the bend
// range, the note is re-anchored with an overlapping legato note.
class ExternalDriver
{
public:
    void setBendRange (int semitones) { bendRange = semitones < 1 ? 1 : semitones; }
    int getBendRange() const { return bendRange; }
    void setChannel (int ch1to16) { channel = (uint8_t) ((ch1to16 - 1) & 15); }

    // RPN 0 (pitch-bend sensitivity) so synths that honour it match the range.
    void writeSetup (std::vector<MidiOutEvent>& out, int sample = 0) const;
    // Note-off for whatever is sounding, and reset the bend.
    void writeReset (std::vector<MidiOutEvent>& out, int sample = 0);

    // midiPitch: absolute fractional MIDI pitch for each sample of the block.
    void process (const Synth& synth, const float* midiPitch, int numSamples, std::vector<MidiOutEvent>& out);

    int currentNote() const { return note; }

private:
    void bend (std::vector<MidiOutEvent>& out, int sample, double pitch, bool force);
    void add (std::vector<MidiOutEvent>& out, int sample, uint8_t a, uint8_t b, uint8_t c) const;

    int bendRange = 24;
    uint8_t channel = 0;
    int note = -1;        // sounding note, -1 if none
    int anchor = 60;      // reference note for the bend (kept through release)
    int lastBend = -1;
    float lastVelocity = 0.8f;
    static constexpr int bendInterval = 16; // samples between bend updates
};

} // namespace gamaka
