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

TEST_CASE("mixed nozzle ratio target derives coarse layer height from current fine layer", "[MixedLayerHeight]")
{
    CHECK(mixed_nozzle_combined_layer_height(0.10, 0.40, 2) == Approx(0.20));
    CHECK(mixed_nozzle_combined_layer_height(0.12, 0.60, 2) == Approx(0.24));
}

TEST_CASE("mixed nozzle ratio target is clamped to the selected coarse nozzle", "[MixedLayerHeight]")
{
    CHECK(mixed_nozzle_combined_layer_height(0.30, 0.40, 2) == Approx(0.40));
    CHECK(mixed_nozzle_combined_layer_height(0.10, 0.40, 1) == Approx(0.20));
}

TEST_CASE("mixed nozzle auto coarse layer height uses half coarse nozzle", "[MixedLayerHeight]")
{
    CHECK(mixed_nozzle_auto_coarse_layer_height(0.40, 0.) == Approx(0.20));
    CHECK(mixed_nozzle_auto_coarse_layer_height(0.80, 0.) == Approx(0.40));
}

TEST_CASE("mixed nozzle auto coarse layer height honors max layer height", "[MixedLayerHeight]")
{
    CHECK(mixed_nozzle_auto_coarse_layer_height(0.80, 0.32) == Approx(0.32));
    CHECK(mixed_nozzle_combined_layer_height(0.10, 0.80, 0.32, 4) == Approx(0.32));
}

TEST_CASE("mixed nozzle target layer height derives a valid fine-layer ratio", "[MixedLayerHeight]")
{
    CHECK(mixed_nozzle_layer_height_ratio_from_target(0.10, 0.20, 0.40, 0.) == 2);
    CHECK(mixed_nozzle_layer_height_ratio_from_target(0.10, 0.40, 0.80, 0.) == 4);
    CHECK(mixed_nozzle_layer_height_ratio_from_target(0.20, 0.20, 0.40, 0.) == 0);
}

TEST_CASE("mixed nozzle auto ratio follows nozzle diameter pairing", "[MixedLayerHeight]")
{
    CHECK(mixed_nozzle_auto_layer_height_ratio(0.20, 0.80) == 4);
    CHECK(mixed_nozzle_auto_layer_height_ratio(0.20, 0.40) == 2);
    CHECK(mixed_nozzle_auto_layer_height_ratio(0.40, 0.80) == 2);
    CHECK(mixed_nozzle_auto_layer_height_ratio(0.40, 0.60) == 2);
}

TEST_CASE("mixed nozzle effective ratio keeps manual override", "[MixedLayerHeight]")
{
    CHECK(mixed_nozzle_effective_layer_height_ratio(false, 3, 0.20, 0.80) == 3);
    CHECK(mixed_nozzle_effective_layer_height_ratio(true, 2, 0.20, 0.80) == 4);
}
