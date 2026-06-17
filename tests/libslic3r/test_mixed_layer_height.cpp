#include <catch2/catch.hpp>

#include "libslic3r/MixedLayerHeight.hpp"

using namespace Slic3r;

TEST_CASE("mixed layer spans target the upper fine layer at exact 2:1 height", "[MixedLayerHeight]")
{
    const std::vector<double> heights {0.1, 0.1, 0.1, 0.1};

    const std::vector<size_t> spans = build_mixed_layer_height_spans(heights, 0.2, true);

    REQUIRE(spans.size() == 4);
    CHECK(spans[0] == 0);
    CHECK(spans[1] == 2);
    CHECK(spans[2] == 0);
    CHECK(spans[3] == 2);
}

TEST_CASE("mixed layer spans do not exceed the requested maximum height", "[MixedLayerHeight]")
{
    const std::vector<double> heights {0.12, 0.12, 0.12};

    const std::vector<size_t> spans = build_mixed_layer_height_spans(heights, 0.2, true);

    CHECK(spans == std::vector<size_t>({0, 0, 0}));
}

TEST_CASE("mixed layer spans can keep the first layer on the fine height", "[MixedLayerHeight]")
{
    const std::vector<double> heights {0.1, 0.1, 0.1, 0.1};

    const std::vector<size_t> spans = build_mixed_layer_height_spans(heights, 0.2, false);

    CHECK(spans == std::vector<size_t>({0, 0, 2, 0}));
}
