#include "Sketch.h"

#include <algorithm>
#include <cmath>

namespace gamaka
{

namespace
{
double bez (double p0, double p1, double p2, double p3, double s)
{
    const double u = 1.0 - s;
    return u * u * u * p0 + 3.0 * u * u * s * p1 + 3.0 * u * s * s * p2 + s * s * s * p3;
}
} // namespace

double SketchNote::offsetAt (double t) const
{
    if (curve.empty())
        return 0.0;
    if (t <= curve.front().t)
        return curve.front().offset;
    if (t >= curve.back().t)
        return curve.back().offset;

    size_t i = 0;
    while (i + 2 < curve.size() && curve[i + 1].t <= t)
        ++i;
    const auto& a = curve[i];
    const auto& b = curve[i + 1];
    const double span = b.t - a.t;
    if (span <= 1.0e-9)
        return b.offset;

    // Each handle may reach at most halfway across, which keeps time monotonic.
    const double x1 = std::clamp (a.outDx / span, 0.0, 0.5);
    const double x2 = 1.0 + std::clamp (b.inDx / span, -0.5, 0.0);
    const double target = (t - a.t) / span;
    double lo = 0.0, hi = 1.0;
    for (int it = 0; it < 30; ++it)
    {
        const double mid = 0.5 * (lo + hi);
        (bez (0.0, x1, x2, 1.0, mid) < target ? lo : hi) = mid;
    }
    return bez (a.offset, a.offset + a.outDy, b.offset + b.inDy, b.offset, 0.5 * (lo + hi));
}

void SketchNote::sortCurve()
{
    std::stable_sort (curve.begin(), curve.end(), [] (const CurvePoint& a, const CurvePoint& b) { return a.t < b.t; });
}

void Sketch::sort()
{
    std::stable_sort (notes.begin(), notes.end(),
                      [] (const SketchNote& a, const SketchNote& b) { return a.start < b.start; });
}

int Sketch::noteAt (double beat) const
{
    int found = -1;
    for (int i = 0; i < (int) notes.size(); ++i)
    {
        const auto& n = notes[(size_t) i];
        if (n.start > beat)
            break;
        if (beat < n.end())
            found = i;
    }
    return found;
}

bool Sketch::evaluate (double beat, double& pitch, int& noteIndex) const
{
    noteIndex = beat < lengthBeats ? noteAt (beat) : -1;
    if (noteIndex < 0)
        return false;
    const auto& n = notes[(size_t) noteIndex];
    pitch = n.pitch + n.offsetAt (beat - n.start);
    return true;
}

SketchPlayer::Action SketchPlayer::step (const Sketch& sketch, double beat, double& pitch, float& velocity)
{
    int idx = -1;
    const bool on = sketch.evaluate (beat, pitch, idx);
    const bool changed = idx != lastNote;
    lastNote = idx;
    if (! on)
    {
        if (! sounding)
            return None;
        sounding = false;
        return NoteOff;
    }
    if (! sounding || (changed && ! sketch.notes[(size_t) idx].legato))
    {
        sounding = true;
        velocity = sketch.notes[(size_t) idx].velocity;
        return NoteOn;
    }
    return None;
}

//==============================================================================
void stampGamaka (SketchNote& note, GamakaMode mode, const RagaScale& scale, double bpm, double kampitaRateHz,
                  double depth)
{
    const double up = scale.neighbour (note.pitch, 1) - note.pitch;
    const double lo = scale.neighbour (note.pitch, -1) - note.pitch;
    const double beatsPerSec = std::max (20.0, bpm) / 60.0;
    const double len = note.length;
    const double flick = std::min (0.4 * len, 0.09 * beatsPerSec); // ~90 ms
    const double slide = std::min (0.5 * len, 0.18 * beatsPerSec); // ~180 ms

    auto point = [] (double t, double off, double handle)
    {
        CurvePoint p;
        p.t = t;
        p.offset = off;
        p.inDx = -handle;
        p.outDx = handle;
        return p;
    };

    auto& c = note.curve;
    c.clear();
    switch (mode)
    {
        case GamakaMode::Kampita:
        {
            // Alternate between the note and its upper neighbour, lingering at the turns.
            const double half = 0.5 * beatsPerSec / std::max (0.5, kampitaRateHz);
            const double h = half * 0.45;
            int k = 0;
            for (double t = 0.0; t <= len + 1.0e-9; t += half, ++k)
                c.push_back (point (t, (k % 2) ? up * depth : 0.0, h));
            break;
        }
        case GamakaMode::JaruUp:
            c = { point (0.0, lo, slide * 0.4), point (slide, 0.0, slide * 0.4) };
            break;
        case GamakaMode::JaruDown:
            c = { point (0.0, up, slide * 0.4), point (slide, 0.0, slide * 0.4) };
            break;
        case GamakaMode::Nokku:
            c = { point (0.0, up, 0.0), point (flick * 0.4, up, flick * 0.15), point (flick, 0.0, flick * 0.2) };
            break;
        case GamakaMode::Odukkal:
            c = { point (0.0, lo, 0.0), point (flick * 0.4, lo, flick * 0.15), point (flick, 0.0, flick * 0.2) };
            break;
        case GamakaMode::Sphurita:
        case GamakaMode::Pratyahata:
        {
            const double target = mode == GamakaMode::Sphurita ? lo : up;
            const double m = 0.5 * len;
            c = { point (m - flick, 0.0, flick * 0.3), point (m, target, flick * 0.3), point (m + flick, 0.0, flick * 0.3) };
            break;
        }
        case GamakaMode::Orikkai:
            c = { point (std::max (0.0, len - 2.0 * flick), 0.0, flick * 0.4), point (len, up, flick * 0.4) };
            break;
        default:
            break;
    }
}

} // namespace gamaka
