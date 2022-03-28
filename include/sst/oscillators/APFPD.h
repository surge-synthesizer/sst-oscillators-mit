//
// Created by Paul Walker on 2/28/22.
//

#ifndef SST_OSCILLATORS_MIT_ALLPASSPD_H
#define SST_OSCILLATORS_MIT_ALLPASSPD_H

#include "API.h"
#include "Helpers.h"

#include <cstdint>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>

namespace sst
{
namespace oscillators_mit
{
/*
 * Based on "Sound Synthesis Using an Allpass Filter Chain with Audio-Rate Coefficient Modulation"
 * DAFx-09 Kleimola, Pekonen, Penttinen, Valimaki and "Adaptive Phase Distortion Synthesis",
 * Lazzarini, Timoney, Pekonen, Valimai, DAFx-09
 */
template <typename ftype = float, int bksz = DEFAULT_BLOCK_SIZE,
          typename TuningProvider = DummyPitchProvider>
struct APFPD
{
    static constexpr int blocksize = bksz;
    const double dsamplerate, dsamplerate_inv;
    const TuningProvider *tuning{nullptr};
    explicit APFPD(double samplerate, TuningProvider *p)
        : dsamplerate(samplerate), dsamplerate_inv(1.0 / samplerate), tuning(p)
    {
        assert(tuning);
    }

    std::string getName() const { return "APF PD"; }

    uint32_t numParams() { return 4; }

    enum ParamIndices
    {
        apf_model,
        apf_amp,
        apf_cm,
        apf_distort
    };

    enum Models
    {
        mod_constant,
        mod_sin,
        mod_saw,
        mod_tri,
        mod_chirp
    };

    ParamType getParamType(uint32_t which)
    {
        switch (which)
        {
        case apf_model:
            return DISCRETE;
        case apf_amp:
        case apf_cm:
        case apf_distort:
            return FLOAT;
        }
        return UNKNOWN;
    }

    bool getParamRange(uint32_t which, ftype &fmin, ftype &fmax, ftype &fdef)
    {
        switch (which)
        {
        case apf_distort:
        case apf_amp:
            fmin = 0;
            fmax = 1;
            fdef = 0.0;
            return true;

        case apf_cm:
            fmin = 0.25;
            fmax = 8;
            fdef = 1.0;
            return true;
        }

        return false;
    }

    bool getDiscreteValues(uint32_t which, std::vector<std::string> &values, int &def)
    {
        if (which == apf_model)
        {
            def = 0;
            values.clear();
            values.push_back("constant");
            values.push_back("sine");
            values.push_back("saw");
            values.push_back("twopoint");
            values.push_back("chirp");
            return true;
        }
        return false;
    }

    std::string getParamName(uint32_t which)
    {
        switch (which)
        {
        case apf_model:
            return "model";
        case apf_amp:
            return "Amplitude";
        case apf_cm:
            return "C:M";
        case apf_distort:
            return "Skew/Distort";
        }
        return "err";
    }

    QuadratureSine<> carrier, sinemodulator;
    InterpOverBlock<blocksize> ampInterp, cmInterp, distortInterp, omegaInterp, fmdepthInterp;

    float phase{0}, carPhase{0}, modPhase{0};
    InterpOverBlock<blocksize> dModPhase;

    bool init(float pitch, ParamData<ftype> *pdata)
    {
        phase = 0;
        carrier.init(tuning->note_to_pitch(pitch) * MIDI_0_FREQ, dsamplerate_inv);
        sinemodulator.init(pdata[apf_cm].f * tuning->note_to_pitch(pitch) * MIDI_0_FREQ,
                           dsamplerate_inv);

        float targetFrequency = tuning->note_to_pitch(pitch) * MIDI_0_FREQ;
        auto targetOmega = targetFrequency * 2.0 * M_PI * dsamplerate_inv;
        omegaInterp.init(targetOmega);
        ampInterp.init(pdata[apf_amp].f);
        distortInterp.init(pdata[apf_distort].f);
        cmInterp.init(pdata[apf_cm].f);
        fmdepthInterp.init(0);

        auto dphase = tuning->pitch_to_dphase(pitch, dsamplerate_inv) * pdata[apf_cm].f;
        dModPhase.init(dphase);
        modPhase = 0;
        phase = 0;
        carPhase = 0;

        return true;
    }
    bool supportsStereo() { return false; }

    // The DSP code is here
    float outP{0}, carrP{0};
    inline void calcDirect(const float carrier[blocksize], const float mod[blocksize],
                           float output[blocksize])
    {
        for (int i = 0; i < blocksize; ++i)
        {
            output[i] = carrP - mod[i] * (carrier[i] - outP);
            outP = output[i];
            carrP = carrier[i];
            if (fabs(outP) > 100)
                std::terminate();
        }
    }

    inline void calc(const float carrier[blocksize], const float mod[blocksize],
                     float output[blocksize])
    {
        calcDirect(carrier, mod, output);
    }

    template <bool FM>
    void process(float pitch, ftype *outputL, ftype *outputR, ParamData<ftype> *pdata,
                 ftype fmDepth, ftype *fmData)
    {
        ampInterp.target(pdata[apf_amp].f);
        distortInterp.target(pdata[apf_distort].f);
        cmInterp.target(pdata[apf_cm].f);

        float targetFrequency = tuning->note_to_pitch(pitch) * MIDI_0_FREQ;
        auto targetOmega = targetFrequency * 2.0 * M_PI * dsamplerate_inv;
        omegaInterp.target(targetOmega);

        auto dphase = tuning->pitch_to_dphase(pitch, dsamplerate_inv);
        dModPhase.target(dphase * pdata[apf_cm].f);

        float fv = 32.0 * M_PI * fmDepth * fmDepth * fmDepth;
        fmdepthInterp.target(std::clamp(fv, -1.e5f, 1.e5f));

        // Collect the next block of the carrier
        float carrierD alignas(16)[blocksize];

        if (FM)
        {
            // TODO: THis can be sse improved
            for (int i = 0; i < blocksize; ++i)
            {
                carPhase += dphase;
                auto tPhase = carPhase + fmdepthInterp.values[i] * fmData[i];
                tPhase = (tPhase + 0.5);
                if (tPhase > 1)
                    tPhase -= (int)tPhase;
                if (tPhase < 0)
                    tPhase -= (int)tPhase - 1;

                tPhase = tPhase - 0.5;
                carrierD[i] = sinePade(2 * M_PI * tPhase);
                if (carPhase > 0.5)
                    carPhase -= 1;
            }
        }
        else
        {
            carrier.setFrequency(targetFrequency, dsamplerate_inv);
            for (int i = 0; i < blocksize; ++i)
            {
                carrierD[i] = carrier.step();
            }
        }
        float modulatorD alignas(16)[blocksize];
        switch (pdata[apf_model].i)
        {
        case mod_sin:
        {
            sinemodulator.setFrequency(targetFrequency * cmInterp.values[0], dsamplerate_inv);

            for (auto i = 0; i < blocksize; ++i)
            {
                modulatorD[i] = (1.0 + sinemodulator.step()) * 0.5;
            }
            break;
        }
        default:
        case mod_constant:
        {
            for (auto i = 0; i < blocksize; ++i)
            {
                modulatorD[i] = 1.0;
            }

            break;
        }
        case mod_saw:
        {
            for (auto i = 0; i < blocksize; ++i)
            {
                auto saw = modPhase;
                auto d = distortInterp.values[i];
                if (d > 0.001 || d < 0.999)
                {
                    if (saw < d)
                        saw += (0.5 - d) * saw / d;
                    else
                        saw += (0.5 - d) * (1 - saw) / (1 - d);
                }
                modulatorD[i] = saw;
                modPhase += dModPhase.values[i];
                if (modPhase > 1)
                    modPhase -= 1;
            }
            break;
        }
        case mod_tri:
        {
            for (auto i = 0; i < blocksize; ++i)
            {
                modulatorD[i] = (modPhase < 0.5 ? modPhase * 2 : 2 - modPhase * 2);
                modPhase += dModPhase.values[i];
                if (modPhase > 1)
                    modPhase -= 1;
            }
            break;
        }
        }
        // sinemodulator.setFrequency(targetFrequency * (1.0 + 3 * pdata[apf_amp].f),
        // dsamplerate_inv);

        float mod alignas(16)[blocksize];
        for (int i = 0; i < blocksize; ++i)
        {
            auto phi = M_PI * ampInterp.values[i] * modulatorD[i];
            auto Q = 0.5 * (phi + omegaInterp.at(i));
            auto m = std::clamp((Q * cos(omegaInterp.at(i)) - sin(omegaInterp.at(i))) / Q, -1., 1.);
            mod[i] = m;
        }

        calc(carrierD, mod, outputL);
        // memcpy(outputL, carrierD, blocksize * sizeof(float));

        memcpy(outputR, outputL, blocksize * sizeof(float));
    } // namespace oscillators_mit
};    // namespace sst
} // namespace oscillators_mit
} // namespace sst
#endif // SST_OSCILLATORS_MIT_ALLPASSPD_H
