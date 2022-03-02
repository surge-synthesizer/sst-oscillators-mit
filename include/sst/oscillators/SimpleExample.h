//
// Created by Paul Walker on 2/28/22.
//

#ifndef SST_OSCILLATORS_MIT_ALLPASSPD_H
#define SST_OSCILLATORS_MIT_ALLPASSPD_H

#include "API.h"
#include <cstdint>
#include <vector>
#include <string>
#include <cassert>

namespace sst
{
namespace oscillators_mit
{
/*
 * DO NOT USE THIS OSCILLATOR TO MAKE MUSIC EVER!
 * This is just a simple example of the full API we use to expose
 * the oscillator to clients.
 */
template <typename ftype = float, int bksz = DEFAULT_BLOCK_SIZE,
          typename TuningProvider = DummyPitchProvider>
struct SimpleExample
{
    static constexpr int blocksize = bksz;
    const double dsamplerate, dsamplerate_inv;
    const TuningProvider *tuning{nullptr};
    explicit SimpleExample(double samplerate, TuningProvider *p)
        : dsamplerate(samplerate), dsamplerate_inv(1.0 / samplerate), tuning(p)
    {
        assert(tuning);
    }

    std::string getName() const { return "Simple Example"; }

    uint32_t numParams() { return 2; }

    ParamType getParamType(uint32_t which)
    {
        switch (which)
        {
        case 0:
            return FLOAT;
        case 1:
            return DISCRETE;
        }
        return UNKNOWN;
    }

    void getParamRange(uint32_t which, ftype &fmin, ftype &fmax, ftype &fdef)
    {
        fmin = 0;
        fmax = 1;
        fdef = 0.5;
    }

    void getDiscreteValues(uint32_t which, std::vector<std::string> &values, int &def)
    {
        values.clear();
        values.push_back("pulse");
        values.push_back("sine");
        values.push_back("saw");
        def = 0;
    }

    std::string getParamName(uint32_t which)
    {
        switch (which)
        {
        case 0:
            return "skew";
        case 1:
            return "shape";
        }
        return "err";
    }

    bool init(ParamData<ftype> *pdata)
    {
        phase = 0;
        return true;
    }
    bool supportsStereo() { return false; }

    template <bool FM>
    void process(float pitch, ftype *outputL, ftype *outputR, ParamData<ftype> *pdata,
                 ftype *fmData)
    {
        auto shp = pdata[1].i;
        auto skew = pdata[0].f;
        auto dphase = tuning->pitch_to_dphase(pitch, dsamplerate_inv);
        for (int i = 0; i < blocksize; ++i)
        {
            if (shp == 0)
                outputL[i] = phase < skew ? -1 : 1;
            else if (shp == 1)
            {
                auto sphase = pow(phase, 1.0 + 1.3 * (skew - 0.5));
                outputL[i] = std::sin(2.0 * M_PI * sphase);
            }
            else if (shp == 2)
            {
                auto sphase = pow(phase, 1.0 + 1.3 * (skew - 0.5));
                outputL[i] = sphase * 2.0 - 1.0;
            }

            outputR[i] = outputL[i];
            phase += dphase;
            if (phase > 1)
                phase -= 1;
        }
    }
    float phase{0};
};
} // namespace oscillators_mit
} // namespace sst
#endif // SST_OSCILLATORS_MIT_ALLPASSPD_H
