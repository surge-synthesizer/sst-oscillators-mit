//
// Created by Paul Walker on 3/2/22.
//

#ifndef SST_OSCILLATORS_MIT_HELPERS_H
#define SST_OSCILLATORS_MIT_HELPERS_H

#include <cmath>
#include "SSE2Import.h"

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
} // namespace oscillators_mit
} // namespace sst
#endif // SST_OSCILLATORS_MIT_HELPERS_H
