//
// Created by Paul Walker on 2/28/22.
//

#include <memory>

#include "catch2/catch2.hpp"
#include "sst/oscillators/APITester.h"
#include "sst/oscillators/SimpleExample.h"

TEST_CASE("Dummy Procider")
{
    auto dp = std::make_unique<sst::oscillators_mit::DummyPitchProvider>();
    REQUIRE(dp->note_to_pitch(60) == 32);
    REQUIRE(dp->pitch_to_dphase(69, 1.0) == Approx(440.0).margin(1e-5));
}
TEST_CASE("API Compliance")
{
    REQUIRE(true); // If it compiles this basically passes
    SECTION("AllPass PD")
    {
        auto t = sst::oscillators_testclients::APITester<sst::oscillators_mit::SimpleExample<>>();
        REQUIRE(true);
    }
}