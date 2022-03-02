//
// Created by Paul Walker on 2/28/22.
//

#ifndef SST_OSCILLATORS_MIT_API_H
#define SST_OSCILLATORS_MIT_API_H

namespace sst
{
namespace oscillators_mit
{
enum ParamType
{
    UNKNOWN,
    FLOAT,
    DISCRETE
};

struct DummyPitchProvider
{
    const double MIDI_0_FREQ = 8.17579891564371; // or 440.0 * pow( 2.0, - (69.0/12.0 ) )

    double note_to_pitch(float note) const { return pow(2.0, note / 12.0); }

    double pitch_to_dphase(float pitch, double dsamplerate_inv) const
    {
        return MIDI_0_FREQ * note_to_pitch(pitch) * dsamplerate_inv;
    }
};

static constexpr uint32_t DEFAULT_BLOCK_SIZE = 32;

template <typename ftype> union ParamData
{
    ftype f;
    int32_t i;
};
} // namespace oscillators_mit
} // namespace sst
#endif // SST_OSCILLATORS_MIT_API_H
