//
// Created by Paul Walker on 3/2/22.
//

#include "catch2/catch2.hpp"
#include "sst/oscillators/Helpers.h"
#include <iostream>

TEST_CASE("Magic Circle")
{
    SECTION("Some Sines")
    {
        auto ms = sst::oscillators_mit::MagicCircle<>();
        ms.init(480.0, 1.f / 48000);

        for (int i = 0; i < 1000; ++i)
        {
            REQUIRE(ms.value() == Approx(sin(i * 480.f / 48000 * 2.0 * M_PI)).margin(1e-3));
            ms.step();
        }
    }
}