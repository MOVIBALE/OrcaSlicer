#include "MixedLayerHeight.hpp"

namespace Slic3r {

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
