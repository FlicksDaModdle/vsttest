#pragma once

#include "Raga.h"
#include "Voices.h"

#include <array>
#include <cstdint>
#include <memory>

namespace gamaka
{

enum class GamakaMode
{
    Plain = 0,
    Kampita,    // oscillation between the note and its raga neighbour
    JaruUp,     // Etra jaru: slide up into the note from below
    JaruDown,   // Irakka jaru: slide down into the note from above
    Nokku,      // stress from the upper note: quick push down onto the note
    Odukkal,    // quick push up onto the note from the lower note
    Sphurita,   // dip to the lower note and back (re-articulates a held note)
    Pratyahata, // flick to the upper note and back
    Orikkai,    // flick up to the upper note as the note is released
    Count
};

const char* gamakaName (GamakaMode m);
const char* gamakaDescription (GamakaMode m);

enum class KeyMode { Swara = 0, ChromaticSnap, ChromaticFree };
enum class GlideMode { Off = 0, Legato, Always };
enum class KampitaDirection { Up = 0, Down, Around };
enum class BendMode { Semitones = 0, RagaSteps };
enum class EngineType { Veena = 0, Violin, Venu };

struct SynthParams
{
    // Raga & tuning
    int raga = 28;              // index into allRagas() (28 = 29 Dheerasankarabharanam)
    Tuning tuning = Tuning::Equal;
    int saMidiNote = 60;        // the pitch of middle Sa
    float saFineCents = 0.0f;
    float a4Hz = 440.0f;
    KeyMode keyMode = KeyMode::Swara;

    // Gamaka selection
    GamakaMode gamaka = GamakaMode::Plain; // the default; keyswitches override it
    bool keyswitches = true;
    int keyswitchBase = 24;     // MIDI note for Plain; the next 8 notes select the others

    // Kampita
    float kampitaRate = 5.5f;   // Hz
    bool kampitaSync = false;
    float kampitaSyncBeats = 0.5f; // cycle length in beats when synced
    float kampitaDepth = 0.85f; // 1 = reaches the neighbouring raga note
    float kampitaDwell = 0.5f;  // 0 = sine, 1 = lingers on each end
    float kampitaTilt = 0.5f;   // fraction of each cycle spent rising
    int kampitaCycles = 0;      // 0 = keep going
    KampitaDirection kampitaDir = KampitaDirection::Up;

    // Slides & flicks
    float jaruTime = 0.18f;     // s
    int jaruSteps = 1;          // raga notes away the jaru starts from
    float flickTime = 0.09f;    // s, nokku / odukkal / sphurita / pratyahata / orikkai
    float onsetDelay = 0.0f;    // s, hold before the gamaka begins

    GlideMode glideMode = GlideMode::Legato;
    float glideTime = 0.12f;    // s
    float slideShape = 0.5f;    // 0 linear, 0.5 S-curve, 1 fast-then-settle
    float smoothing = 0.010f;   // s, final pitch smoothing

    // Controllers
    float modWheelDepth = 1.0f; // kampita depth at full mod wheel / aftertouch
    float bendRange = 2.0f;     // semitones or raga steps
    BendMode bendMode = BendMode::RagaSteps;

    // Sound
    EngineType engine = EngineType::Veena;
    VoiceParams voice;
    float velocitySens = 0.7f;
    float tamburaLevel = 0.0f;
    int tamburaString = 0;      // 0 = Pa, 1 = Ma, 2 = Ni
    float gain = 0.8f;
};

// An articulation the synth performed, for driving an external instrument.
struct VoiceEvent
{
    enum Type { On, Off } type;
    int sample;      // offset within the current render block
    float velocity;
    bool legato;
};

class Synth
{
public:
    Synth();

    void prepare (double sampleRate);
    void reset();
    void setParams (const SynthParams& p);
    void setTempo (double bpm) { tempoBpm = bpm > 0.0 ? bpm : 120.0; }

    // Raw MIDI (status byte first).
    void handleMidi (const uint8_t* data, int size);
    void noteOn (int note, int velocity);
    void noteOff (int note);
    void allNotesOff();

    // Renders mono output. pitchOut, if non-null, receives the pitch in
    // semitones relative to middle Sa for each sample.
    // pitchIn, if non-null, overrides the gamaka engine's pitch (pattern playback).
    void render (float* out, int numSamples, float* pitchOut = nullptr, const float* pitchIn = nullptr);

    // Articulations for pattern playback, which drives pitch via render's pitchIn.
    void patternNoteOn (float velocity);
    void patternNoteOff();

    // When muted, the internal engine is silent but pitch and articulation
    // still run, so an external instrument can be driven instead.
    void setEngineMuted (bool shouldMute) { engineMuted = shouldMute; }
    double expressionLevel() const;

    // Articulations since beginBlock(); offsets are relative to the samples
    // rendered since then.
    void beginBlock() { numEvents = 0; blockPos = 0; }
    int getNumEvents() const { return numEvents; }
    const VoiceEvent& getEvent (int i) const { return events[(size_t) i]; }

    // Live state, for the UI.
    GamakaMode activeGamaka() const { return activeMode; }
    double currentPitch() const { return smoothed2; } // semitones re middle Sa
    float currentLevel() const { return levelMeter; }
    bool isSounding() const;
    const RagaScale& scale() const { return ragaScale; }
    double saFrequency() const { return saHz; }

    // Maps a MIDI key to a pitch (semitones re middle Sa). Returns false for
    // keyswitch keys.
    bool keyToPitch (int note, double& pitch) const;

private:
    void startNote (int note, float velocity, bool legato, bool articulate);
    void releaseVoice();
    void updateNeighbours();
    double tickPitch();
    Engine& engine();

    SynthParams params;
    RagaScale ragaScale;
    double fs = 44100.0, invFs = 1.0 / 44100.0;
    double saHz = 261.63;
    double tempoBpm = 120.0;

    VeenaEngine veena;
    ViolinEngine violin;
    VenuEngine venu;
    Tambura tambura;
    EngineType currentEngine = EngineType::Veena;
    float gainSmoothed = 0.8f, exprSmoothed = 1.0f, tamburaSmoothed = 0.0f;

    // Note stack (mono, last-note priority)
    std::array<int, 128> stack {};
    int stackSize = 0;
    std::array<bool, 128> sustained {};
    bool sustainPedal = false;
    int currentNote = -1;
    float lastVelocity = 0.8f;

    // Gamaka state
    GamakaMode activeMode = GamakaMode::Plain;
    GamakaMode lastParamMode = GamakaMode::Plain;
    GamakaMode noteMode = GamakaMode::Plain;
    double target = 0.0;
    double up1 = 1.0, lo1 = -1.0, upJ = 1.0, loJ = -1.0;
    double noteTime = 0.0, releaseTime = 0.0;
    bool noteIsLegato = false, released = true;
    bool gliding = false;
    double glideFrom = 0.0, glideElapsed = 0.0;
    double kampitaPhase = 0.0;
    double lastPreBend = 0.0;
    double smoothed1 = 0.0, smoothed2 = 0.0;
    int pendingOffSamples = -1;

    // Controllers
    float modWheel = 0.0f, aftertouch = 0.0f, expression = 1.0f;
    float pitchBend = 0.0f; // -1..1

    float levelMeter = 0.0f;

    bool engineMuted = false;
    std::array<VoiceEvent, 256> events {};
    int numEvents = 0, blockPos = 0;
    void pushEvent (VoiceEvent::Type type, float velocity, bool legato);
    void engineNoteOn (float velocity, bool legato);
    void engineNoteOff();
};

} // namespace gamaka
