#include "Synth.h"

#include <cmath>
#include <limits>

namespace gamaka
{

const char* gamakaName (GamakaMode m)
{
    static const char* names[] = { "Plain", "Kampita", "Jaru Up", "Jaru Down", "Nokku",
                                   "Odukkal", "Sphurita", "Pratyahata", "Orikkai" };
    return names[std::clamp ((int) m, 0, (int) GamakaMode::Count - 1)];
}

const char* gamakaDescription (GamakaMode m)
{
    static const char* text[] = {
        "Plain note (still glides when played legato)",
        "Oscillates between the note and its raga neighbour",
        "Etra jaru: slides up into the note from a lower raga note",
        "Irakka jaru: slides down into the note from a higher raga note",
        "Stress from above: starts on the upper note and pushes down onto the note",
        "Pushes up onto the note from the lower raga note",
        "Dips to the lower raga note and back (re-articulates a held note)",
        "Flicks to the upper raga note and back",
        "Flicks up to the upper raga note as you release the key",
    };
    return text[std::clamp ((int) m, 0, (int) GamakaMode::Count - 1)];
}

namespace
{
float slideCurve (float x, float shape)
{
    x = clampf (x, 0.0f, 1.0f);
    const float s = x * x * (3.0f - 2.0f * x);
    if (shape < 0.5f)
        return lerpf (x, s, shape * 2.0f);
    const float e = 1.0f - (1.0f - x) * (1.0f - x) * (1.0f - x);
    return lerpf (s, e, (shape - 0.5f) * 2.0f);
}

// One kampita cycle, 0 at phase 0 and 1, 1 at the turning point.
float kampitaShape (float phase, float dwell, float tilt)
{
    const float a = 0.15f + 0.7f * tilt;
    const float x = phase < a ? 0.5f * phase / a : 0.5f + 0.5f * (phase - a) / (1.0f - a);
    const float r = 0.5f - 0.5f * (float) std::cos (twoPi * x);
    return lerpf (r, smoothstep (smoothstep (r)), dwell);
}

// Out-and-back touch that lingers briefly on the far note.
float bump (float x) { return smoothstep (std::min (1.0f, 1.6f * (float) std::sin (3.141592653589793 * x))); }

int floorDiv12 (int a) { return (a >= 0) ? a / 12 : -((11 - a) / 12); }
} // namespace

//==============================================================================
Synth::Synth() { setParams (params); }

void Synth::prepare (double sampleRate)
{
    fs = sampleRate;
    invFs = 1.0 / fs;
    veena.prepare (fs);
    violin.prepare (fs);
    venu.prepare (fs);
    tambura.prepare (fs);
    reset();
}

void Synth::reset()
{
    veena.reset();
    violin.reset();
    venu.reset();
    tambura.reset();
    stackSize = 0;
    sustained.fill (false);
    sustainPedal = false;
    currentNote = -1;
    released = true;
    gliding = false;
    pendingOffSamples = -1;
    modWheel = aftertouch = pitchBend = 0.0f;
    expression = 1.0f;
}

void Synth::setParams (const SynthParams& p)
{
    const bool scaleChanged = p.raga != params.raga || p.tuning != params.tuning;
    if (p.engine != currentEngine)
    {
        engine().reset();
        currentEngine = p.engine;
    }
    if (p.gamaka != lastParamMode)
        activeMode = lastParamMode = p.gamaka;

    params = p;
    if (scaleChanged || ragaScale.ragaIndex() != p.raga)
    {
        ragaScale.set (p.raga, p.tuning);
        updateNeighbours();
    }
    saHz = p.a4Hz * std::pow (2.0, (p.saMidiNote - 69 + p.saFineCents / 100.0) / 12.0);
}

Engine& Synth::engine()
{
    switch (currentEngine)
    {
        case EngineType::Violin: return violin;
        case EngineType::Venu:   return venu;
        default:                 return veena;
    }
}

bool Synth::isSounding() const
{
    if (engineMuted)
        return currentNote >= 0 && (! released || releaseTime < params.voice.release + params.flickTime);
    switch (currentEngine)
    {
        case EngineType::Violin: return violin.isActive();
        case EngineType::Venu:   return venu.isActive();
        default:                 return veena.isActive();
    }
}

double Synth::expressionLevel() const { return exprSmoothed; }

//==============================================================================
bool Synth::keyToPitch (int note, double& pitch) const
{
    if (params.keyswitches && note >= params.keyswitchBase && note < params.keyswitchBase + (int) GamakaMode::Count)
        return false;

    switch (params.keyMode)
    {
        case KeyMode::Swara:
        {
            // C = S, D = R, E = G, F = M, G = P, A = D, B = N; C4 (60) is middle Sa.
            // Black keys give the chromatic svarasthana (for anya swaras).
            static constexpr int letterOf[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
            const int octave = floorDiv12 (note) - 5;
            const int pc = note - (octave + 5) * 12;
            pitch = letterOf[pc] >= 0 ? ragaScale.letterPitch (letterOf[pc], octave)
                                      : ragaScale.chromaticPitch (pc) + 12.0 * octave;
            return true;
        }
        case KeyMode::ChromaticSnap:
            pitch = ragaScale.snap (ragaScale.chromaticPitch (note - params.saMidiNote));
            return true;
        case KeyMode::ChromaticFree:
        default:
            pitch = ragaScale.chromaticPitch (note - params.saMidiNote);
            return true;
    }
}

void Synth::handleMidi (const uint8_t* d, int size)
{
    if (size < 1)
        return;
    const int status = d[0] & 0xf0;
    const int d1 = size > 1 ? d[1] & 0x7f : 0;
    const int d2 = size > 2 ? d[2] & 0x7f : 0;

    switch (status)
    {
        case 0x90: d2 > 0 ? noteOn (d1, d2) : noteOff (d1); break;
        case 0x80: noteOff (d1); break;
        case 0xb0:
            if (d1 == 1)
                modWheel = d2 / 127.0f;
            else if (d1 == 2 || d1 == 11)
                expression = d2 / 127.0f;
            else if (d1 == 64)
            {
                const bool down = d2 >= 64;
                if (sustainPedal && ! down)
                {
                    sustainPedal = false;
                    for (int n = 0; n < 128; ++n)
                        if (sustained[(size_t) n])
                        {
                            sustained[(size_t) n] = false;
                            noteOff (n);
                        }
                }
                sustainPedal = down;
            }
            else if (d1 == 120 || d1 == 123)
                allNotesOff();
            break;
        case 0xd0: aftertouch = d1 / 127.0f; break;
        case 0xe0: pitchBend = clampf ((float) (((d2 << 7) | d1) - 8192) / 8191.0f, -1.0f, 1.0f); break;
        default: break;
    }
}

void Synth::noteOn (int note, int velocity)
{
    if (note < 0 || note > 127)
        return;
    if (params.keyswitches && note >= params.keyswitchBase && note < params.keyswitchBase + (int) GamakaMode::Count)
    {
        activeMode = (GamakaMode) (note - params.keyswitchBase);
        return;
    }

    // Remove from the stack if it's somehow already there, then push.
    for (int i = 0; i < stackSize; ++i)
        if (stack[(size_t) i] == note)
        {
            for (int j = i; j < stackSize - 1; ++j)
                stack[(size_t) j] = stack[(size_t) j + 1];
            --stackSize;
            break;
        }
    const bool legato = stackSize > 0;
    stack[(size_t) stackSize++] = note;
    sustained[(size_t) note] = false;

    const float v = velocity / 127.0f;
    lastVelocity = 1.0f - params.velocitySens + params.velocitySens * v;
    startNote (note, lastVelocity, legato, true);
}

void Synth::noteOff (int note)
{
    if (note < 0 || note > 127)
        return;
    if (params.keyswitches && note >= params.keyswitchBase && note < params.keyswitchBase + (int) GamakaMode::Count)
        return;
    if (sustainPedal)
    {
        sustained[(size_t) note] = true;
        return;
    }

    bool found = false;
    for (int i = 0; i < stackSize; ++i)
        if (stack[(size_t) i] == note)
        {
            for (int j = i; j < stackSize - 1; ++j)
                stack[(size_t) j] = stack[(size_t) j + 1];
            --stackSize;
            found = true;
            break;
        }
    if (! found || note != currentNote)
        return;

    if (stackSize > 0)
        startNote (stack[(size_t) stackSize - 1], lastVelocity, true, false); // fall back to the held note
    else
        releaseVoice();
}

void Synth::allNotesOff()
{
    stackSize = 0;
    sustained.fill (false);
    if (currentNote >= 0 && ! released)
        releaseVoice();
}

void Synth::startNote (int note, float velocity, bool legato, bool articulate)
{
    double p = 0.0;
    if (! keyToPitch (note, p))
        return;

    const bool sounding = isSounding() && currentNote >= 0;
    bool glide = false;
    if (params.glideMode == GlideMode::Legato)
        glide = legato && sounding;
    else if (params.glideMode == GlideMode::Always)
        glide = sounding;

    if (glide)
    {
        glideFrom = lastPreBend;
        glideElapsed = 0.0;
        gliding = params.glideTime > 0.0f;
    }
    else
        gliding = false;

    target = p;
    currentNote = note;
    updateNeighbours();

    noteTime = 0.0;
    releaseTime = 0.0;
    released = false;
    pendingOffSamples = -1;
    noteIsLegato = glide;
    noteMode = activeMode;
    if (noteMode == GamakaMode::Kampita || ! glide)
        kampitaPhase = 0.0;

    if (! sounding)
    {
        // Start the smoother on the note's first pitch rather than swooping in
        // from wherever the last note ended.
        smoothed1 = smoothed2 = std::numeric_limits<double>::quiet_NaN();
    }

    if (articulate)
        engineNoteOn (velocity, legato);
}

void Synth::releaseVoice()
{
    released = true;
    releaseTime = 0.0;
    if (noteMode == GamakaMode::Orikkai)
        pendingOffSamples = std::max (1, (int) (params.flickTime * fs));
    else
        engineNoteOff();
}

void Synth::pushEvent (VoiceEvent::Type type, float velocity, bool legato)
{
    if (numEvents < (int) events.size())
        events[(size_t) numEvents++] = { type, blockPos, velocity, legato };
}

void Synth::engineNoteOn (float velocity, bool legato)
{
    pushEvent (VoiceEvent::On, velocity, legato);
    engine().noteOn (velocity, legato, params.voice);
}

void Synth::engineNoteOff()
{
    pushEvent (VoiceEvent::Off, 0.0f, false);
    engine().noteOff (params.voice);
}

void Synth::updateNeighbours()
{
    const int d = ragaScale.nearestDegree (target);
    const double pd = ragaScale.degreeToPitch (d);
    const bool above = pd > target + 1e-6, below = pd < target - 1e-6;
    auto up = [&] (int steps) { return ragaScale.degreeToPitch (above ? d + steps - 1 : d + steps); };
    auto lo = [&] (int steps) { return ragaScale.degreeToPitch (below ? d - steps + 1 : d - steps); };
    up1 = up (1);
    lo1 = lo (1);
    upJ = up (std::max (1, params.jaruSteps));
    loJ = lo (std::max (1, params.jaruSteps));
}

double Synth::tickPitch()
{
    const auto& P = params;
    const double dt = invFs;
    noteTime += dt;
    if (released)
        releaseTime += dt;

    // Legato glide between notes
    double base = target;
    if (gliding)
    {
        glideElapsed += dt;
        const double x = glideElapsed / std::max (1.0e-4, (double) P.glideTime);
        if (x >= 1.0)
            gliding = false;
        else
            base = glideFrom + (target - glideFrom) * slideCurve ((float) x, P.slideShape);
    }

    // Onset / release gamakas (offsets relative to the target note)
    const double t = noteTime, D = P.onsetDelay;
    double off = 0.0;
    switch (noteMode)
    {
        case GamakaMode::JaruUp:
        case GamakaMode::JaruDown:
            if (! noteIsLegato)
            {
                const double a = (noteMode == GamakaMode::JaruUp ? loJ : upJ) - target;
                const double x = (t - D) / std::max (1.0e-4, (double) P.jaruTime);
                off = x < 0.0 ? a : x < 1.0 ? a * (1.0 - slideCurve ((float) x, P.slideShape)) : 0.0;
            }
            break;
        case GamakaMode::Nokku:
        case GamakaMode::Odukkal:
            if (! noteIsLegato)
            {
                const double a = (noteMode == GamakaMode::Nokku ? up1 : lo1) - target;
                const double x = (t - D) / std::max (1.0e-4, (double) P.flickTime);
                off = x < 0.0 ? a : x < 1.0 ? a * (1.0 - x * x) : 0.0; // accelerating: lands with a push
            }
            break;
        case GamakaMode::Sphurita:
        case GamakaMode::Pratyahata:
        {
            const double a = (noteMode == GamakaMode::Sphurita ? lo1 : up1) - target;
            const double x = (t - D) / std::max (1.0e-4, (double) P.flickTime);
            if (x >= 0.0 && x < 1.0)
                off = a * bump ((float) x);
            break;
        }
        case GamakaMode::Orikkai:
            if (released)
                off = (up1 - target) * smoothstep ((float) (releaseTime / std::max (1.0e-4, (double) P.flickTime)));
            break;
        default:
            break;
    }

    // Kampita: from the gamaka itself and/or live from mod wheel / aftertouch.
    const bool kampitaNote = noteMode == GamakaMode::Kampita;
    const double rate = P.kampitaSync ? (tempoBpm / 60.0) / std::max (0.0625f, P.kampitaSyncBeats)
                                      : (double) P.kampitaRate;
    float depth = P.modWheelDepth * std::max (modWheel, aftertouch);
    if (kampitaNote && t >= D && (P.kampitaCycles == 0 || kampitaPhase < P.kampitaCycles))
        depth = std::max (depth, P.kampitaDepth);

    double kOff = 0.0;
    if (depth > 1.0e-4f)
    {
        const float frac = (float) (kampitaPhase - std::floor (kampitaPhase));
        switch (P.kampitaDir)
        {
            case KampitaDirection::Up:
                kOff = depth * (up1 - target) * kampitaShape (frac, P.kampitaDwell, P.kampitaTilt);
                break;
            case KampitaDirection::Down:
                kOff = depth * (lo1 - target) * kampitaShape (frac, P.kampitaDwell, P.kampitaTilt);
                break;
            case KampitaDirection::Around:
            {
                float f = frac + 0.25f;
                if (f >= 1.0f)
                    f -= 1.0f;
                const double s = 2.0 * kampitaShape (f, P.kampitaDwell, P.kampitaTilt) - 1.0;
                kOff = s >= 0.0 ? s * depth * (up1 - target) : -s * depth * (lo1 - target);
                break;
            }
        }
    }
    if (! (kampitaNote && t < D))
        kampitaPhase += rate * dt;

    lastPreBend = base + off + kOff;

    // Pitch bend: plain semitones, or stepping through the raga's notes.
    double bentBase = base;
    if (pitchBend != 0.0f)
    {
        if (P.bendMode == BendMode::Semitones)
            bentBase = base + pitchBend * P.bendRange;
        else
            bentBase = ragaScale.degreeFToPitch (ragaScale.pitchToDegreeF (base) + pitchBend * P.bendRange);
    }

    const double raw = bentBase + off + kOff;
    if (std::isnan (smoothed2))
        smoothed1 = smoothed2 = raw;
    const double tau = std::max (1.0e-4, 0.5 * P.smoothing);
    const double c = std::exp (-dt / tau);
    smoothed1 = raw + (smoothed1 - raw) * c;
    smoothed2 = smoothed1 + (smoothed2 - smoothed1) * c;
    return smoothed2;
}

void Synth::patternNoteOn (float velocity)
{
    stackSize = 0;
    currentNote = 128; // not a key: key releases never end a pattern note
    noteMode = GamakaMode::Plain;
    released = false;
    noteTime = releaseTime = 0.0;
    pendingOffSamples = -1;
    engineNoteOn (velocity, false);
}

void Synth::patternNoteOff()
{
    if (currentNote >= 0 && ! released)
    {
        released = true;
        releaseTime = 0.0;
        engineNoteOff();
    }
}

void Synth::render (float* out, int numSamples, float* pitchOut, const float* pitchIn)
{
    const float gainCoeff = (float) std::exp (-1.0 / (0.02 * fs));
    auto& eng = engine();

    for (int i = 0; i < numSamples; ++i)
    {
        double pitch;
        if (pitchIn != nullptr)
        {
            pitch = pitchIn[i];
            noteTime += invFs;
            if (released)
                releaseTime += invFs;
            smoothed1 = smoothed2 = lastPreBend = pitch;
        }
        else
            pitch = tickPitch();
        if (pendingOffSamples >= 0 && --pendingOffSamples < 0)
            engineNoteOff();

        gainSmoothed = params.gain + (gainSmoothed - params.gain) * gainCoeff;
        exprSmoothed = expression + (exprSmoothed - expression) * gainCoeff;
        tamburaSmoothed = params.tamburaLevel + (tamburaSmoothed - params.tamburaLevel) * gainCoeff;

        const float freq = (float) (saHz * std::exp2 (pitch / 12.0));
        float s = engineMuted ? 0.0f : eng.tick (freq, params.voice) * exprSmoothed;
        if (tamburaSmoothed > 1.0e-4f)
            s += tambura.tick ((float) saHz, params.tamburaString, params.voice.brightness) * tamburaSmoothed;

        s = std::tanh (s * gainSmoothed);
        out[i] = s;
        if (pitchOut != nullptr)
            pitchOut[i] = (float) pitch;
        levelMeter = std::max (std::abs (s), levelMeter * 0.9999f);
        ++blockPos;
    }
}

} // namespace gamaka
