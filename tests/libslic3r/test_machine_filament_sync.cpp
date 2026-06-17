#include <catch2/catch.hpp>

#include "libslic3r/MachineFilamentSync.hpp"

#include "nlohmann/json.hpp"

using namespace Slic3r;
using nlohmann::json;

TEST_CASE("U1 machine filament sync maps physical heads into logical tool slots", "[MachineFilamentSync]")
{
    json response = {
        {"result",
         {{"status",
           {{"print_task_config",
             {{"filament_vendor", {"Snapmaker", "Snapmaker", "Generic", "Generic"}},
              {"filament_type", {"PLA", "PLA", "PETG", "ABS"}},
              {"filament_sub_type", {"Basic", "Silk", "NONE", "NONE"}},
              {"filament_color_rgba", {"000000FF", "FFFFFFFF", "003776FF", "F78E0EFF"}},
              {"filament_exist", {true, true, true, false}},
              {"extruder_map_table", {1, 0, 2, 3}}}},
            {"extruder", {{"nozzle_diameter", 0.4}}},
            {"extruder1", {{"nozzle_diameter", 0.2}}},
            {"extruder2", {{"nozzle_diameter", 0.4}}},
            {"extruder3", {{"nozzle_diameter", 0.4}}}}}}}};

    const std::vector<MachineFilamentSyncSlot> slots = build_machine_filament_sync_slots(response);

    REQUIRE(slots.size() == 4);
    CHECK(slots[0].slot_index == 0);
    CHECK(slots[0].physical_index == 1);
    CHECK(slots[0].display_name == "Snapmaker PLA Silk");
    CHECK(slots[0].filament_type == "PLA");
    CHECK(slots[0].color == "#FFFFFF");
    CHECK(slots[0].nozzle_diameter == "0.2");
    CHECK(slots[0].exists);

    CHECK(slots[1].slot_index == 1);
    CHECK(slots[1].physical_index == 0);
    CHECK(slots[1].display_name == "Snapmaker PLA Basic");
    CHECK(slots[1].color == "#000000");
    CHECK(slots[1].nozzle_diameter == "0.4");

    CHECK(slots[2].display_name == "Generic PETG");
    CHECK(slots[2].color == "#003776");

    CHECK(slots[3].display_name == "Generic ABS");
    CHECK_FALSE(slots[3].exists);
}
