//
// Created by Paul Walker on 3/2/22.
//

#ifndef SST_OSCILLATORS_MIT_HELPERS_H
#define SST_OSCILLATORS_MIT_HELPERS_H

#include <cmath>
#include <array>
#include "SSE2Import.h"
#include <iostream>

namespace sst
{
namespace oscillators_mit
{

// MCF (or 'magic circle') form of a sine oscillator from
// https://ccrma.stanford.edu/~jos/pasp/Digital_Sinusoid_Generators.html or
// https://vicanek.de/articles/QuadOsc.pdf
template <typename ftype = float, ftype SIN(ftype) = std::sin> struct MagicCircle
{
    ftype k{-1000}, u0, un, v0{0.f}, vn;

    inline void setFrequency(float frequency, float samplerate_inv)
    {
        // epsilon = 2 sin(pi f T)
        k = 2.f * SIN(M_PI * frequency * samplerate_inv);
    }

    inline void init(float frequency, float samplerate_inv)
    {
        setFrequency(frequency, samplerate_inv);
        u0 = SIN(M_PI * frequency * samplerate_inv + M_PI / 2.0); // since it is cos
        v0 = 0.f;
    }

    inline ftype value() { return v0; }

    inline ftype step()
    {
        un = u0 - k * v0;
        vn = v0 + k * un;
        u0 = un;
        v0 = vn;
        return vn;
    }
};

// The Quadrature Oscillator from https://vicanek.de/articles/QuadOsc.pdf
template <typename ftype = float> struct QuadratureSine
{
    ftype v0, vn, u0, un, k1, k2;

    inline void setFrequency(float frequency, float samplerate_inv)
    {
        k1 = (M_PI * frequency * samplerate_inv);
        k2 = 2.0 * k1 / (1 + k1 * k1);

        float norm = 1.f / sqrt(u0 * u0 + v0 * v0);
        u0 = u0 * norm;
        v0 = v0 * norm;
    }

    inline void init(float frequency, float samplerate_inv)
    {
        u0 = 1.0f;
        v0 = 0.f;
        setFrequency(frequency, samplerate_inv);
    }

    inline float step()
    {
        auto w = u0 - k1 * v0;
        vn = v0 + k2 * w;
        un = w - k1 * vn;

        v0 = vn;
        u0 = un;

        return v0;
    }

    inline ftype sinv() { return v0; }
    inline ftype cosv() { return u0; }
};

/*
 * https://www.wolframalpha.com/input?i=PadeApproximant%5BSin%5Bx%5D%2C%7Bx%2C0%2C%7B5%2C6%7D%7D%5D
 *
 * sin(x) = (183284640 x - 23819040 x^3 + 532182 x^5)
 *   /(183284640 + 6728400 x^2 + 126210 x^4 + 1331 x^6)
 *   = (x (183284640 + x^2 (-23819040 + 532182 x^2)))/
 *     (183284640 + x^2 (6728400 + x^2 (126210 + 1331 x^2)))
 */
inline float sinePade(float x)
{
    auto x2 = x * x;
    auto num = (x * (183284640.0 + x2 * (-23819040.0 + 532182.0 * x2)));
    auto den = (183284640 + x2 * (6728400 + x2 * (126210 + 1331 * x2)));
    return num / den;
}

template <int bs = 32, typename ftype = float> struct InterpOverBlock
{
    static constexpr int blocksize = bs;
    static constexpr ftype blocksize_inv = ((ftype)1) / blocksize;

    template <ftype F(int), std::size_t... I>
    static constexpr std::array<float, sizeof...(I)> fillArray(std::index_sequence<I...>)
    {
        return std::array<float, sizeof...(I)>{F(I)...};
    }
    template <ftype F(int), std::size_t N> static constexpr std::array<float, N> fillArray()
    {
        return fillArray<F>(std::make_index_sequence<N>{});
    }

    static constexpr ftype deltaI(int i) { return i * blocksize_inv; }
    static constexpr std::array<ftype, blocksize> delta = fillArray<deltaI, blocksize>();

    ftype values alignas(16)[blocksize];
    ftype v0;

    inline void init(ftype v)
    {
        for (auto i = 0; i < blocksize; ++i)
        {
            values[i] = v;
        }
        v0 = v;
    }

    inline void target(ftype v)
    {
        auto dv = (v - v0);

        for (auto i = 0; i < blocksize; ++i)
            values[i] = v0 + dv * delta[i];
        v0 = v;
    }

    inline ftype at(int i) const { return values[i]; }
};
} // namespace oscillators_mit
} // namespace sst
#endif // SST_OSCILLATORS_MIT_HELPERS_H
