#include "Voices.h"

namespace gamaka
{

//==============================================================================
void WaveguideString::prepare (double sampleRate)
{
    fs = sampleRate;
    int size = 1;
    while (size < (int) (fs / 15.0) + 8)
        size <<= 1;
    buf.assign ((size_t) size, 0.0f);
    exc.assign ((size_t) size, 0.0f);
    mask = size - 1;
    reset();
}

void WaveguideString::reset()
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    writePos = 0;
    excPos = excLen = 0;
    lpState = 0.0f;
    env = 0.0f;
}

void WaveguideString::setBrightness (float b01)
{
    lpCoeff = 0.02f + 0.6f * (1.0f - clampf (b01, 0.0f, 1.0f));
}

void WaveguideString::setDecay (float t60Seconds) { t60 = std::max (0.01f, t60Seconds); }

void WaveguideString::setFrequency (float hz)
{
    freq = clampf (hz, 16.0f, (float) fs * 0.25f);
    // The loop lowpass adds a / (1 - a) samples of delay at low frequencies.
    const float filterDelay = lpCoeff / (1.0f - lpCoeff);
    delay = std::max (3.0f, (float) (fs / freq) - filterDelay);
    loopGain = (float) std::exp (-6.907755 / (freq * t60));
}

void WaveguideString::pluck (float amplitude, float brightness01, float pluckPosition)
{
    excLen = std::min ((int) std::lround (fs / freq), mask);
    excPos = 0;

    OnePole lp;
    lp.setCutoff (600.0f + 9000.0f * brightness01 * brightness01, fs);
    for (int i = 0; i < excLen; ++i)
        exc[(size_t) i] = lp.process (noise.next());

    // Comb filter = plucking point along the string.
    const int p = std::max (1, (int) (pluckPosition * (float) excLen));
    for (int i = excLen - 1; i >= p; --i)
        exc[(size_t) i] -= exc[(size_t) (i - p)];

    double sumSq = 0.0;
    for (int i = 0; i < excLen; ++i)
        sumSq += exc[(size_t) i] * exc[(size_t) i];
    const float rms = (float) std::sqrt (sumSq / std::max (1, excLen));
    const float gain = rms > 1.0e-6f ? amplitude * 0.5f / rms : 0.0f;
    for (int i = 0; i < excLen; ++i)
        exc[(size_t) i] *= gain;
}

float WaveguideString::readInterpolated (float d) const
{
    float pos = (float) writePos - d;
    const int size = mask + 1;
    if (pos < 0.0f)
        pos += (float) size;
    const int i = (int) pos;
    const float f = pos - (float) i;
    const float xm1 = buf[(size_t) ((i - 1) & mask)];
    const float x0 = buf[(size_t) (i & mask)];
    const float x1 = buf[(size_t) ((i + 1) & mask)];
    const float x2 = buf[(size_t) ((i + 2) & mask)];
    // 4-point Hermite
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

float WaveguideString::tick()
{
    const float rd = readInterpolated (delay);
    lpState = (1.0f - lpCoeff) * rd + lpCoeff * lpState;
    float y = lpState * loopGain;
    if (excPos < excLen)
        y += exc[(size_t) excPos++];
    if (std::abs (y) < 1.0e-15f)
        y = 0.0f;
    buf[(size_t) writePos] = y;
    writePos = (writePos + 1) & mask;
    env += (std::abs (y) - env) * 0.0005f;
    return y;
}

//==============================================================================
void VeenaEngine::prepare (double sampleRate)
{
    fs = sampleRate;
    string.prepare (fs);
    buzzHp.setHighpass (fs, 1800.0, 0.7);
    dc.prepare (fs);
    lastBody = -1.0f;
    reset();
}

void VeenaEngine::reset()
{
    string.reset();
    buzzHp.reset();
    for (auto& b : bodyEq)
        b.reset();
    dc.reset();
    held = false;
}

void VeenaEngine::noteOn (float velocity, bool legato, const VoiceParams& p)
{
    held = true;
    const float amp = legato ? velocity * p.legatoPluck : velocity;
    if (amp > 0.01f)
        string.pluck (amp, p.brightness * (0.5f + 0.5f * velocity));
}

void VeenaEngine::noteOff (const VoiceParams&) { held = false; }

float VeenaEngine::tick (float freqHz, const VoiceParams& p)
{
    if (p.body != lastBody)
    {
        lastBody = p.body;
        bodyEq[0].setPeak (fs, 190.0, 1.4, 9.0 * p.body);
        bodyEq[1].setPeak (fs, 520.0, 2.0, 5.0 * p.body);
        bodyEq[2].setPeak (fs, 2700.0, 1.0, 4.0 * p.body);
    }

    string.setBrightness (p.brightness);
    string.setDecay (held ? p.decay : std::min (p.decay, p.release));
    string.setFrequency (freqHz);
    const float s = string.tick();

    // Jivari: amplitude-dependent asymmetric clipping, high-passed so it only
    // adds the bright, buzzy partials that bloom and fade with the pluck.
    const float buzz = buzzHp.process (std::tanh (3.0f * s + 0.4f) - 0.379949f);
    float y = s + p.jivari * 0.6f * buzz;

    for (auto& b : bodyEq)
        y = b.process (y);
    return dc.process (y) * 0.7f;
}

//==============================================================================
void SustainEnvelope::setTimes (float attack, float release, double fs)
{
    atkCoeff = (float) std::exp (-1.0 / (std::max (0.001f, attack) * 0.3 * fs));
    relCoeff = (float) std::exp (-1.0 / (std::max (0.005f, release) * 0.2 * fs));
}

float SustainEnvelope::tick()
{
    const float c = releasing ? relCoeff : atkCoeff;
    level = target + (level - target) * c;
    return level;
}

//==============================================================================
void ViolinEngine::prepare (double sampleRate)
{
    fs = sampleRate;
    noiseBp.setBandpass (fs, 3000.0, 0.8);
    wobble.setCutoff (3.0f, fs);
    accentDecay = (float) std::exp (-1.0 / (0.04 * fs));
    dc.prepare (fs);
    lastBody = -1.0f;
    reset();
}

void ViolinEngine::reset()
{
    env = {};
    phase = 0.0;
    accent = 0.0f;
    lp.reset();
    noiseBp.reset();
    for (auto& b : bodyEq)
        b.reset();
    dc.reset();
}

void ViolinEngine::noteOn (float vel, bool legato, const VoiceParams& p)
{
    velocity = vel;
    env.target = 1.0f;
    env.releasing = false;
    accent = legato ? p.legatoPluck * 0.6f : 1.0f;
}

void ViolinEngine::noteOff (const VoiceParams&)
{
    env.target = 0.0f;
    env.releasing = true;
}

float ViolinEngine::tick (float freqHz, const VoiceParams& p)
{
    if (p.body != lastBody)
    {
        lastBody = p.body;
        bodyEq[0].setPeak (fs, 285.0, 2.5, 8.0 * p.body);  // A0 air mode
        bodyEq[1].setPeak (fs, 470.0, 2.0, 5.0 * p.body);  // B1 body modes
        bodyEq[2].setPeak (fs, 1500.0, 1.5, -4.0 * p.body);
        bodyEq[3].setPeak (fs, 2900.0, 1.2, 6.0 * p.body); // bridge hill
    }

    env.setTimes (p.attack, p.release, fs);
    const float e = env.tick();
    accent *= accentDecay;

    const double dt = std::min (0.45, freqHz / fs);
    phase += dt;
    if (phase >= 1.0)
        phase -= 1.0;
    const float saw = (float) (2.0 * phase - 1.0) - polyBlep (phase, dt);

    const float w = wobble.process (noise.next());
    const float drive = e * velocity;
    const float cutoff = freqHz * (1.5f + 14.0f * p.brightness * (0.4f + 0.6f * drive)) + 300.0f;
    const float tone = lp.process (saw, cutoff, 0.8f, fs);
    const float bow = noiseBp.process (noise.next()) * p.breath * (0.35f + 1.5f * accent);

    float y = (tone * (1.0f + 4.0f * w + 0.25f * accent) + bow) * drive;
    for (auto& b : bodyEq)
        y = b.process (y);
    return dc.process (y) * 0.35f;
}

//==============================================================================
void VenuEngine::prepare (double sampleRate)
{
    fs = sampleRate;
    breathBp.setBandpass (fs, 2200.0, 0.6);
    breathLp.setLowpass (fs, 7000.0, 0.7);
    wobble.setCutoff (4.0f, fs);
    chiffDecay = (float) std::exp (-1.0 / (0.05 * fs));
    reset();
}

void VenuEngine::reset()
{
    env = {};
    phase = 0.0;
    chiff = 0.0f;
    breathBp.reset();
    breathLp.reset();
    toneLp.reset();
}

void VenuEngine::noteOn (float vel, bool legato, const VoiceParams& p)
{
    velocity = vel;
    env.target = 1.0f;
    env.releasing = false;
    chiff = legato ? p.legatoPluck * 0.5f : 1.0f;
}

void VenuEngine::noteOff (const VoiceParams&)
{
    env.target = 0.0f;
    env.releasing = true;
}

float VenuEngine::tick (float freqHz, const VoiceParams& p)
{
    env.setTimes (p.attack, p.release, fs);
    const float e = env.tick();
    chiff *= chiffDecay;

    phase += freqHz / fs;
    if (phase >= 1.0)
        phase -= 1.0;
    const double ph = twoPi * phase;
    const float h2 = 0.08f + 0.2f * p.brightness, h3 = 0.03f + 0.1f * p.brightness;
    const float tone = (float) (std::sin (ph) + h2 * std::sin (2.0 * ph) + h3 * std::sin (3.0 * ph));

    const float n = noise.next();
    const float air = breathLp.process (breathBp.process (n));
    const float w = wobble.process (noise.next());
    const float drive = e * velocity;

    const float body = toneLp.process (tone, freqHz * (3.0f + 10.0f * p.brightness), 0.7f, fs);
    const float y = (body * (1.0f - 0.4f * p.breath) * (1.0f + 3.0f * w)
                     + air * p.breath * (1.2f + 4.0f * chiff))
                    * drive;
    return y * 0.4f;
}

//==============================================================================
void Tambura::prepare (double sampleRate)
{
    fs = sampleRate;
    for (auto& s : strings)
        s.prepare (fs);
    buzzHp.setHighpass (fs, 2000.0, 0.7);
    bodyLow.setPeak (fs, 140.0, 1.0, 6.0);
    reset();
}

void Tambura::reset()
{
    for (auto& s : strings)
        s.reset();
    buzzHp.reset();
    bodyLow.reset();
    clock = 0.0;
    nextString = 0;
}

float Tambura::tick (float saHz, int firstString, float brightness)
{
    static constexpr double firstRatio[3] = { 0.75, 2.0 / 3.0, 15.0 / 16.0 }; // mandra P / M / N
    const float freqs[4] = { (float) (saHz * firstRatio[std::clamp (firstString, 0, 2)]),
                             saHz * 1.0007f, saHz * 0.9995f, saHz * 0.5f };

    static constexpr double cycle = 3.6;
    static constexpr double pluckAt[4] = { 0.0, 0.8, 1.6, 2.4 };
    clock += 1.0 / fs;
    if (clock >= cycle)
        clock -= cycle;

    float sum = 0.0f;
    for (int i = 0; i < 4; ++i)
    {
        auto& s = strings[(size_t) i];
        s.setBrightness (0.55f + 0.4f * brightness);
        s.setDecay (6.0f);
        s.setFrequency (freqs[i]);
        if (i == nextString && clock >= pluckAt[i] && clock < pluckAt[i] + 0.5)
        {
            s.pluck (0.45f + 0.05f * noise.next(), 0.35f, 0.08f);
            nextString = (nextString + 1) % 4;
        }
        sum += s.tick();
    }

    const float buzz = buzzHp.process (std::tanh (4.0f * sum + 0.5f) - 0.462117f);
    return bodyLow.process (sum + 0.9f * buzz) * 0.35f;
}

} // namespace gamaka
