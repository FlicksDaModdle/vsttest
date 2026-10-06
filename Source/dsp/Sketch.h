#pragma once

#include "Raga.h"
#include "Synth.h"

#include <vector>

namespace gamaka
{

// A bezier control point on a note's pitch curve. Times are in beats from the
// note start; offsets are semitones relative to the note's own pitch.
struct CurvePoint
{
    double t = 0.0;
    double offset = 0.0;
    double inDx = -0.08, inDy = 0.0;  // handle towards the previous point
    double outDx = 0.08, outDy = 0.0; // handle towards the next point
};

struct SketchNote
{
    double start = 0.0;   // beats
    double length = 1.0;  // beats
    double pitch = 0.0;   // semitones relative to middle Sa
    float velocity = 0.8f;
    bool legato = false;  // continue the previous note's sound instead of re-plucking
    std::vector<CurvePoint> curve; // sorted by t; empty = steady pitch

    double end() const { return start + length; }
    // Pitch offset at time t (beats from the note start). Before the first
    // point and after the last one the curve holds that point's value.
    double offsetAt (double t) const;
    void sortCurve();
};

struct Sketch
{
    std::vector<SketchNote> notes; // sorted by start
    double lengthBeats = 16.0;

    void sort();
    // The note sounding at `beat` (the latest-starting one if notes overlap), or -1.
    int noteAt (double beat) const;
    bool evaluate (double beat, double& pitch, int& noteIndex) const;
};

// Fills a note's curve with a gamaka shape drawn from the raga's neighbours.
// rateHz/tempo set the kampita speed; depth 1 reaches the neighbouring note.
void stampGamaka (SketchNote& note, GamakaMode mode, const RagaScale& scale, double bpm, double kampitaRateHz = 5.5,
                  double depth = 1.0);

// Walks a sketch in time and reports where notes start and stop.
struct SketchPlayer
{
    enum Action { None, NoteOn, NoteOff };

    void reset() { lastNote = -2; sounding = false; }
    // Call with increasing beats. Writes the pitch whenever a note sounds.
    Action step (const Sketch& sketch, double beat, double& pitch, float& velocity);
    bool isSounding() const { return sounding; }

private:
    int lastNote = -2;
    bool sounding = false;
};

} // namespace gamaka
