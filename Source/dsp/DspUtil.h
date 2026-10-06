#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gamaka
{

constexpr double twoPi = 6.283185307179586;

inline float clampf (float v, float lo, float hi) { return std::min (hi, std::max (lo, v)); }
inline float lerpf (float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstep (float x)
{
    x = clampf (x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

// Fast deterministic white noise in [-1, 1].
struct Noise
{
    uint32_t state = 0x12345678u;
    explicit Noise (uint32_t seed = 0x12345678u) : state (seed ? seed : 1u) {}
    float next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float) ((int32_t) state) * (1.0f / 2147483648.0f);
    }
};

struct OnePole
{
    float a = 0.0f, z = 0.0f;
    void setTime (float seconds, double fs)
    {
        a = seconds <= 0.0f ? 0.0f : (float) std::exp (-1.0 / (seconds * fs));
    }
    void setCutoff (float hz, double fs) { a = (float) std::exp (-twoPi * hz / fs); }
    float process (float x) { return z = x + (z - x) * a; }
    void reset (float v = 0.0f) { z = v; }
};

struct DcBlocker
{
    float x1 = 0.0f, y1 = 0.0f, r = 0.995f;
    void prepare (double fs) { r = (float) std::exp (-twoPi * 20.0 / fs); }
    float process (float x)
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }
    void reset() { x1 = y1 = 0.0f; }
};

// RBJ biquad, transposed direct form II.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void setPeak (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = twoPi * f / fs, alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        set (1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
    }
    void setBandpass (double fs, double f, double q)
    {
        const double w = twoPi * f / fs, alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        set (alpha, 0, -alpha, 1 + alpha, -2 * c, 1 - alpha);
    }
    void setHighpass (double fs, double f, double q)
    {
        const double w = twoPi * f / fs, alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + alpha, -2 * c, 1 - alpha);
    }
    void setLowpass (double fs, double f, double q)
    {
        f = std::min (f, fs * 0.45);
        const double w = twoPi * f / fs, alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + alpha, -2 * c, 1 - alpha);
    }
    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0.0f; }

private:
    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = (float) (nb0 / na0);
        b1 = (float) (nb1 / na0);
        b2 = (float) (nb2 / na0);
        a1 = (float) (na1 / na0);
        a2 = (float) (na2 / na0);
    }
};

// Topology-preserving state-variable lowpass; safe to modulate per sample.
struct SvfLowpass
{
    float ic1 = 0, ic2 = 0;
    float process (float x, float cutoffHz, float q, double fs)
    {
        cutoffHz = clampf (cutoffHz, 20.0f, (float) fs * 0.45f);
        const float g = (float) std::tan (3.141592653589793 * cutoffHz / fs);
        const float k = 1.0f / q;
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1;
        ic2 = 2 * v2 - ic2;
        return v2;
    }
    void reset() { ic1 = ic2 = 0; }
};

inline float polyBlep (double t, double dt)
{
    if (t < dt)
    {
        t /= dt;
        return (float) (t + t - t * t - 1.0);
    }
    if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return (float) (t * t + t + t + 1.0);
    }
    return 0.0f;
}

} // namespace gamaka
