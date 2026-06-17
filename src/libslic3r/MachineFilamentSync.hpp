#ifndef slic3r_MachineFilamentSync_hpp_
#define slic3r_MachineFilamentSync_hpp_

#include <string>
#include <vector>

#include "nlohmann/json.hpp"

namespace Slic3r {

struct MachineFilamentSyncSlot
{
    size_t      slot_index     = 0;
    size_t      physical_index = 0;
    std::string display_name;
    std::string filament_type;
    std::string color;
    std::string nozzle_diameter;
    bool        exists = true;
};

std::vector<MachineFilamentSyncSlot> build_machine_filament_sync_slots(const nlohmann::json& response);

} // namespace Slic3r

#endif // slic3r_MachineFilamentSync_hpp_
