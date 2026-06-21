#include "MixedLayerHeight.hpp"

#include <algorithm>
#include <cmath>

namespace Slic3r {

namespace {

constexpr int mixed_nozzle_min_layer_ratio = 2;
constexpr int mixed_nozzle_max_layer_ratio = 8;

int clamp_mixed_nozzle_layer_ratio(const int ratio)
{
    return std::min(std::max(mixed_nozzle_min_layer_ratio, ratio), mixed_nozzle_max_layer_ratio);
}

} // namespace

int mixed_nozzle_auto_layer_height_ratio(const double fine_nozzle_diameter,
                                         const double coarse_nozzle_diameter)
{
    if (fine_nozzle_diameter <= 0. || coarse_nozzle_diameter <= 0.)
        return mixed_nozzle_min_layer_ratio;

    return clamp_mixed_nozzle_layer_ratio(int(std::ceil(coarse_nozzle_diameter / fine_nozzle_diameter)));
}

int mixed_nozzle_effective_layer_height_ratio(const bool   auto_ratio,
                                              const int    manual_ratio,
                                              const double fine_nozzle_diameter,
                                              const double coarse_nozzle_diameter)
{
    return auto_ratio ?
               mixed_nozzle_auto_layer_height_ratio(fine_nozzle_diameter, coarse_nozzle_diameter) :
               clamp_mixed_nozzle_layer_ratio(manual_ratio);
}

double mixed_nozzle_combined_layer_height(const double fine_layer_height,
                                          const double coarse_nozzle_diameter,
                                          const int    layer_ratio)
{
    if (fine_layer_height <= 0. || coarse_nozzle_diameter <= 0.)
        return 0.;

    const int ratio = clamp_mixed_nozzle_layer_ratio(layer_ratio);
    return std::min(fine_layer_height * double(ratio), coarse_nozzle_diameter);
}

std::vector<size_t> build_mixed_layer_height_spans(const std::vector<double>& layer_heights,
                                                   const double               max_combined_height,
                                                   const bool                 include_first_layer)
{
    std::vector<size_t> spans(layer_heights.size(), 0);
    if (layer_heights.size() < 2 || max_combined_height <= 0.)
        return spans;

    constexpr double eps = 1e-6;
    const size_t     first_layer = include_first_layer ? 0 : 1;

    double accumulated_height = 0.;
    size_t accumulated_layers = 0;

    for (size_t layer_idx = first_layer; layer_idx < layer_heights.size(); ++layer_idx) {
        const double layer_height = layer_heights[layer_idx];
        if (layer_height <= 0.)
            continue;

        if (accumulated_layers > 0 && accumulated_height + layer_height > max_combined_height + eps) {
            if (accumulated_layers > 1)
                spans[layer_idx - 1] = accumulated_layers;
            accumulated_height = 0.;
            accumulated_layers = 0;
        }

        accumulated_height += layer_height;
        ++accumulated_layers;

        if (accumulated_height >= max_combined_height - eps) {
            if (accumulated_layers > 1)
                spans[layer_idx] = accumulated_layers;
            accumulated_height = 0.;
            accumulated_layers = 0;
        }
    }

    if (accumulated_layers > 1)
        spans.back() = accumulated_layers;

    return spans;
}

} // namespace Slic3r
