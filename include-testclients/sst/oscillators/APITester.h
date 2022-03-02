//
// Created by Paul Walker on 3/1/22.
//

#ifndef SST_OSCILLATORS_MIT_APITESTER_H
#define SST_OSCILLATORS_MIT_APITESTER_H

// This header is purposefully fragie w.r.t CATCH2

#include <memory>
#include <sst/oscillators/API.h>

namespace sst
{
namespace oscillators_testclients
{
template <typename T> struct APITester
{
    APITester()
    {
        tuning = std::make_unique<sst::oscillators_mit::DummyPitchProvider>();
        osc = std::make_unique<T>(48000, tuning.get());
        REQUIRE(T::blocksize > 0);
        REQUIRE(osc->numParams() >= 0);
        REQUIRE(!osc->getName().empty());

        sst::oscillators_mit::ParamData<float> data[7];
        REQUIRE(osc->numParams() <= 7);

        for (auto i = 0; i < osc->numParams(); ++i)
        {
            auto opt = osc->getParamType(i);
            REQUIRE(opt != sst::oscillators_mit::ParamType::UNKNOWN);
            if (opt == sst::oscillators_mit::FLOAT)
            {
                float min, max, def;
                REQUIRE(osc->getParamRange(i, min, max, def));
                REQUIRE(min < max);
                REQUIRE(def >= min);
                REQUIRE(def <= max);
                data[i].f = def;
            }
            if (opt == sst::oscillators_mit::DISCRETE)
            {
                std::vector<std::string> values;
                int defv;
                REQUIRE(osc->getDiscreteValues(i, values, defv));
                REQUIRE(!values.empty());
                REQUIRE(defv >= 0);
                REQUIRE(defv < values.size());
                data[i].i = defv;
            }

            REQUIRE(!osc->getParamName(i).empty());
        }

        REQUIRE(osc->init(data));

        auto isS = osc->supportsStereo();
        INFO("Is Stereo" << isS);

        float dL[32], dR[32];
        osc->template process<false>(60, dL, dR, data, 0.f, nullptr);
    }
    std::unique_ptr<T> osc;
    std::unique_ptr<sst::oscillators_mit::DummyPitchProvider> tuning;
};
} // namespace oscillators_testclients
} // namespace sst
#endif // SST_OSCILLATORS_MIT_APITESTER_H
