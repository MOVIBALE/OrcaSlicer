#ifndef slic3r_MixedLayerHeight_hpp_
#define slic3r_MixedLayerHeight_hpp_

#include <cstddef>
#include <vector>

namespace Slic3r {

std::vector<size_t> build_mixed_layer_height_spans(const std::vector<double>& layer_heights,
                                                   double                     max_combined_height,
                                                   bool                       include_first_layer);

double mixed_nozzle_combined_layer_height(double fine_layer_height,
                                          double coarse_nozzle_diameter,
                                          int    layer_ratio);

} // namespace Slic3r

#endif
