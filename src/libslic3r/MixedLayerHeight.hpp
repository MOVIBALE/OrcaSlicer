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

int mixed_nozzle_auto_layer_height_ratio(double fine_nozzle_diameter,
                                         double coarse_nozzle_diameter);

int mixed_nozzle_effective_layer_height_ratio(bool   auto_ratio,
                                              int    manual_ratio,
                                              double fine_nozzle_diameter,
                                              double coarse_nozzle_diameter);

} // namespace Slic3r

#endif
