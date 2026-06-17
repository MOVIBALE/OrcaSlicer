#ifndef slic3r_MixedLayerHeight_hpp_
#define slic3r_MixedLayerHeight_hpp_

#include <cstddef>
#include <vector>

namespace Slic3r {

std::vector<size_t> build_mixed_layer_height_spans(const std::vector<double>& layer_heights,
                                                   double                     max_combined_height,
                                                   bool                       include_first_layer);

} // namespace Slic3r

#endif
