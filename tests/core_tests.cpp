// Standalone tests for the Gamaka DSP core (no JUCE needed).
// Usage: gamaka_tests [output-dir-for-demo-wavs]

#include "../Source/dsp/Sketch.h"
#include "../Source/dsp/ExternalDriver.h"
#include "../Source/dsp/Synth.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <set>
#include <string>
#include <vector>

using namespace gamaka;

static int failures = 0;
#define CHECK(cond, ...)                                                   \
    do                                                                     \
    {                                                                      \
        if (! (cond))                                                      \
        {                                                                  \
            ++failures;                                                    \
            std::printf ("FAIL %s:%d  %s  ", __FILE__, __LINE__, #cond);   \
            std::printf (__VA_ARGS__);                                     \
            std::printf ("\n");                                            \
        }                                                                  \
    } while (0)

static bool near (double a, double b, double tol) { return std::abs (a - b) <= tol; }

constexpr double fs = 48000.0;

struct Render
{
    std::vector<float> audio, pitch;
    double at (double seconds) const { return pitch[std::min (pitch.size() - 1, (size_t) (seconds * fs))]; }
    double minPitch (double t0, double t1) const
    {
        double m = 1e9;
        for (size_t i = (size_t) (t0 * fs); i < (size_t) (t1 * fs); ++i)
            m = std::min (m, (double) pitch[i]);
        return m;
    }
    double maxPitch (double t0, double t1) const
    {
        double m = -1e9;
        for (size_t i = (size_t) (t0 * fs); i < (size_t) (t1 * fs); ++i)
            m = std::max (m, (double) pitch[i]);
        return m;
    }
};

// A tiny sequencer: events are (time, action).
struct Seq
{
    std::vector<std::pair<double, std::function<void (Synth&)>>> events;
    void on (double t, int note, int vel = 100) { events.push_back ({ t, [=] (Synth& s) { s.noteOn (note, vel); } }); }
    void off (double t, int note) { events.push_back ({ t, [=] (Synth& s) { s.noteOff (note); } }); }
    void at (double t, std::function<void (Synth&)> f) { events.push_back ({ t, std::move (f) }); }
};

static Render run (const SynthParams& p, Seq seq, double seconds)
{
    Synth synth;
    synth.prepare (fs);
    synth.setParams (p);
    std::stable_sort (seq.events.begin(), seq.events.end(),
                      [] (auto& a, auto& b) { return a.first < b.first; });
    Render r;
    const size_t total = (size_t) (seconds * fs);
    r.audio.resize (total);
    r.pitch.resize (total);
    size_t pos = 0, ev = 0;
    while (pos < total)
    {
        while (ev < seq.events.size() && (size_t) (seq.events[ev].first * fs) <= pos)
            seq.events[ev++].second (synth);
        size_t end = total;
        if (ev < seq.events.size())
            end = std::min (end, std::max (pos + 1, (size_t) (seq.events[ev].first * fs)));
        end = std::min (end, pos + 256);
        synth.render (r.audio.data() + pos, (int) (end - pos), r.pitch.data() + pos);
        pos = end;
    }
    return r;
}

// Fundamental estimate by normalised autocorrelation over a window.
static double estimateF0 (const std::vector<float>& x, double t0, double len, double fmin, double fmax)
{
    const size_t s = (size_t) (t0 * fs), n = (size_t) (len * fs);
    const int lagMin = (int) (fs / fmax), lagMax = (int) (fs / fmin);
    std::vector<double> c ((size_t) lagMax + 2, 0.0);
    for (int lag = lagMin - 1; lag <= lagMax + 1; ++lag)
    {
        double num = 0, e0 = 0, e1 = 0;
        for (size_t i = 0; i < n; ++i)
        {
            num += x[s + i] * x[s + i + (size_t) lag];
            e0 += x[s + i] * x[s + i];
            e1 += x[s + i + (size_t) lag] * x[s + i + (size_t) lag];
        }
        c[(size_t) lag] = num / std::sqrt (e0 * e1 + 1e-20);
    }
    int best = lagMin;
    for (int lag = lagMin; lag <= lagMax; ++lag)
        if (c[(size_t) lag] > c[(size_t) best])
            best = lag;
    // parabolic interpolation
    const double a = c[(size_t) best - 1], b = c[(size_t) best], d = c[(size_t) best + 1];
    const double shift = 0.5 * (a - d) / (a - 2 * b + d);
    return fs / (best + shift);
}

static void writeWav (const std::string& path, const std::vector<float>& x)
{
    std::ofstream f (path, std::ios::binary);
    auto u32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto u16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    f.write ("RIFF", 4);
    u32 (36 + (uint32_t) x.size() * 2);
    f.write ("WAVEfmt ", 8);
    u32 (16); u16 (1); u16 (1); u32 ((uint32_t) fs); u32 ((uint32_t) fs * 2); u16 (2); u16 (16);
    f.write ("data", 4);
    u32 ((uint32_t) x.size() * 2);
    for (float v : x)
    {
        const int16_t s = (int16_t) std::lround (std::max (-1.0f, std::min (1.0f, v)) * 32767.0f);
        f.write ((const char*) &s, 2);
    }
}

static void checkAudioSane (const Render& r, const char* what)
{
    float peak = 0;
    bool finite = true;
    for (float v : r.audio)
    {
        finite = finite && std::isfinite (v);
        peak = std::max (peak, std::abs (v));
    }
    CHECK (finite, "%s produced non-finite samples", what);
    CHECK (peak > 0.05f && peak < 0.999f, "%s peak %.3f", what, peak);
}

//==============================================================================
static void testRagas()
{
    CHECK (allRagas().size() > 72, "ragas %zu", allRagas().size());
    std::set<std::vector<int>> distinct;
    for (int i = 0; i < 72; ++i)
    {
        std::vector<int> semis;
        for (auto s : allRagas()[(size_t) i].swaras)
            semis.push_back (swaraSemitone (s));
        CHECK (semis.size() == 7, "mela %d size", i + 1);
        for (size_t k = 1; k < semis.size(); ++k)
            CHECK (semis[k] > semis[k - 1], "mela %d not ascending", i + 1);
        distinct.insert (semis);
    }
    CHECK (distinct.size() == 72, "melakartas are not all distinct (%zu)", distinct.size());

    auto expectScale = [] (int mela, std::vector<double> semis)
    {
        RagaScale s;
        s.set (mela - 1, Tuning::Equal);
        for (size_t k = 0; k < semis.size(); ++k)
            CHECK (near (s.degreeToPitch ((int) k), semis[k], 1e-9), "mela %d degree %zu", mela, k);
    };
    expectScale (29, { 0, 2, 4, 5, 7, 9, 11, 12 });  // Sankarabharanam
    expectScale (15, { 0, 1, 4, 5, 7, 8, 11, 12 });  // Mayamalavagowla
    expectScale (65, { 0, 2, 4, 6, 7, 9, 11, 12 });  // Kalyani
    expectScale (8, { 0, 1, 3, 5, 7, 8, 10 });       // Todi
    expectScale (1, { 0, 1, 2, 5, 7, 8, 9 });        // Kanakangi

    RagaScale s;
    s.set (28, Tuning::Equal);
    CHECK (near (s.neighbour (4, 1), 5, 1e-9), "G3 upper");
    CHECK (near (s.neighbour (4, -1), 2, 1e-9), "G3 lower");
    CHECK (near (s.neighbour (0, -1), -1, 1e-9), "S lower = .N3");
    CHECK (s.nearestDegree (-1.0) == -1, "nearest .N3");
    CHECK (s.degreeLabel (-1) == ".N3" && s.degreeLabel (7) == "S'", "labels %s %s",
           s.degreeLabel (-1).c_str(), s.degreeLabel (7).c_str());
    CHECK (near (s.degreeFToPitch (s.pitchToDegreeF (3.3)), 3.3, 1e-9), "degree round-trip");
    CHECK (near (s.pitchToDegreeF (4.5), 2.5, 1e-9), "G3..M1 midpoint");

    // Janya: Mohanam (S R2 G3 P D2) — Ma key snaps to G3, Ni key to upper Sa.
    int mohanam = -1;
    for (int i = 0; i < (int) allRagas().size(); ++i)
        if (allRagas()[(size_t) i].name.rfind ("Mohanam", 0) == 0)
            mohanam = i;
    CHECK (mohanam >= 72, "Mohanam missing");
    s.set (mohanam, Tuning::Equal);
    CHECK (s.notesPerOctave() == 5, "mohanam size");
    CHECK (near (s.neighbour (4, 1), 7, 1e-9), "mohanam G3 upper = P");
    CHECK (near (s.letterPitch (3, 0), 4, 1e-9), "mohanam Ma key");
    CHECK (near (s.letterPitch (6, 0), 12, 1e-9), "mohanam Ni key");

    // Just intonation: Pa is a pure fifth.
    s.set (28, Tuning::Just);
    CHECK (near (s.degreeToPitch (4), 12.0 * std::log2 (1.5), 1e-9), "just Pa");
    CHECK (near (tunedSemitone (-5, Tuning::Just), 12.0 * std::log2 (0.75), 1e-9), "just mandra Pa");
}

static void testKeyMapping()
{
    SynthParams p;
    p.raga = 14; // 15 Mayamalavagowla
    Synth synth;
    synth.prepare (fs);
    synth.setParams (p);
    double pitch = 0;
    CHECK (synth.keyToPitch (60, pitch) && near (pitch, 0, 1e-9), "C4 = S");
    CHECK (synth.keyToPitch (62, pitch) && near (pitch, 1, 1e-9), "D4 = R1 in Mayamalavagowla (%f)", pitch);
    CHECK (synth.keyToPitch (69, pitch) && near (pitch, 8, 1e-9), "A4 = D1");
    CHECK (synth.keyToPitch (71 - 12, pitch) && near (pitch, -1, 1e-9), "B3 = mandra N3");
    CHECK (synth.keyToPitch (61, pitch) && near (pitch, 1, 1e-9), "black key chromatic");
    CHECK (! synth.keyToPitch (25, pitch), "keyswitch region is not a note");

    p.keyMode = KeyMode::ChromaticSnap;
    p.saMidiNote = 62; // Sa = D
    synth.setParams (p);
    CHECK (synth.keyToPitch (62 + 3, pitch) && (near (pitch, 4, 1e-9) || near (pitch, 1, 1e-9)), "snap %f", pitch);
    CHECK (synth.keyToPitch (62 + 7, pitch) && near (pitch, 7, 1e-9), "snap Pa");
}

static SynthParams baseParams()
{
    SynthParams p;
    p.raga = 28; // Sankarabharanam
    p.engine = EngineType::Violin;
    p.smoothing = 0.004f;
    return p;
}

static void testKampita()
{
    auto p = baseParams();
    p.gamaka = GamakaMode::Kampita;
    p.kampitaDepth = 1.0f;
    p.kampitaRate = 5.0f;
    Seq seq;
    seq.on (0.0, 64); // G3
    seq.off (2.0, 64);
    auto r = run (p, seq, 2.2);
    CHECK (near (r.at (0.0005), 4.0, 0.05), "kampita starts on the note %f", r.at (0.0005));
    CHECK (near (r.minPitch (0.5, 1.5), 4.0, 0.08), "kampita low %f", r.minPitch (0.5, 1.5));
    CHECK (near (r.maxPitch (0.5, 1.5), 5.0, 0.08), "kampita reaches M1 %f", r.maxPitch (0.5, 1.5));

    // Count oscillations: rising crossings of the midpoint in one second ~ rate.
    int crossings = 0;
    for (size_t i = (size_t) (0.5 * fs); i < (size_t) (1.5 * fs); ++i)
        if (r.pitch[i - 1] < 4.5 && r.pitch[i] >= 4.5)
            ++crossings;
    CHECK (crossings == 5, "kampita rate: %d cycles", crossings);
    checkAudioSane (r, "kampita");

    // Finite cycles stop on the note.
    p.kampitaCycles = 2;
    auto r2 = run (p, seq, 2.2);
    CHECK (near (r2.minPitch (0.6, 1.5), 4.0, 0.02) && near (r2.maxPitch (0.6, 1.5), 4.0, 0.02),
           "kampita stops after 2 cycles");

    // Down direction oscillates towards R2.
    p.kampitaCycles = 0;
    p.kampitaDir = KampitaDirection::Down;
    auto r3 = run (p, seq, 2.2);
    CHECK (near (r3.minPitch (0.5, 1.5), 2.0, 0.08) && near (r3.maxPitch (0.5, 1.5), 4.0, 0.08), "kampita down");

    // In Mohanam the upper neighbour of G3 is Pa — the oscillation is raga-aware.
    p.kampitaDir = KampitaDirection::Up;
    p.raga = 72; // Mohanam (first janya)
    auto r4 = run (p, seq, 2.2);
    CHECK (near (r4.maxPitch (0.5, 1.5), 7.0, 0.1), "mohanam kampita goes to Pa %f", r4.maxPitch (0.5, 1.5));
}

static void testModWheelKampita()
{
    auto p = baseParams();
    Seq seq;
    seq.on (0.0, 67); // Pa
    seq.at (1.0, [] (Synth& s) { const uint8_t cc[] = { 0xb0, 1, 127 }; s.handleMidi (cc, 3); });
    seq.off (2.5, 67);
    auto r = run (p, seq, 2.6);
    CHECK (near (r.minPitch (0.2, 0.95), 7, 0.01) && near (r.maxPitch (0.2, 0.95), 7, 0.01), "steady before wheel");
    CHECK (near (r.maxPitch (1.2, 2.4), 9, 0.1), "mod wheel kampita reaches D2 %f", r.maxPitch (1.2, 2.4));
}

static void testJaru()
{
    auto p = baseParams();
    p.gamaka = GamakaMode::JaruUp;
    p.jaruTime = 0.2f;
    Seq seq;
    seq.on (0.0, 67); // Pa
    seq.off (1.0, 67);
    auto r = run (p, seq, 1.1);
    CHECK (near (r.at (0.001), 5.0, 0.1), "jaru up starts on M1 %f", r.at (0.001));
    CHECK (r.at (0.1) > 5.3 && r.at (0.1) < 6.8, "jaru mid %f", r.at (0.1));
    CHECK (near (r.at (0.3), 7.0, 0.02), "jaru lands on P %f", r.at (0.3));
    // monotonic rise
    bool mono = true;
    for (size_t i = 1; i < (size_t) (0.3 * fs); ++i)
        mono = mono && r.pitch[i] >= r.pitch[i - 1] - 1e-6;
    CHECK (mono, "jaru up is monotonic");

    p.gamaka = GamakaMode::JaruDown;
    p.jaruSteps = 2;
    auto r2 = run (p, seq, 1.1);
    CHECK (near (r2.at (0.001), 11.0, 0.1), "jaru down from 2 steps above (N3) %f", r2.at (0.001));
    CHECK (near (r2.at (0.3), 7.0, 0.02), "jaru down lands %f", r2.at (0.3));
}

static void testFlicks()
{
    auto p = baseParams();
    p.flickTime = 0.1f;
    Seq seq;
    seq.on (0.0, 64);
    seq.off (0.8, 64);

    p.gamaka = GamakaMode::Nokku;
    auto n = run (p, seq, 1.0);
    CHECK (near (n.at (0.001), 5.0, 0.1), "nokku starts above %f", n.at (0.001));
    CHECK (near (n.at (0.2), 4.0, 0.02), "nokku lands");

    p.gamaka = GamakaMode::Odukkal;
    auto o = run (p, seq, 1.0);
    CHECK (near (o.at (0.001), 2.0, 0.1), "odukkal starts below %f", o.at (0.001));

    p.gamaka = GamakaMode::Sphurita;
    p.onsetDelay = 0.2f;
    auto s = run (p, seq, 1.0);
    CHECK (near (s.at (0.1), 4.0, 0.02), "sphurita holds first");
    CHECK (near (s.minPitch (0.2, 0.35), 2.0, 0.15), "sphurita dips to R2 %f", s.minPitch (0.2, 0.35));
    CHECK (near (s.at (0.5), 4.0, 0.02), "sphurita returns");

    p.gamaka = GamakaMode::Pratyahata;
    auto pr = run (p, seq, 1.0);
    CHECK (near (pr.maxPitch (0.2, 0.35), 5.0, 0.15), "pratyahata touches M1");

    p.gamaka = GamakaMode::Orikkai;
    p.onsetDelay = 0.0f;
    p.voice.release = 0.4f;
    auto ok = run (p, seq, 1.2);
    CHECK (near (ok.at (0.7), 4.0, 0.02), "orikkai steady while held");
    CHECK (near (ok.at (0.95), 5.0, 0.05), "orikkai flicks up on release %f", ok.at (0.95));
}

static void testLegatoGlide()
{
    auto p = baseParams();
    p.glideTime = 0.15f;
    Seq seq;
    seq.on (0.0, 60);
    seq.on (0.5, 67);   // legato: S -> P
    seq.off (0.6, 60);
    seq.off (1.0, 67);
    seq.on (1.5, 64);   // detached: no glide
    seq.off (2.0, 64);
    auto r = run (p, seq, 2.5);
    CHECK (near (r.at (0.45), 0.0, 0.01), "on Sa");
    const double mid = r.at (0.575);
    CHECK (mid > 2.0 && mid < 5.0, "gliding %f", mid);
    CHECK (near (r.at (0.7), 7.0, 0.02), "arrived at Pa");
    bool mono = true;
    for (size_t i = (size_t) (0.5 * fs); i < (size_t) (0.7 * fs); ++i)
        mono = mono && r.pitch[i] >= r.pitch[i - 1] - 1e-6;
    CHECK (mono, "glide monotonic");
    CHECK (near (r.at (1.52), 4.0, 0.02), "detached note jumps (no glide) %f", r.at (1.52));

    // Trill: hold Sa, tap Ri — releasing Ri glides back to Sa.
    Seq trill;
    trill.on (0.0, 60);
    trill.on (0.3, 62);
    trill.off (0.5, 62);
    trill.off (1.0, 60);
    auto t = run (p, trill, 1.2);
    CHECK (near (t.at (0.49), 2.0, 0.02) && near (t.at (0.8), 0.0, 0.02), "falls back to held note");
}

static void testKeyswitchAndBend()
{
    auto p = baseParams();
    Seq seq;
    seq.on (0.0, p.keyswitchBase + (int) GamakaMode::JaruUp);
    seq.off (0.01, p.keyswitchBase + (int) GamakaMode::JaruUp);
    seq.on (0.1, 67);
    seq.off (1.0, 67);
    auto r = run (p, seq, 1.1);
    CHECK (near (r.at (0.102), 5.0, 0.15), "keyswitch selected jaru %f", r.at (0.102));

    // Raga-step bend: full bend up from G3 with range 2 lands on Pa.
    p.bendRange = 2;
    p.bendMode = BendMode::RagaSteps;
    Seq b;
    b.on (0.0, 64);
    b.at (0.2, [] (Synth& s) { const uint8_t pb[] = { 0xe0, 0x7f, 0x7f }; s.handleMidi (pb, 3); });
    b.off (0.6, 64);
    auto rb = run (p, b, 0.7);
    CHECK (near (rb.at (0.5), 7.0, 0.01), "raga bend to P %f", rb.at (0.5));
}

static void testTuning()
{
    for (auto eng : { EngineType::Veena, EngineType::Violin, EngineType::Venu })
        for (int note : { 48, 60, 67, 79 })
        {
            auto p = baseParams();
            p.engine = eng;
            p.voice.decay = 6.0f;
            Seq seq;
            seq.on (0.0, note);
            seq.off (1.5, note);
            auto r = run (p, seq, 1.6);
            const double expected = 261.6256 * std::exp2 (r.at (0.5) / 12.0);
            const double f0 = estimateF0 (r.audio, 0.4, 0.1, expected / 1.5, expected * 1.5);
            const double cents = 1200.0 * std::log2 (f0 / expected);
            CHECK (std::abs (cents) < 5.0, "engine %d note %d: %.2f Hz vs %.2f Hz (%.1f cents)", (int) eng, note, f0,
                   expected, cents);
            checkAudioSane (r, "tuning");
        }
}

static void testVeenaSlideKeepsRinging()
{
    // A veena should hold its pluck through a legato slide (no re-pluck with legatoPluck = 0).
    auto p = baseParams();
    p.engine = EngineType::Veena;
    p.voice.legatoPluck = 0.0f;
    p.voice.decay = 8.0f;
    p.glideTime = 0.25f;
    Seq seq;
    seq.on (0.0, 60);
    seq.on (0.5, 67);
    seq.off (0.55, 60);
    seq.off (1.5, 67);
    auto r = run (p, seq, 1.6);
    const double f0 = estimateF0 (r.audio, 1.0, 0.08, 300, 500);
    const double expected = 261.6256 * std::exp2 (7.0 / 12.0);
    CHECK (std::abs (1200.0 * std::log2 (f0 / expected)) < 8.0, "string slid to Pa: %.2f vs %.2f", f0, expected);
    float rmsAfter = 0;
    for (size_t i = (size_t) (1.0 * fs); i < (size_t) (1.1 * fs); ++i)
        rmsAfter += r.audio[i] * r.audio[i];
    CHECK (rmsAfter > 1.0f, "still ringing after slide (%f)", rmsAfter);
}

static void testTambura()
{
    auto p = baseParams();
    p.tamburaLevel = 0.8f;
    auto r = run (p, Seq {}, 4.0);
    checkAudioSane (r, "tambura");
}

static void testExternalDriver()
{
    // Gamaka drives an external synth: reconstruct the pitch that synth would
    // play from the MIDI stream and compare with Gamaka's own pitch curve.
    for (int range : { 24, 2 })
    {
        auto p = baseParams();
        p.gamaka = GamakaMode::Kampita;
        p.glideTime = 0.2f;
        Synth synth;
        synth.prepare (fs);
        synth.setParams (p);
        synth.setEngineMuted (true);
        ExternalDriver drv;
        drv.setBendRange (range);
        std::vector<MidiOutEvent> midi;

        const double saMidi = 60.0;
        int noteOns = 0, sounding = -1, bendValue = 8192;
        double maxErr = 0.0;
        std::vector<float> pitch (256), midiPitch (256);
        for (int block = 0; block < (int) (2.0 * fs / 256); ++block)
        {
            synth.beginBlock();
            const double t = block * 256 / fs;
            if (block == (int) (0.1 * fs / 256)) synth.noteOn (60, 100);
            if (block == (int) (0.6 * fs / 256)) synth.noteOn (72, 100); // legato octave slide
            if (block == (int) (0.7 * fs / 256)) synth.noteOff (60);
            if (block == (int) (1.5 * fs / 256)) synth.noteOff (72);
            std::vector<float> audio (256);
            synth.render (audio.data(), 256, pitch.data());
            for (int i = 0; i < 256; ++i)
                midiPitch[(size_t) i] = (float) (saMidi + pitch[(size_t) i]);
            midi.clear();
            drv.process (synth, midiPitch.data(), 256, midi);
            size_t m = 0;
            for (int i = 0; i < 256; ++i)
            {
                for (; m < midi.size() && midi[m].sample <= i; ++m)
                {
                    const auto& e = midi[m];
                    if ((e.data[0] & 0xf0) == 0x90) { ++noteOns; sounding = e.data[1]; }
                    if ((e.data[0] & 0xf0) == 0x80 && e.data[1] == sounding) sounding = -1;
                    if ((e.data[0] & 0xf0) == 0xe0) bendValue = e.data[1] | (e.data[2] << 7);
                }
                if (sounding >= 0 && t > 0.15 && t < 1.45)
                {
                    const double played = sounding + (bendValue - 8192) / 8191.0 * range;
                    maxErr = std::max (maxErr, std::abs (played - midiPitch[(size_t) i]));
                }
            }
        }
        CHECK (maxErr < 0.06, "range %d: external pitch error %.3f semitones", range, maxErr);
        if (range == 24)
            CHECK (noteOns == 1, "legato slide uses pure bend (%d note-ons)", noteOns);
        else
            CHECK (noteOns > 1, "small bend range re-anchors (%d note-ons)", noteOns);
        CHECK (sounding == -1, "external note released");
    }
}

static void testSketch()
{
    RagaScale scale;
    scale.set (28, Tuning::Equal); // Sankarabharanam
    Sketch sk;
    sk.lengthBeats = 4.0;
    SketchNote a; a.start = 0.0; a.length = 1.0; a.pitch = 0.0;
    a.curve = { { 0.0, 0.0, -0.1, 0, 0.25, 0 }, { 1.0, 4.0, -0.25, 0, 0.1, 0 } }; // S sliding to G3
    SketchNote b; b.start = 1.0; b.length = 1.0; b.pitch = 4.0; b.legato = true; // continues on G3
    SketchNote c; c.start = 2.5; c.length = 1.0; c.pitch = 7.0;                 // re-plucked P after a gap
    sk.notes = { c, a, b };
    sk.sort();
    CHECK (sk.notes[0].start == 0.0 && sk.notes[2].start == 2.5, "sorted");

    double p = 0; int idx = 0;
    CHECK (sk.evaluate (0.5, p, idx) && near (p, 2.0, 1e-6) && idx == 0, "S-curve midpoint %f", p);
    CHECK (sk.evaluate (0.05, p, idx) && p < 0.2, "eases out %f", p);
    bool mono = true; double prev = -1;
    for (int i = 0; i < 100; ++i) { sk.evaluate (i / 100.0, p, idx); mono = mono && p >= prev - 1e-9; prev = p; }
    CHECK (mono, "monotonic bezier slide");
    CHECK (! sk.evaluate (2.2, p, idx), "gap is silent");
    CHECK (! sk.evaluate (4.2, p, idx), "silent past the end");

    SketchPlayer player;
    int ons = 0, offs = 0; float vel = 0;
    for (int i = 0; i < 4000; ++i)
    {
        const auto act = player.step (sk, i / 1000.0, p, vel);
        ons += act == SketchPlayer::NoteOn;
        offs += act == SketchPlayer::NoteOff;
    }
    CHECK (ons == 2 && offs == 2, "legato note doesn't re-pluck: ons %d offs %d", ons, offs);

    // Stamped gamakas use the raga's neighbours.
    SketchNote g; g.pitch = 4.0; g.length = 2.0;
    stampGamaka (g, GamakaMode::Kampita, scale, 120.0, 5.0, 1.0);
    double mx = -9, mn = 9;
    for (int i = 0; i <= 200; ++i) { const double o = g.offsetAt (i / 100.0); mx = std::max (mx, o); mn = std::min (mn, o); }
    CHECK (near (mx, 1.0, 1e-6) && near (mn, 0.0, 1e-6), "kampita stamp G3<->M1 %f %f", mn, mx);
    stampGamaka (g, GamakaMode::JaruUp, scale, 120.0);
    CHECK (near (g.offsetAt (0.0), -2.0, 1e-9) && near (g.offsetAt (1.0), 0.0, 1e-9), "jaru stamp from R2");
    stampGamaka (g, GamakaMode::Orikkai, scale, 120.0);
    CHECK (near (g.offsetAt (2.0), 1.0, 1e-9) && near (g.offsetAt (0.5), 0.0, 1e-9), "orikkai stamp");

    // Render the sketch through the synth and check the slide is audible at the right pitch.
    Synth synth;
    synth.prepare (fs);
    auto sp = baseParams();
    sp.engine = EngineType::Veena;
    synth.setParams (sp);
    player.reset();
    const double bpm = 120.0;
    std::vector<float> audio ((size_t) (2.0 * fs)), pitchIn (audio.size()), pitchOut (audio.size());
    for (size_t i = 0; i < audio.size(); ++i)
    {
        double pp = 0;
        const auto act = player.step (sk, i / fs * bpm / 60.0, pp, vel);
        if (act == SketchPlayer::NoteOn) synth.patternNoteOn (vel);
        if (act == SketchPlayer::NoteOff) synth.patternNoteOff();
        pitchIn[i] = (float) pp;
        synth.render (&audio[i], 1, &pitchOut[i], &pitchIn[i]);
    }
    const double f0 = estimateF0 (audio, 0.7, 0.05, 250, 400); // beats 1.4..1.5: G3, legato
    CHECK (std::abs (1200 * std::log2 (f0 / (261.6256 * std::exp2 (4.0 / 12.0)))) < 10, "rendered G3 %f", f0);
    checkAudioSane (Render { audio, pitchOut }, "sketch render");
}

//==============================================================================
static void renderDemos (const std::string& dir)
{
    // A short Kalyani phrase on each engine: S R G (kampita) M (jaru up) P (nokku) D N S' with slides.
    for (auto eng : { EngineType::Veena, EngineType::Violin, EngineType::Venu })
    {
        SynthParams p;
        p.raga = 64; // 65 Mechakalyani
        p.engine = eng;
        p.saMidiNote = 61;
        p.tamburaLevel = 0.35f;
        const int ks = p.keyswitchBase;
        Seq s;
        double t = 0.5;
        auto note = [&] (int key, double len, GamakaMode m, bool legato = false)
        {
            s.on (t - 0.01, ks + (int) m);
            s.off (t - 0.005, ks + (int) m);
            s.on (t, key);
            s.off (t + len + (legato ? 0.05 : -0.03), key);
            t += len;
        };
        note (60, 0.5, GamakaMode::Plain);
        note (62, 0.5, GamakaMode::Odukkal);
        note (64, 1.0, GamakaMode::Kampita);
        note (66, 0.6, GamakaMode::JaruUp);
        note (67, 0.8, GamakaMode::Nokku);
        note (69, 0.5, GamakaMode::Plain, true);
        note (71, 0.5, GamakaMode::Pratyahata, true);
        note (72, 1.2, GamakaMode::Kampita);
        note (71, 0.4, GamakaMode::JaruDown);
        note (67, 1.0, GamakaMode::Orikkai);
        t += 0.4;
        note (64, 0.6, GamakaMode::Sphurita);
        note (62, 0.4, GamakaMode::Plain, true);
        note (60, 2.0, GamakaMode::Plain);
        auto r = run (p, s, t + 2.0);
        static const char* names[] = { "veena", "violin", "venu" };
        writeWav (dir + "/demo_kalyani_" + names[(int) eng] + ".wav", r.audio);
    }
}

int main (int argc, char** argv)
{
    testRagas();
    testKeyMapping();
    testKampita();
    testModWheelKampita();
    testJaru();
    testFlicks();
    testLegatoGlide();
    testKeyswitchAndBend();
    testTuning();
    testVeenaSlideKeepsRinging();
    testTambura();
    testExternalDriver();
    testSketch();

    if (argc > 1)
        renderDemos (argv[1]);

    if (failures == 0)
        std::printf ("All tests passed.\n");
    else
        std::printf ("%d check(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
