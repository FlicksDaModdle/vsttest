#pragma once

#include "DspUtil.h"

#include <array>
#include <vector>

namespace gamaka
{

struct VoiceParams
{
    float brightness = 0.6f; // 0..1
    float decay = 4.0f;      // seconds (plucked string sustain, T60)
    float body = 0.5f;       // 0..1 body resonance amount
    float jivari = 0.35f;    // 0..1 bridge buzz (veena / tambura)
    float attack = 0.03f;    // seconds (sustained engines)
    float release = 0.35f;   // seconds
    float breath = 0.25f;    // 0..1 bow / breath noise
    float legatoPluck = 0.3f; // 0..1 re-articulation strength on legato notes
};

// A digital-waveguide (Karplus-Strong family) string whose pitch can be
// modulated continuously every sample, which is what lets the veena hold one
// pluck through long slides and oscillations like a real fretted string.
class WaveguideString
{
public:
    void prepare (double sampleRate);
    void reset();

    void setFrequency (float hz);
    void setBrightness (float b01);
    void setDecay (float t60Seconds);
    void pluck (float amplitude, float brightness01, float pluckPosition = 0.13f);
    float tick();

    float level() const { return env; }

private:
    float readInterpolated (float delay) const;

    double fs = 44100.0;
    std::vector<float> buf;
    std::vector<float> exc;
    int mask = 0, writePos = 0;
    int excPos = 0, excLen = 0;
    float freq = 220.0f, delay = 100.0f;
    float lpCoeff = 0.3f, lpState = 0.0f;
    float t60 = 4.0f, loopGain = 0.99f;
    float env = 0.0f; // running output-level estimate
    Noise noise;
};

class Engine
{
public:
    virtual ~Engine() = default;
    virtual void prepare (double sampleRate) = 0;
    virtual void reset() = 0;
    virtual void noteOn (float velocity, bool legato, const VoiceParams& p) = 0;
    virtual void noteOff (const VoiceParams& p) = 0;
    virtual float tick (float freqHz, const VoiceParams& p) = 0;
    virtual bool isActive() const = 0;
};

class VeenaEngine : public Engine
{
public:
    void prepare (double sampleRate) override;
    void reset() override;
    void noteOn (float velocity, bool legato, const VoiceParams& p) override;
    void noteOff (const VoiceParams& p) override;
    float tick (float freqHz, const VoiceParams& p) override;
    bool isActive() const override { return string.level() > 1.0e-4f; }

private:
    double fs = 44100.0;
    WaveguideString string;
    Biquad buzzHp;
    std::array<Biquad, 3> bodyEq;
    float lastBody = -1.0f;
    bool held = false;
    DcBlocker dc;
};

// Sustained engines share an amplitude envelope.
struct SustainEnvelope
{
    float level = 0.0f, target = 0.0f;
    float atkCoeff = 0.0f, relCoeff = 0.0f;
    bool releasing = true;
    void setTimes (float attack, float release, double fs);
    float tick();
};

class ViolinEngine : public Engine
{
public:
    void prepare (double sampleRate) override;
    void reset() override;
    void noteOn (float velocity, bool legato, const VoiceParams& p) override;
    void noteOff (const VoiceParams& p) override;
    float tick (float freqHz, const VoiceParams& p) override;
    bool isActive() const override { return env.level > 1.0e-4f || ! env.releasing; }

private:
    double fs = 44100.0, phase = 0.0;
    SustainEnvelope env;
    float velocity = 0.8f, accent = 0.0f, accentDecay = 0.999f;
    SvfLowpass lp;
    Biquad noiseBp;
    std::array<Biquad, 4> bodyEq;
    float lastBody = -1.0f;
    OnePole wobble;
    Noise noise { 0x9e3779b9u };
    DcBlocker dc;
};

class VenuEngine : public Engine
{
public:
    void prepare (double sampleRate) override;
    void reset() override;
    void noteOn (float velocity, bool legato, const VoiceParams& p) override;
    void noteOff (const VoiceParams& p) override;
    float tick (float freqHz, const VoiceParams& p) override;
    bool isActive() const override { return env.level > 1.0e-4f || ! env.releasing; }

private:
    double fs = 44100.0, phase = 0.0;
    SustainEnvelope env;
    float velocity = 0.8f, chiff = 0.0f, chiffDecay = 0.999f;
    Biquad breathBp, breathLp;
    SvfLowpass toneLp;
    OnePole wobble;
    Noise noise { 0x51ed270bu };
};

// Four-string tambura drone that plucks itself in the traditional P-S-S-S cycle.
class Tambura
{
public:
    void prepare (double sampleRate);
    void reset();
    // saHz = frequency of middle Sa; firstString: 0 = Pa, 1 = Ma, 2 = Ni.
    float tick (float saHz, int firstString, float brightness);

private:
    double fs = 44100.0;
    std::array<WaveguideString, 4> strings;
    Biquad buzzHp, bodyLow;
    double clock = 0.0;
    int nextString = 0;
    Noise noise { 0x2545f491u };
};

} // namespace gamaka
