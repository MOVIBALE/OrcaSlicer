#include <catch2/catch.hpp>

#include "libslic3r/libslic3r.h"
#include "libslic3r/GCode.hpp"
#include "libslic3r/GCodeReader.hpp"
#include "libslic3r/Geometry/ConvexHull.hpp"
#include "libslic3r/ModelArrange.hpp"
#include "libslic3r/PresetBundle.hpp"

#include "test_data.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <functional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include <boost/filesystem.hpp>

using namespace Slic3r;
using namespace Slic3r::Test;

std::regex perimeters_regex("G1 X[-0-9.]* Y[-0-9.]* E[-0-9.]* ; perimeter");
std::regex infill_regex("G1 X[-0-9.]* Y[-0-9.]* E[-0-9.]* ; infill");
std::regex skirt_regex("G1 X[-0-9.]* Y[-0-9.]* E[-0-9.]* ; skirt");

static size_t count_occurrences(const std::string& text, const std::string& needle)
{
    size_t count = 0;
    size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

static boost::filesystem::path source_root()
{
    return boost::filesystem::path(TEST_DATA_DIR).parent_path().parent_path();
}

static void select_preset_exact(PresetCollection& presets, const std::string& name)
{
    const auto& available = presets.get_presets();
    for (size_t index = 0; index < available.size(); ++index) {
        if (available[index].name == name) {
            presets.select_preset(index);
            return;
        }
    }
    throw std::runtime_error("PresetBundle did not load preset: " + name);
}

static DynamicPrintConfig resolved_preset_config(
    const std::string& vendor,
    const std::string& printer,
    const std::string& process,
    const std::string& filament,
    size_t filament_count = 1)
{
    PresetBundle bundle;
    const boost::filesystem::path profiles = source_root() / "resources" / "profiles";
    const auto [substitutions, loaded] = bundle.load_vendor_configs_from_json(
        profiles.string(), vendor, PresetBundle::LoadConfigBundleAttribute::LoadSystem,
        ForwardCompatibilitySubstitutionRule::Disable);
    if (!substitutions.empty() || loaded == 0)
        throw std::runtime_error("PresetBundle failed to resolve vendor: " + vendor);

    select_preset_exact(bundle.printers, printer);
    select_preset_exact(bundle.prints, process);
    select_preset_exact(bundle.filaments, filament);
    bundle.filament_presets.assign(1, filament);
    if (filament_count > 1)
        bundle.set_num_filaments(static_cast<unsigned int>(filament_count), "#808080");

    DynamicPrintConfig config = bundle.full_config();
    config.set_deserialize_strict({
        { "layer_height", 0.2 },
        { "first_layer_height", 0.2 }
    });
    return config;
}

static size_t min_position(size_t lhs, size_t rhs)
{
    if (lhs == std::string::npos)
        return rhs;
    if (rhs == std::string::npos)
        return lhs;
    return std::min(lhs, rhs);
}

static size_t find_gcode_line(
    const std::string& gcode,
    size_t begin,
    size_t end,
    const std::function<bool(std::string_view)>& predicate)
{
    size_t line_begin = begin;
    while (line_begin < end) {
        const size_t line_end = std::min(gcode.find('\n', line_begin), end);
        const std::string_view line(gcode.data() + line_begin, line_end - line_begin);
        if (predicate(line))
            return line_begin;
        if (line_end == std::string::npos || line_end >= end)
            break;
        line_begin = line_end + 1;
    }
    return std::string::npos;
}

static std::string_view gcode_words(std::string_view line)
{
    const size_t comment = line.find(';');
    return line.substr(0, comment);
}

static bool has_gcode_word(std::string_view words, char word)
{
    for (size_t i = 0; i < words.size(); ++i) {
        if (words[i] == word && (i == 0 || std::isspace(static_cast<unsigned char>(words[i - 1]))))
            return true;
    }
    return false;
}

static bool gcode_word_value(std::string_view words, char word, double& value)
{
    for (size_t i = 0; i < words.size(); ++i) {
        if (words[i] != word || (i > 0 && !std::isspace(static_cast<unsigned char>(words[i - 1]))))
            continue;

        const std::string value_and_tail(words.substr(i + 1));
        char*             end = nullptr;
        value                 = std::strtod(value_and_tail.c_str(), &end);
        return end != value_and_tail.c_str();
    }
    return false;
}

static bool is_linear_xy_move(std::string_view words)
{
    return words.rfind("G1 ", 0) == 0 && (has_gcode_word(words, 'X') || has_gcode_word(words, 'Y'));
}

static bool is_xy_move_without_extrusion(std::string_view line)
{
    const std::string_view words = gcode_words(line);
    return is_linear_xy_move(words) && !has_gcode_word(words, 'E');
}

static bool is_xy_extrusion(std::string_view line)
{
    const std::string_view words = gcode_words(line);
    double                 extrusion = 0.0;
    return is_linear_xy_move(words) && gcode_word_value(words, 'E', extrusion) && extrusion > 0.0;
}

static bool is_z_only_move(std::string_view line)
{
    const std::string_view words = gcode_words(line);
    return words.rfind("G1 Z", 0) == 0 && words.find(" X") == std::string_view::npos &&
           words.find(" Y") == std::string_view::npos && words.find(" E") == std::string_view::npos;
}

enum class FeatureToolRole : size_t
{
    OuterWall,
    InnerWall,
    SparseInfill,
    SolidInfill,
    Count
};

static std::array<std::array<bool, 4>, static_cast<size_t>(FeatureToolRole::Count)>
feature_tool_usage(const std::string& gcode)
{
    std::array<std::array<bool, 4>, static_cast<size_t>(FeatureToolRole::Count)> usage {};
    int current_tool = -1;
    int current_role = -1;
    std::istringstream stream(gcode);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.size() >= 2 && line[0] == 'T' && std::isdigit(static_cast<unsigned char>(line[1]))) {
            current_tool = std::atoi(line.c_str() + 1);
            current_role = -1;
            continue;
        }

        if (line == ";TYPE:Outer wall")
            current_role = static_cast<int>(FeatureToolRole::OuterWall);
        else if (line == ";TYPE:Inner wall")
            current_role = static_cast<int>(FeatureToolRole::InnerWall);
        else if (line == ";TYPE:Sparse infill")
            current_role = static_cast<int>(FeatureToolRole::SparseInfill);
        else if (line == ";TYPE:Internal solid infill")
            current_role = static_cast<int>(FeatureToolRole::SolidInfill);
        else if (line.rfind(";TYPE:", 0) == 0)
            current_role = -1;

        if (current_role >= 0 && current_tool >= 0 && current_tool < 4 && is_xy_extrusion(line))
            usage[size_t(current_role)][size_t(current_tool)] = true;
    }
    return usage;
}

static std::array<std::vector<double>, static_cast<size_t>(FeatureToolRole::Count)>
feature_layer_zs(const std::string& gcode)
{
    std::array<std::vector<double>, static_cast<size_t>(FeatureToolRole::Count)> zs;
    double current_z = -1.;
    int current_role = -1;
    std::istringstream stream(gcode);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind(";Z:", 0) == 0) {
            current_z = std::atof(line.c_str() + 3);
            continue;
        }

        if (line == ";TYPE:Outer wall")
            current_role = static_cast<int>(FeatureToolRole::OuterWall);
        else if (line == ";TYPE:Inner wall")
            current_role = static_cast<int>(FeatureToolRole::InnerWall);
        else if (line == ";TYPE:Sparse infill")
            current_role = static_cast<int>(FeatureToolRole::SparseInfill);
        else if (line == ";TYPE:Internal solid infill")
            current_role = static_cast<int>(FeatureToolRole::SolidInfill);
        else if (line.rfind(";TYPE:", 0) == 0)
            current_role = -1;

        if (current_role < 0 || current_z < 0. || !is_xy_extrusion(line))
            continue;

        std::vector<double>& role_zs = zs[size_t(current_role)];
        if (role_zs.empty() || std::abs(role_zs.back() - current_z) > EPSILON)
            role_zs.push_back(current_z);
    }
    return zs;
}

static size_t feature_z_delta_count(const std::vector<double>& zs, const double delta, const double tolerance = 0.015)
{
    size_t count = 0;
    for (size_t index = 1; index < zs.size(); ++index)
        if (std::abs((zs[index] - zs[index - 1]) - delta) <= tolerance)
            ++count;
    return count;
}

static void require_one_shot_per_layer(const std::string& gcode)
{
    size_t layer = gcode.find(";LAYER_CHANGE");
    REQUIRE(layer != std::string::npos);
    while (layer != std::string::npos) {
        const size_t next_layer = gcode.find(";LAYER_CHANGE", layer + 1);
        const size_t layer_end = next_layer == std::string::npos ? gcode.size() : next_layer;
        CHECK(count_occurrences(gcode.substr(layer, layer_end - layer), "\nESP_TIMELAPSE_SHOT\n") == 1);
        layer = next_layer;
    }
}

static void require_shot_after_completed_layer(const std::string& gcode)
{
    size_t layer = gcode.find(";LAYER_CHANGE");
    REQUIRE(layer != std::string::npos);
    while (layer != std::string::npos) {
        const size_t next_layer = gcode.find(";LAYER_CHANGE", layer + 1);
        const size_t layer_end = next_layer == std::string::npos ? gcode.size() : next_layer;
        const size_t first_extrusion = find_gcode_line(gcode, layer, layer_end, is_xy_extrusion);
        const size_t shot = find_gcode_line(gcode, layer, layer_end,
            [](std::string_view line) { return line == "ESP_TIMELAPSE_SHOT"; });
        const size_t extrusion_after_shot = find_gcode_line(
            gcode, shot == std::string::npos ? layer : shot + 1, layer_end, is_xy_extrusion);

        REQUIRE(first_extrusion != std::string::npos);
        REQUIRE(shot != std::string::npos);
        CHECK(first_extrusion < shot);
        CHECK(extrusion_after_shot == std::string::npos);
        layer = next_layer;
    }
}

static void require_real_wipe_tower_before_each_shot(const std::string& gcode)
{
    size_t layer = gcode.find(";LAYER_CHANGE");
    REQUIRE(layer != std::string::npos);
    while (layer != std::string::npos) {
        const size_t next_layer = gcode.find(";LAYER_CHANGE", layer + 1);
        const size_t layer_end  = next_layer == std::string::npos ? gcode.size() : next_layer;
        const size_t shot = find_gcode_line(gcode, layer, layer_end,
            [](std::string_view line) { return line == "ESP_TIMELAPSE_SHOT"; });
        REQUIRE(shot != std::string::npos);

        bool   found_real_tower = false;
        size_t tower_start      = layer;
        while ((tower_start = gcode.find("; WIPE_TOWER_START", tower_start)) != std::string::npos && tower_start < shot) {
            const size_t tower_end = gcode.find("; WIPE_TOWER_END", tower_start);
            REQUIRE(tower_end != std::string::npos);
            REQUIRE(tower_end < shot);
            if (find_gcode_line(gcode, tower_start, tower_end, is_xy_extrusion) != std::string::npos)
                found_real_tower = true;
            tower_start = tower_end + 1;
        }

        CHECK(found_real_tower);
        layer = next_layer;
    }
}

static void require_first_layer_tower_moves_inside_footprint(const std::string& gcode, Points footprint_points)
{
    REQUIRE(footprint_points.size() >= 3);
    const Polygon footprint = Geometry::convex_hull(std::move(footprint_points));
    const Polygons allowed = offset(footprint, scale_(0.02));
    REQUIRE_FALSE(allowed.empty());

    const size_t layer = gcode.find(";LAYER_CHANGE");
    const size_t next_layer = gcode.find(";LAYER_CHANGE", layer + 1);
    const size_t tower_begin = gcode.find("; WIPE_TOWER_START", layer);
    const size_t tower_end = gcode.find("; WIPE_TOWER_END", tower_begin);
    REQUIRE(layer != std::string::npos);
    REQUIRE(next_layer != std::string::npos);
    REQUIRE(tower_begin != std::string::npos);
    REQUIRE(tower_end != std::string::npos);
    REQUIRE(tower_end < next_layer);

    double x = 0.0;
    double y = 0.0;
    bool have_x = false;
    bool have_y = false;
    size_t checked_moves = 0;
    size_t line_begin = layer;
    while (line_begin < tower_end) {
        const size_t line_end = std::min(gcode.find('\n', line_begin), tower_end);
        const std::string_view line(gcode.data() + line_begin, line_end - line_begin);
        const std::string_view words = gcode_words(line);
        double value = 0.0;
        if (gcode_word_value(words, 'X', value)) {
            x = value;
            have_x = true;
        }
        if (gcode_word_value(words, 'Y', value)) {
            y = value;
            have_y = true;
        }
        if (line_begin >= tower_begin && is_linear_xy_move(words) && have_x && have_y) {
            ++checked_moves;
            CHECK(contains(allowed, Point::new_scale(x, y)));
        }
        if (line_end == std::string::npos || line_end >= tower_end)
            break;
        line_begin = line_end + 1;
    }
    REQUIRE(checked_moves > 0);
}

static double maximum_smooth_timelapse_z(const std::string& gcode)
{
    double maximum_z = 0.0;
    size_t begin = 0;
    while ((begin = gcode.find("; ESP32_TIMELAPSE_SMOOTH_BEGIN layer=", begin)) != std::string::npos) {
        const size_t end = gcode.find("; ESP32_TIMELAPSE_SMOOTH_END", begin);
        REQUIRE(end != std::string::npos);
        size_t line = begin;
        while ((line = gcode.find("\nG1 Z", line)) != std::string::npos && line < end) {
            const size_t value_begin = line + 5;
            maximum_z = std::max(maximum_z, std::stod(gcode.substr(value_begin)));
            line = value_begin;
        }
        begin = end;
    }
    return maximum_z;
}

static double final_smooth_timelapse_lift(const std::string& gcode)
{
    const size_t begin = gcode.rfind("; ESP32_TIMELAPSE_SMOOTH_BEGIN layer=");
    REQUIRE(begin != std::string::npos);
    const size_t end = gcode.find("; ESP32_TIMELAPSE_SMOOTH_END", begin);
    REQUIRE(end != std::string::npos);

    const size_t layer_z = gcode.rfind(";Z:", begin);
    REQUIRE(layer_z != std::string::npos);
    const double nominal_z = std::stod(gcode.substr(layer_z + 3));

    const size_t lift = gcode.find("\nG1 Z", begin);
    REQUIRE(lift != std::string::npos);
    REQUIRE(lift < end);
    const double lifted_z = std::stod(gcode.substr(lift + 5));
    return lifted_z - nominal_z;
}

static void require_traditional_timelapse_timing_contract(const std::string& gcode, int dwell_ms)
{
    size_t begin = 0;
    size_t block_count = 0;
    const std::string dwell_command = "G4 P" + std::to_string(dwell_ms);
    while ((begin = gcode.find("; ESP32_TIMELAPSE_TRADITIONAL_BEGIN layer=", begin)) != std::string::npos) {
        ++block_count;
        const size_t end = gcode.find("; ESP32_TIMELAPSE_TRADITIONAL_END", begin);
        REQUIRE(end != std::string::npos);
        const size_t motion_barrier = find_gcode_line(gcode, begin, end,
            [](std::string_view line) { return line == "M400"; });
        const size_t shot = find_gcode_line(gcode, begin, end,
            [](std::string_view line) { return line == "ESP_TIMELAPSE_SHOT"; });
        const size_t dwell = find_gcode_line(gcode, begin, end,
            [&dwell_command](std::string_view line) { return line == dwell_command; });
        const size_t park = find_gcode_line(gcode, begin, end, is_xy_move_without_extrusion);

        REQUIRE(motion_barrier != std::string::npos);
        REQUIRE(shot != std::string::npos);
        REQUIRE(dwell != std::string::npos);
        CHECK(motion_barrier < shot);
        CHECK(shot < dwell);
        CHECK(park == std::string::npos);
        begin = end + 1;
    }
    REQUIRE(block_count > 0);
}

static void require_safe_smooth_timelapse_motion(const std::string& gcode)
{
    size_t begin = 0;
    size_t block_count = 0;
    while ((begin = gcode.find("; ESP32_TIMELAPSE_SMOOTH_BEGIN layer=", begin)) != std::string::npos) {
        ++block_count;
        const size_t end_marker = gcode.find("; ESP32_TIMELAPSE_SMOOTH_END", begin);
        REQUIRE(end_marker != std::string::npos);
        const size_t end = gcode.find('\n', end_marker);
        REQUIRE(end != std::string::npos);

        const size_t lift = find_gcode_line(gcode, begin, end, is_z_only_move);
        const size_t park = find_gcode_line(gcode, lift == std::string::npos ? begin : lift + 1, end, [](std::string_view line) {
            const std::string_view words = gcode_words(line);
            return words.rfind("G1 X", 0) == 0 && words.find(" Y") != std::string_view::npos &&
                   words.find(" F18000") != std::string_view::npos && words.find(" E") == std::string_view::npos &&
                   words.find(" Z") == std::string_view::npos;
        });
        const size_t parked_m400 = find_gcode_line(gcode, park == std::string::npos ? begin : park, end,
            [](std::string_view line) { return line == "M400"; });
        const size_t shot = find_gcode_line(gcode, begin, end,
            [](std::string_view line) { return line == "ESP_TIMELAPSE_SHOT"; });
        const size_t dwell = find_gcode_line(gcode, begin, end,
            [](std::string_view line) { return line == "G4 P2000"; });
        const size_t descent_before_shot = find_gcode_line(
            gcode, park == std::string::npos ? begin : park + 1, shot == std::string::npos ? end : shot,
            [](std::string_view line) {
                const std::string_view words = gcode_words(line);
                return (words.rfind("G0 ", 0) == 0 || words.rfind("G1 ", 0) == 0) &&
                       words.find(" Z") != std::string_view::npos;
            });

        REQUIRE(lift != std::string::npos);
        REQUIRE(park != std::string::npos);
        REQUIRE(parked_m400 != std::string::npos);
        REQUIRE(shot != std::string::npos);
        REQUIRE(dwell != std::string::npos);
        CHECK(lift < park);
        CHECK(park < parked_m400);
        CHECK(parked_m400 < shot);
        CHECK(shot < dwell);
        CHECK(descent_before_shot == std::string::npos);

        const size_t next_block = gcode.find("; ESP32_TIMELAPSE_SMOOTH_BEGIN layer=", end + 1);
        const size_t resume_end = next_block == std::string::npos ? gcode.size() : next_block;
        const size_t first_extrusion = find_gcode_line(gcode, end + 1, resume_end, is_xy_extrusion);
        if (next_block == std::string::npos) {
            CHECK(first_extrusion == std::string::npos);
        } else {
            const size_t resume_xy = find_gcode_line(gcode, end + 1, resume_end, is_xy_move_without_extrusion);
            const size_t descent_before_resume_xy = find_gcode_line(
                gcode, end + 1, resume_xy == std::string::npos ? resume_end : resume_xy, is_z_only_move);
            const size_t restore_z = find_gcode_line(
                gcode, resume_xy == std::string::npos ? end + 1 : resume_xy, resume_end, is_z_only_move);
            REQUIRE(resume_xy != std::string::npos);
            REQUIRE(restore_z != std::string::npos);
            REQUIRE(first_extrusion != std::string::npos);
            CHECK(descent_before_resume_xy == std::string::npos);
            CHECK(resume_xy < restore_z);
            CHECK(restore_z < first_extrusion);
        }

        begin = end + 1;
    }
    REQUIRE(block_count > 0);
}

static void require_no_large_extrusion_after_final_layer(const std::string& gcode, double maximum_absolute_e)
{
    const size_t final_layer = gcode.rfind(";LAYER_CHANGE");
    const size_t machine_end = gcode.rfind("\n; filament end gcode");
    REQUIRE(final_layer != std::string::npos);
    REQUIRE(machine_end != std::string::npos);
    REQUIRE(final_layer < machine_end);

    size_t line_begin = final_layer;
    while (line_begin < machine_end) {
        const size_t line_end = std::min(gcode.find('\n', line_begin), machine_end);
        const std::string_view words = gcode_words(
            std::string_view(gcode.data() + line_begin, line_end - line_begin));
        double extrusion = 0.0;
        if ((words.rfind("G0 ", 0) == 0 || words.rfind("G1 ", 0) == 0) &&
            gcode_word_value(words, 'E', extrusion)) {
            INFO("Unexpected large final-layer extrusion: " << words);
            CHECK(std::abs(extrusion) <= maximum_absolute_e);
        }
        if (line_end == std::string::npos || line_end >= machine_end)
            break;
        line_begin = line_end + 1;
    }
}

static std::string_view layer_change_marker(const std::string& gcode)
{
    if (gcode.find(";LAYER_CHANGE") != std::string::npos)
        return ";LAYER_CHANGE";
    if (gcode.find("; CHANGE_LAYER") != std::string::npos)
        return "; CHANGE_LAYER";
    throw std::runtime_error("G-code has no recognized layer-change marker");
}

static size_t gcode_layer_count(const std::string& gcode)
{
    return count_occurrences(gcode, std::string(layer_change_marker(gcode)));
}

static DynamicPrintConfig esp32_timelapse_config(
    const char* timelapse_type,
    bool supports_esp32_timelapse = true,
    const char* gcode_flavor = "klipper")
{
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.set_deserialize_strict({
        { "layer_height", 0.2 },
        { "first_layer_height", 0.2 },
        { "first_layer_extrusion_width", 0 },
        { "start_gcode", "" },
        { "end_gcode", "" },
        { "machine_start_gcode", "" },
        { "machine_end_gcode", "" },
        { "gcode_flavor", gcode_flavor },
        { "nozzle_diameter", "0.4" },
        { "filament_diameter", "1.75" },
        { "supports_esp32_timelapse", supports_esp32_timelapse },
        { "esp32_timelapse_gcode", "ESP_TIMELAPSE_SHOT" },
        { "esp32_timelapse_park_x", 240.0 },
        { "esp32_timelapse_park_y", 240.0 },
        { "esp32_timelapse_travel_speed", 18000.0 },
        { "esp32_timelapse_dwell_ms", 2000 },
        { "timelapse_type", timelapse_type }
    });
    config.set_key_value("printable_area", new ConfigOptionPoints{ Vec2d(0.0, 0.0), Vec2d(256.0, 0.0), Vec2d(256.0, 256.0), Vec2d(0.0, 256.0) });
    config.set_key_value("printable_height", new ConfigOptionFloat(256.0));
    config.set_key_value("bed_mesh_min", new ConfigOptionPoint(Vec2d(0.0, 0.0)));
    config.set_key_value("bed_mesh_max", new ConfigOptionPoint(Vec2d(0.0, 0.0)));
    config.set_key_value("bed_mesh_probe_distance", new ConfigOptionPoint(Vec2d(0.0, 0.0)));
    config.set_key_value("overhang_fan_threshold",
        new ConfigOptionEnumsGeneric(&ConfigOptionEnum<OverhangFanThreshold>::get_enum_values(), 1, Overhang_threshold_bridge));
    config.set_key_value("z_hop_types",
        new ConfigOptionEnumsGeneric(&ConfigOptionEnum<ZHopType>::get_enum_values(), 1, ZHopType::zhtSlope));
    config.set_key_value("retract_lift_enforce",
        new ConfigOptionEnumsGeneric(&ConfigOptionEnum<RetractLiftEnforceType>::get_enum_values(), 1, RetractLiftEnforceType::rletAllSurfaces));
    return config;
}

static void configure_u1_tool_slots(DynamicPrintConfig& config, size_t slots)
{
    config.set_num_extruders(slots);
    std::string nozzle_diameters;
    std::string filament_diameters;
    std::string filament_colours;
    std::string purge_volumes;
    for (size_t index = 0; index < slots; ++index) {
        if (index != 0) {
            nozzle_diameters += ',';
            filament_diameters += ',';
            filament_colours += ',';
            purge_volumes += ',';
        }
        nozzle_diameters += "0.4";
        filament_diameters += "1.75";
        filament_colours += index % 2 == 0 ? "#000000" : "#ffffff";
        purge_volumes += "15";
    }
    config.set_deserialize_strict({
        { "nozzle_diameter", nozzle_diameters },
        { "filament_diameter", filament_diameters },
        { "filament_colour", filament_colours },
        { "filament_minimal_purge_on_wipe_tower", purge_volumes }
    });
}

static std::string slice_cubes_for_timelapse(
    const DynamicPrintConfig& config,
    const std::vector<int>& object_extruders,
    bool is_bambu_printer = false,
    const std::function<void(const Print&)>& process_inspector = {},
    double object_scale_z = 1.35,
    bool center_on_printable_area = false)
{
    Slic3r::Print print;
    print.is_BBL_printer() = is_bambu_printer;
    Slic3r::Model model;
    for (size_t object_idx = 0; object_idx < object_extruders.size(); ++object_idx) {
        ModelObject* object = model.add_object();
        object->name = "esp32_cube_" + std::to_string(object_idx) + ".stl";
        object->input_file = object->name;
        ModelVolume* volume = object->add_volume(Slic3r::Test::mesh(TestMesh::cube_20x20x20));
        object->config.set_key_value("extruder", new ConfigOptionInt(object_extruders[object_idx]));
        volume->config.set_key_value("extruder", new ConfigOptionInt(object_extruders[object_idx]));
        ModelInstance* instance = object->add_instance();
        instance->set_scaling_factor(Vec3d(1.35, 1.35, object_scale_z));
    }
    arrange_objects(model, InfiniteBed{}, ArrangeParams{ scaled(min_object_distance(config)) }, [](arrangement::ArrangePolygon&) {});
    if (center_on_printable_area) {
        const Points bed_shape = get_bed_shape(config);
        if (bed_shape.size() < 3)
            throw std::runtime_error("Resolved printer preset has no usable printable area");
        model.center_instances_around_point(unscale(Polygon(bed_shape).bounding_box().center()));
    }
    for (ModelObject* model_object : model.objects) {
        model_object->ensure_on_bed();
        print.auto_assign_extruders(model_object);
    }
    print.apply(model, config);
    const StringObjectException validation = print.validate();
    if (center_on_printable_area && !validation.string.empty())
        throw std::runtime_error("Resolved timelapse preset failed print validation: " + validation.string);
    print.set_status_silent();

    boost::filesystem::path path = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("esp32-timelapse-%%%%-%%%%-%%%%.gcode");
    print.process();
    if (process_inspector)
        process_inspector(print);
    Slic3r::GCode gcode_generator;
    Slic3r::GCodeProcessorResult result;
    result.reset();
    try {
        gcode_generator.do_export(&print, path.string().c_str(), &result, nullptr);
    } catch (...) {
        boost::filesystem::remove(path);
        throw;
    }
    std::string gcode;
    {
        std::ifstream stream(path.string());
        gcode.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }
    boost::filesystem::remove(path);
    return gcode;
}

static std::string slice_centered_cube_for_timelapse(
    const DynamicPrintConfig& config,
    bool is_bambu_printer = false,
    bool center_on_printable_area = false)
{
    return slice_cubes_for_timelapse(
        config, { 1 }, is_bambu_printer, {}, 1.35, center_on_printable_area);
}

static std::string validate_centered_cube_for_timelapse(const DynamicPrintConfig& config)
{
    Slic3r::Print print;
    Slic3r::Model model;
    ModelObject* object = model.add_object();
    object->name = "esp32_validation_cube.stl";
    ModelVolume* volume = object->add_volume(Slic3r::Test::mesh(TestMesh::cube_20x20x20));
    object->config.set_key_value("extruder", new ConfigOptionInt(1));
    volume->config.set_key_value("extruder", new ConfigOptionInt(1));
    object->add_instance();
    arrange_objects(model, InfiniteBed{}, ArrangeParams{ scaled(min_object_distance(config)) }, [](arrangement::ArrangePolygon&) {});
    object->ensure_on_bed();
    print.auto_assign_extruders(object);
    print.apply(model, config);
    return print.validate().string;
}

SCENARIO("ESP32 Timelapse Box Klipper timelapse G-code generation", "[PrintGCode][Timelapse][ESP32]")
{
    GIVEN("A Klipper printer with ESP32 Timelapse Box timelapse capability") {
        WHEN("timelapse is off") {
            DynamicPrintConfig config = esp32_timelapse_config("2");
            configure_u1_tool_slots(config, 4);
            config.set_deserialize_strict("enable_prime_tower", "0");
            config.set_deserialize_strict("time_lapse_gcode", "LEGACY_TIMELAPSE_FRAME");
            const std::string gcode = slice_centered_cube_for_timelapse(config);

            THEN("neither ESP32 Timelapse Box nor the legacy hook is emitted") {
                REQUIRE(count_occurrences(gcode, ";LAYER_CHANGE") == 135);
                REQUIRE(gcode.find("\nESP_TIMELAPSE_SHOT\n") == std::string::npos);
                REQUIRE(gcode.find("\nLEGACY_TIMELAPSE_FRAME\n") == std::string::npos);
                REQUIRE(gcode.find("; WIPE_TOWER_START") == std::string::npos);
                REQUIRE(gcode.find("; enable_prime_tower = 1") == std::string::npos);
            }
        }

        WHEN("timelapse is selected but the printer capability is disabled") {
            DynamicPrintConfig config = esp32_timelapse_config("0", false);
            config.set_deserialize_strict("time_lapse_gcode", "NATIVE_PROFILE_FRAME");
            const std::string gcode = slice_centered_cube_for_timelapse(config);

            THEN("the printer keeps its native behavior without adding ESP32 frames") {
                const size_t layers = count_occurrences(gcode, ";LAYER_CHANGE");
                CHECK(count_occurrences(gcode, "\nNATIVE_PROFILE_FRAME\n") == layers);
                CHECK(gcode.find("\nESP_TIMELAPSE_SHOT\n") == std::string::npos);
            }
        }

        WHEN("the ESP32 capability is enabled without a configured frame command") {
            DynamicPrintConfig config = esp32_timelapse_config("0");
            config.set_deserialize_strict("esp32_timelapse_gcode", "");

            THEN("validation blocks slicing with an actionable configuration error") {
                const std::string error = validate_centered_cube_for_timelapse(config);
                REQUIRE_FALSE(error.empty());
                CHECK(error.find("no frame command is configured") != std::string::npos);
            }
        }

        WHEN("the configured frame command contains comments only") {
            DynamicPrintConfig config = esp32_timelapse_config("0");
            config.set_deserialize_strict(
                "esp32_timelapse_gcode", "; disabled\n; ESP_TIMELAPSE_SHOT");

            THEN("validation rejects the non-executable camera configuration") {
                const std::string error = validate_centered_cube_for_timelapse(config);
                REQUIRE_FALSE(error.empty());
                CHECK(error.find("no frame command is configured") != std::string::npos);
            }
        }

        WHEN("the printer emits its native frame hook alongside ESP32") {
            DynamicPrintConfig config = esp32_timelapse_config("0");
            config.set_deserialize_strict("before_layer_change_gcode", "TIMELAPSE_TAKE_FRAME");
            const std::string gcode = slice_centered_cube_for_timelapse(config);

            THEN("native and external frames are both emitted once per layer") {
                const size_t layers = count_occurrences(gcode, ";LAYER_CHANGE");
                CHECK(count_occurrences(gcode, "\nTIMELAPSE_TAKE_FRAME\n") == layers);
                CHECK(count_occurrences(gcode, "\nESP_TIMELAPSE_SHOT\n") == layers);
            }
        }

        WHEN("the standard U1 timelapse mode is off") {
            DynamicPrintConfig config = esp32_timelapse_config("2", false);
            config.set_deserialize_strict(
                "before_layer_change_gcode",
                "{if timelapse_type != 2}\nNATIVE_U1_TIMELAPSE_FRAME\n{endif}");
            const std::string gcode = slice_centered_cube_for_timelapse(config);

            THEN("neither the native U1 hook nor ESP32 Timelapse Box emits a frame") {
                REQUIRE(gcode.find("\nNATIVE_U1_TIMELAPSE_FRAME\n") == std::string::npos);
                REQUIRE(gcode.find("\nESP_TIMELAPSE_SHOT\n") == std::string::npos);
            }
        }

        WHEN("a capability-disabled U1-style profile uses native timelapse") {
            DynamicPrintConfig config = esp32_timelapse_config("0", false);
            config.set_deserialize_strict(
                "before_layer_change_gcode",
                "{if timelapse_type != 2}\nNATIVE_U1_TIMELAPSE_FRAME\n{endif}");
            const std::string gcode = slice_centered_cube_for_timelapse(config);

            THEN("the native U1 frame hook remains active without ESP32 Timelapse Box output") {
                const size_t layers = count_occurrences(gcode, ";LAYER_CHANGE");
                REQUIRE(count_occurrences(gcode, "\nNATIVE_U1_TIMELAPSE_FRAME\n") == layers);
                REQUIRE(gcode.find("\nESP_TIMELAPSE_SHOT\n") == std::string::npos);
            }
        }

        WHEN("timelapse is selected for a non-Klipper G-code flavor") {
            const DynamicPrintConfig config = esp32_timelapse_config("0", true, "marlin");
            const std::string gcode = slice_centered_cube_for_timelapse(config);

            THEN("no ESP32 Timelapse Box shot command is emitted") {
                REQUIRE(gcode.find("\nESP_TIMELAPSE_SHOT\n") == std::string::npos);
            }
        }

        WHEN("traditional timelapse is selected") {
            DynamicPrintConfig config = esp32_timelapse_config("0");
            configure_u1_tool_slots(config, 4);
            config.set_deserialize_strict("enable_prime_tower", "0");
            config.set_deserialize_strict("time_lapse_gcode", "");
            config.set_deserialize_strict(
                "before_layer_change_gcode",
                "{if timelapse_type != 2}\nNATIVE_U1_TIMELAPSE_FRAME\n{endif}");
            const std::string gcode = slice_centered_cube_for_timelapse(config);

            THEN("one ESP32 Timelapse Box shot is emitted for each layer with debug boundaries") {
                const size_t layers = count_occurrences(gcode, ";LAYER_CHANGE");
                const size_t shots = count_occurrences(gcode, "\nESP_TIMELAPSE_SHOT\n");
                REQUIRE(layers == 135);
                REQUIRE(shots == layers);
                REQUIRE(count_occurrences(gcode, "; ESP32_TIMELAPSE_TRADITIONAL_BEGIN layer=") == layers);
                REQUIRE(count_occurrences(gcode, "; ESP32_TIMELAPSE_TRADITIONAL_END") == layers);
                REQUIRE(count_occurrences(gcode, "\nNATIVE_U1_TIMELAPSE_FRAME\n") == layers);
                REQUIRE(gcode.find("; WIPE_TOWER_START") == std::string::npos);
                REQUIRE(gcode.find("; enable_prime_tower = 1") == std::string::npos);
                require_shot_after_completed_layer(gcode);
                require_traditional_timelapse_timing_contract(gcode, 2000);
            }
        }

        WHEN("the receiver dwell is shorter than the ESP32 Timelapse Box trigger contract") {
            DynamicPrintConfig config = esp32_timelapse_config("0");
            config.set_key_value("esp32_timelapse_dwell_ms", new ConfigOptionInt(1999));

            THEN("validation rejects the unsafe interval") {
                const std::string error = validate_centered_cube_for_timelapse(config);
                REQUIRE_FALSE(error.empty());
                CHECK(error.find("at least 2000 ms") != std::string::npos);

                bool generation_rejected = false;
                try {
                    (void) slice_centered_cube_for_timelapse(config);
                } catch (const std::exception& generation_error) {
                    generation_rejected = true;
                    CHECK(std::string(generation_error.what()).find("at least 2000 ms") != std::string::npos);
                }
                CHECK(generation_rejected);
            }
        }

        WHEN("smooth timelapse park coordinates are outside the printable area") {
            DynamicPrintConfig config = esp32_timelapse_config("1");
            config.set_key_value("printable_area", new ConfigOptionPoints{
                Vec2d(0.0, 0.0), Vec2d(180.0, 0.0), Vec2d(180.0, 180.0), Vec2d(0.0, 180.0) });

            THEN("validation rejects the unsafe generic Klipper profile") {
                CHECK_FALSE(validate_centered_cube_for_timelapse(config).empty());
            }
        }

        WHEN("smooth timelapse park position enters a bed exclusion area") {
            DynamicPrintConfig config = esp32_timelapse_config("1");
            config.set_key_value("bed_exclude_area", new ConfigOptionPoints {
                Vec2d(230.0, 230.0), Vec2d(250.0, 230.0), Vec2d(250.0, 250.0), Vec2d(230.0, 250.0)
            });

            THEN("validation rejects the excluded park position") {
                const std::string error = validate_centered_cube_for_timelapse(config);
                REQUIRE_FALSE(error.empty());
                CHECK(error.find("explicit park position") != std::string::npos);
            }
        }

        WHEN("smooth timelapse park position overlaps the generated tower") {
            DynamicPrintConfig config = esp32_timelapse_config("1");
            config.set_deserialize_strict({
                { "esp32_timelapse_park_x", "20" },
                { "esp32_timelapse_park_y", "240" }
            });

            THEN("slicing rejects the tower collision") {
                bool rejected = false;
                try {
                    (void) slice_centered_cube_for_timelapse(config);
                } catch (const std::exception& error) {
                    rejected = true;
                    CHECK(std::string(error.what()).find("park position overlaps the prime tower") != std::string::npos);
                }
                CHECK(rejected);
            }
        }

        WHEN("smooth timelapse park position overlaps the model") {
            DynamicPrintConfig config = esp32_timelapse_config("1");
            config.set_deserialize_strict({
                { "esp32_timelapse_park_x", "128" },
                { "esp32_timelapse_park_y", "128" }
            });

            THEN("slicing rejects the model collision") {
                bool rejected = false;
                try {
                    (void) slice_centered_cube_for_timelapse(config, false, true);
                } catch (const std::exception& error) {
                    rejected = true;
                    CHECK(std::string(error.what()).find("park position overlaps the model") != std::string::npos);
                }
                CHECK(rejected);
            }
        }

        WHEN("smooth timelapse tower is outside the printable area") {
            DynamicPrintConfig config = esp32_timelapse_config("1");
            config.set_deserialize_strict({
                { "wipe_tower_x", "250" },
                { "wipe_tower_y", "250" }
            });

            THEN("validation rejects the invalid tower footprint") {
                const std::string error = validate_centered_cube_for_timelapse(config);
                REQUIRE_FALSE(error.empty());
                CHECK(error.find("prime tower inside the printable area") != std::string::npos);
            }
        }

        WHEN("smooth timelapse tower overlaps the model") {
            DynamicPrintConfig config = esp32_timelapse_config("1");
            config.set_deserialize_strict({
                { "wipe_tower_x", "5" },
                { "wipe_tower_y", "5" }
            });

            THEN("validation blocks the colliding stabilization tower") {
                const std::string error = validate_centered_cube_for_timelapse(config);
                REQUIRE_FALSE(error.empty());
                CHECK(error.find("prime tower overlaps the model") != std::string::npos);
            }
        }

        WHEN("smooth timelapse is combined with by-object printing") {
            DynamicPrintConfig config = esp32_timelapse_config("1");
            config.set_key_value("print_sequence", new ConfigOptionEnum<PrintSequence>(PrintSequence::ByObject));

            THEN("validation rejects the unsupported print sequence") {
                const std::string error = validate_centered_cube_for_timelapse(config);
                REQUIRE_FALSE(error.empty());
                CHECK(error.find("by object") != std::string::npos);
            }
        }

        WHEN("smooth timelapse is combined with spiral vase mode") {
            DynamicPrintConfig config = esp32_timelapse_config("1");
            config.set_deserialize_strict("spiral_mode", "1");

            THEN("validation rejects the unsupported tower combination") {
                const std::string error = validate_centered_cube_for_timelapse(config);
                REQUIRE_FALSE(error.empty());
                CHECK(error.find("spiral vase") != std::string::npos);
            }
        }

        WHEN("smooth timelapse has insufficient Z headroom") {
            DynamicPrintConfig config = esp32_timelapse_config("1");
            config.set_deserialize_strict("printable_height", "27.05");

            THEN("slicing stops instead of parking with reduced clearance") {
                bool rejected = false;
                try {
                    (void) slice_centered_cube_for_timelapse(config);
                } catch (const std::exception& error) {
                    rejected = true;
                    CHECK(std::string(error.what()).find("does not have enough Z clearance") != std::string::npos);
                }
                CHECK(rejected);
            }
        }

        WHEN("smooth timelapse has exactly the required Z headroom") {
            DynamicPrintConfig config = esp32_timelapse_config("1");
            config.set_deserialize_strict("printable_height", "27.4");
            const std::string gcode = slice_centered_cube_for_timelapse(config);

            THEN("the full configured lift is emitted without exceeding printable height") {
                CHECK(maximum_smooth_timelapse_z(gcode) <= 27.4 + EPSILON);
                CHECK(final_smooth_timelapse_lift(gcode) >= 0.4 - EPSILON);
            }
        }

        WHEN("smooth timelapse is selected for a single-tool print") {
            DynamicPrintConfig config = esp32_timelapse_config("1");
            configure_u1_tool_slots(config, 4);
            config.set_deserialize_strict("enable_prime_tower", "0");
            Points generated_tower_footprint;
            const std::string gcode = slice_cubes_for_timelapse(
                config, { 1 }, false,
                [&generated_tower_footprint](const Print& print) {
                    generated_tower_footprint = print.first_layer_wipe_tower_corners();
                });

            THEN("a real stabilization tower is extruded on every captured layer") {
                const size_t layers = count_occurrences(gcode, ";LAYER_CHANGE");
                const size_t shots = count_occurrences(gcode, "\nESP_TIMELAPSE_SHOT\n");
                REQUIRE(layers == 135);
                REQUIRE(shots == layers);
                REQUIRE(count_occurrences(gcode, "; ESP32_TIMELAPSE_SMOOTH_BEGIN layer=") == layers);
                REQUIRE(gcode.find("; ESP32_TIMELAPSE_SMOOTH_BEGIN layer=") != std::string::npos);
                REQUIRE(gcode.find("SAVE_GCODE_STATE NAME=ESP32_TIMELAPSE_SMOOTH") != std::string::npos);
                REQUIRE(gcode.find("G1 X240 Y240 F18000") != std::string::npos);
                REQUIRE(gcode.find("\nESP_TIMELAPSE_SHOT\n") != std::string::npos);
                REQUIRE(gcode.find("G4 P2000") != std::string::npos);
                REQUIRE(gcode.find("; ESP32_TIMELAPSE_SMOOTH_END") != std::string::npos);
                REQUIRE(gcode.find("; WIPE_TOWER_START") != std::string::npos);
                REQUIRE(gcode.find("; enable_prime_tower = 1") != std::string::npos);
                require_one_shot_per_layer(gcode);
                require_shot_after_completed_layer(gcode);
                require_real_wipe_tower_before_each_shot(gcode);
                require_first_layer_tower_moves_inside_footprint(gcode, std::move(generated_tower_footprint));
                require_safe_smooth_timelapse_motion(gcode);
            }
        }

        WHEN("smooth timelapse is selected with a wipe tower") {
            DynamicPrintConfig config = esp32_timelapse_config("1");
            configure_u1_tool_slots(config, 4);
            config.set_deserialize_strict("enable_prime_tower", "1");
            const std::string gcode = slice_cubes_for_timelapse(config, { 1, 2 });

            THEN("each completed layer is captured once and final purge precedes the final frame") {
                const size_t wipe_tower = min_position(
                    min_position(gcode.find(";TYPE:Prime tower"), gcode.find(";TYPE:WipeTower")),
                    gcode.find("; FEATURE: WipeTower"));
                REQUIRE(wipe_tower != std::string::npos);

                const size_t smooth_begin = gcode.find("; ESP32_TIMELAPSE_SMOOTH_BEGIN layer=", wipe_tower);
                REQUIRE(smooth_begin != std::string::npos);
                const size_t wipe_tower_before = min_position(
                    min_position(gcode.rfind(";TYPE:Prime tower", smooth_begin), gcode.rfind(";TYPE:WipeTower", smooth_begin)),
                    gcode.rfind("; FEATURE: WipeTower", smooth_begin));
                REQUIRE(wipe_tower_before != std::string::npos);
                REQUIRE(count_occurrences(gcode, "; ESP32_TIMELAPSE_SMOOTH_BEGIN layer=") ==
                    count_occurrences(gcode, "\nESP_TIMELAPSE_SHOT\n"));
                const size_t layer_count = count_occurrences(gcode, ";LAYER_CHANGE");
                REQUIRE(layer_count > 0);
                REQUIRE(count_occurrences(gcode, "\nESP_TIMELAPSE_SHOT\n") == layer_count);
                require_one_shot_per_layer(gcode);
                require_shot_after_completed_layer(gcode);
                require_real_wipe_tower_before_each_shot(gcode);
                require_safe_smooth_timelapse_motion(gcode);

                const std::string final_marker = "; ESP32_TIMELAPSE_SMOOTH_BEGIN layer=" +
                                                 std::to_string(layer_count - 1);
                const size_t final_smooth_begin = gcode.rfind(final_marker);
                const size_t final_purge_end     = gcode.rfind("; CP TOOLCHANGE END", final_smooth_begin);
                REQUIRE(final_smooth_begin != std::string::npos);
                REQUIRE(final_purge_end != std::string::npos);
                CHECK(final_purge_end < final_smooth_begin);
                CHECK(gcode.find("; CP TOOLCHANGE START", final_smooth_begin) == std::string::npos);
            }
        }

        WHEN("Bambu legacy traditional timelapse is generated") {
            DynamicPrintConfig config = esp32_timelapse_config("0", false, "marlin");
            config.set_deserialize_strict({
                { "printer_structure", "i3" },
                { "enable_prime_tower", true },
                { "time_lapse_gcode", "BAMBU_TIMELAPSE_FRAME" }
            });
            const std::string gcode = slice_centered_cube_for_timelapse(config, true);

            THEN("the existing Bambu hook runs once per layer without ESP32 Timelapse Box output") {
                const size_t layers = count_occurrences(gcode, "; CHANGE_LAYER");
                CHECK(count_occurrences(gcode, "\nBAMBU_TIMELAPSE_FRAME\n") == layers);
                CHECK(gcode.find("\nESP_TIMELAPSE_SHOT\n") == std::string::npos);
            }
        }

        WHEN("Bambu legacy smooth timelapse is generated") {
            DynamicPrintConfig config = esp32_timelapse_config("1", false, "marlin");
            config.set_deserialize_strict({
                { "printer_structure", "i3" },
                { "enable_prime_tower", true },
                { "time_lapse_gcode", "BAMBU_TIMELAPSE_FRAME" }
            });
            const std::string gcode = slice_centered_cube_for_timelapse(config, true);

            THEN("the existing Bambu smooth hook and wipe tower behavior remain active") {
                const size_t layers = count_occurrences(gcode, "; CHANGE_LAYER");
                CHECK(count_occurrences(gcode, "\nBAMBU_TIMELAPSE_FRAME\n") == layers);
                CHECK(gcode.find("\nESP_TIMELAPSE_SHOT\n") == std::string::npos);
                CHECK(dynamic_print_config_uses_legacy_smooth_timelapse(config));
            }
        }
    }
}

TEST_CASE("Real U1 Off Traditional and multi-tool use additive ESP32 semantics",
          "[PrintGCode][Timelapse][ESP32][PresetBundle][U1]")
{
    auto make_config = [](size_t filament_count = 1) {
        DynamicPrintConfig config = resolved_preset_config(
            "Snapmaker", "Snapmaker U1 (0.4 nozzle)",
            "0.20 Standard @Snapmaker U1 (0.4 nozzle)", "Snapmaker PLA Basic @U1", filament_count);
        REQUIRE(config.opt_bool("supports_esp32_timelapse"));
        REQUIRE(config.opt_bool("single_extruder_multi_material") == false);
        REQUIRE(config.opt_bool("purge_in_prime_tower") == false);
        return config;
    };

    DynamicPrintConfig off = make_config();
    off.set_deserialize_strict({ { "timelapse_type", "2" }, { "enable_prime_tower", false } });
    const std::string off_gcode = slice_centered_cube_for_timelapse(off, false, true);
    REQUIRE(gcode_layer_count(off_gcode) == 135);
    REQUIRE(off_gcode.find("\nTIMELAPSE_TAKE_FRAME\n") == std::string::npos);
    REQUIRE(off_gcode.find("\nESP_TIMELAPSE_SHOT\n") == std::string::npos);
    REQUIRE(off_gcode.find("\nESP32_TIMELAPSE_SHOT\n") == std::string::npos);
    REQUIRE(off_gcode.find("; WIPE_TOWER_START") == std::string::npos);
    REQUIRE(off_gcode.find("; enable_prime_tower = 1") == std::string::npos);

    DynamicPrintConfig traditional = make_config();
    traditional.set_deserialize_strict({ { "timelapse_type", "0" }, { "enable_prime_tower", false } });
    const std::string traditional_gcode = slice_centered_cube_for_timelapse(traditional, false, true);
    REQUIRE(gcode_layer_count(traditional_gcode) == 135);
    REQUIRE(count_occurrences(traditional_gcode, "\nTIMELAPSE_TAKE_FRAME\n") == 135);
    REQUIRE(count_occurrences(traditional_gcode, "\nESP_TIMELAPSE_SHOT\n") == 135);
    REQUIRE(traditional_gcode.find("\nESP32_TIMELAPSE_SHOT\n") == std::string::npos);
    REQUIRE(traditional_gcode.find("; WIPE_TOWER_START") == std::string::npos);
    REQUIRE(traditional_gcode.find("; enable_prime_tower = 1") == std::string::npos);
    require_traditional_timelapse_timing_contract(traditional_gcode, 2000);

    DynamicPrintConfig multi_tool = make_config(4);
    multi_tool.set_deserialize_strict({ { "timelapse_type", "1" }, { "enable_prime_tower", true } });
    const std::string multi_tool_gcode = slice_cubes_for_timelapse(
        multi_tool, { 1, 3, 4 }, false, {}, 1.35, true);
    REQUIRE(gcode_layer_count(multi_tool_gcode) == 135);
    REQUIRE(count_occurrences(multi_tool_gcode, "\nTIMELAPSE_TAKE_FRAME\n") == 135);
    REQUIRE(count_occurrences(multi_tool_gcode, "\nESP_TIMELAPSE_SHOT\n") == 135);
    REQUIRE(multi_tool_gcode.find("\nESP32_TIMELAPSE_SHOT\n") == std::string::npos);
    REQUIRE(multi_tool_gcode.find("; single_extruder_multi_material = 0") != std::string::npos);
    REQUIRE(multi_tool_gcode.find("; purge_in_prime_tower = 0") != std::string::npos);
    require_one_shot_per_layer(multi_tool_gcode);
    require_real_wipe_tower_before_each_shot(multi_tool_gcode);
    require_safe_smooth_timelapse_motion(multi_tool_gcode);
    require_no_large_extrusion_after_final_layer(multi_tool_gcode, 20.0);
}

TEST_CASE("Real U1 feature filament assignments reach G-code",
          "[PrintGCode][MixedNozzle][FeatureFilament][U1]")
{
    DynamicPrintConfig config = resolved_preset_config(
        "Snapmaker", "Snapmaker U1 (0.4 nozzle)",
            "0.20 Standard @Snapmaker U1 (0.4 nozzle)", "Snapmaker PLA Basic @U1", 4);
    config.set_deserialize_strict({
        { "timelapse_type", "2" },
        { "enable_prime_tower", false },
        { "mixed_nozzle_mode", "same_layer" },
        { "outer_wall_filament", 2 },
        { "wall_filament", 4 },
        { "sparse_infill_filament", 4 },
        { "solid_infill_filament", 4 },
        { "wall_loops", 3 },
        { "sparse_infill_density", 15 }
    });

    const std::string gcode = slice_cubes_for_timelapse(
        config, { 2 }, false, {}, 1.35, true);
    const auto usage = feature_tool_usage(gcode);
    const auto outer = static_cast<size_t>(FeatureToolRole::OuterWall);
    const auto inner = static_cast<size_t>(FeatureToolRole::InnerWall);
    const auto sparse = static_cast<size_t>(FeatureToolRole::SparseInfill);
    const auto solid = static_cast<size_t>(FeatureToolRole::SolidInfill);

    REQUIRE(usage[outer][1]);
    REQUIRE(usage[inner][3]);
    REQUIRE(usage[sparse][3]);
    REQUIRE(usage[solid][3]);
    for (size_t tool = 0; tool < 4; ++tool) {
        if (tool != 1)
            CHECK_FALSE(usage[outer][tool]);
        if (tool != 3) {
            CHECK_FALSE(usage[inner][tool]);
            CHECK_FALSE(usage[sparse][tool]);
            CHECK_FALSE(usage[solid][tool]);
        }
    }
}

TEST_CASE("Real U1 mixed nozzle mode combines inner features every two fine layers",
          "[PrintGCode][MixedNozzle][MixedLayer][U1]")
{
    DynamicPrintConfig config = resolved_preset_config(
        "Snapmaker", "Snapmaker U1 (0.4 nozzle)",
        "0.20 Standard @Snapmaker U1 (0.4 nozzle)", "Snapmaker PLA Basic @U1", 4);
    config.set_deserialize_strict({
        { "timelapse_type", "2" },
        { "enable_prime_tower", false },
        { "layer_height", 0.1 },
        { "initial_layer_print_height", 0.1 },
        { "outer_wall_line_width", 0.22 },
        { "inner_wall_line_width", 0.42 },
        { "sparse_infill_line_width", 0.45 },
        { "internal_solid_infill_line_width", 0.42 },
        { "mixed_nozzle_mode", "mixed_layer" },
        { "mixed_nozzle_sparse_infill_combination", true },
        { "mixed_nozzle_inner_wall_combination", true },
        { "mixed_nozzle_internal_solid_infill_combination", false },
        { "mixed_nozzle_auto_coarse_layer_height", false },
        { "mixed_nozzle_coarse_layer_height", 0.2 },
        { "mixed_nozzle_auto_layer_height_ratio", false },
        { "mixed_nozzle_layer_height_ratio", 2 },
        { "outer_wall_filament", 1 },
        { "wall_filament", 2 },
        { "sparse_infill_filament", 2 },
        { "solid_infill_filament", 2 },
        { "wall_loops", 3 },
        { "top_shell_layers", 1 },
        { "bottom_shell_layers", 1 },
        { "sparse_infill_density", 15 }
    });
    config.option<ConfigOptionFloats>("nozzle_diameter")->values = { 0.2, 0.4, 0.4, 0.4 };
    config.option<ConfigOptionFloats>("max_layer_height")->values = { 0.16, 0.32, 0.32, 0.32 };

    const std::string gcode = slice_cubes_for_timelapse(
        config, { 2 }, false, {}, 0.1, true);
    const auto usage = feature_tool_usage(gcode);
    const auto zs = feature_layer_zs(gcode);
    const auto outer = static_cast<size_t>(FeatureToolRole::OuterWall);
    const auto inner = static_cast<size_t>(FeatureToolRole::InnerWall);
    const auto sparse = static_cast<size_t>(FeatureToolRole::SparseInfill);

    REQUIRE(usage[outer][0]);
    REQUIRE(usage[inner][1]);
    REQUIRE(usage[sparse][1]);
    CHECK_FALSE(usage[outer][1]);
    CHECK_FALSE(usage[inner][0]);
    CHECK_FALSE(usage[sparse][0]);

    REQUIRE(zs[outer].size() >= 15);
    REQUIRE(zs[inner].size() >= 6);
    REQUIRE(zs[sparse].size() >= 4);
    CHECK(feature_z_delta_count(zs[outer], 0.1) > feature_z_delta_count(zs[outer], 0.2));
    CHECK(feature_z_delta_count(zs[inner], 0.2) > feature_z_delta_count(zs[inner], 0.1));
    CHECK(feature_z_delta_count(zs[sparse], 0.2) > feature_z_delta_count(zs[sparse], 0.1));
}

SCENARIO( "PrintGCode basic functionality", "[PrintGCode]") {
    GIVEN("A default configuration and a print test object") {
        WHEN("the output is executed with no support material") {
            Slic3r::Print print;
            Slic3r::Model model;
            Slic3r::Test::init_print({TestMesh::cube_20x20x20}, print, model, {
                { "layer_height",					0.2 },
                { "first_layer_height",				0.2 },
                { "first_layer_extrusion_width",	0 },
                { "gcode_comments",					true },
                { "start_gcode",					"" }
                });
            std::string gcode = Slic3r::Test::gcode(print);
            THEN("Some text output is generated.") {
                REQUIRE(gcode.size() > 0);
            }
            THEN("Exported text contains slic3r version") {
                REQUIRE(gcode.find(SLIC3R_VERSION) != std::string::npos);
            }
            //THEN("Exported text contains git commit id") {
            //    REQUIRE(gcode.find("; Git Commit") != std::string::npos);
            //    REQUIRE(gcode.find(SLIC3R_BUILD_ID) != std::string::npos);
            //}
            THEN("Exported text contains extrusion statistics.") {
                REQUIRE(gcode.find("; external perimeters extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; perimeters extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; infill extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; solid infill extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; top infill extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; support material extrusion width") == std::string::npos);
                REQUIRE(gcode.find("; first layer extrusion width") == std::string::npos);
            }
            THEN("Exported text does not contain cooling markers (they were consumed)") {
                REQUIRE(gcode.find(";_EXTRUDE_SET_SPEED") == std::string::npos);
            }

            THEN("GCode preamble is emitted.") {
                REQUIRE(gcode.find("G21 ; set units to millimeters") != std::string::npos);
            }

            THEN("Config options emitted for print config, default region config, default object config") {
                REQUIRE(gcode.find("; first_layer_temperature") != std::string::npos);
                REQUIRE(gcode.find("; layer_height") != std::string::npos);
                REQUIRE(gcode.find("; fill_density") != std::string::npos);
            }
            THEN("Infill is emitted.") {
                std::smatch has_match;
                REQUIRE(std::regex_search(gcode, has_match, infill_regex));
            }
            THEN("Perimeters are emitted.") {
				std::smatch has_match;
                REQUIRE(std::regex_search(gcode, has_match, perimeters_regex));
            }
            THEN("Skirt is emitted.") {
                std::smatch has_match;
                REQUIRE(std::regex_search(gcode, has_match, skirt_regex));
            }
            THEN("final Z height is 20mm") {
                double final_z = 0.0;
                GCodeReader reader;
                reader.apply_config(print.config());
                reader.parse_buffer(gcode, [&final_z] (GCodeReader& self, const GCodeReader::GCodeLine& line) {
                    final_z = std::max<double>(final_z, static_cast<double>(self.z())); // record the highest Z point we reach
                });
                REQUIRE(final_z == Approx(20.));
            }
        }
        WHEN("output is executed with complete objects and two differently-sized meshes") {
            Slic3r::Print print;
            Slic3r::Model model;
            Slic3r::Test::init_print({TestMesh::cube_20x20x20,TestMesh::cube_20x20x20}, print, model, {
                { "first_layer_extrusion_width",    0 },
                { "first_layer_height",             0.3 },
                { "layer_height",                   0.2 },
                { "support_material",               false },
                { "raft_layers",                    0 },
                { "complete_objects",               true },
                { "gcode_comments",                 true },
                { "between_objects_gcode",          "; between-object-gcode" }
                });
            std::string gcode = Slic3r::Test::gcode(print);
            THEN("Some text output is generated.") {
                REQUIRE(gcode.size() > 0);
            }
            THEN("Infill is emitted.") {
                std::smatch has_match;
                REQUIRE(std::regex_search(gcode, has_match, infill_regex));
            }
            THEN("Perimeters are emitted.") {
                std::smatch has_match;
                REQUIRE(std::regex_search(gcode, has_match, perimeters_regex));
            }
            THEN("Skirt is emitted.") {
                std::smatch has_match;
                REQUIRE(std::regex_search(gcode, has_match, skirt_regex));
            }
            THEN("Between-object-gcode is emitted.") {
                REQUIRE(gcode.find("; between-object-gcode") != std::string::npos);
            }
            THEN("final Z height is 20.1mm") {
                double final_z = 0.0;
                GCodeReader reader;
                reader.apply_config(print.config());
                reader.parse_buffer(gcode, [&final_z] (GCodeReader& self, const GCodeReader::GCodeLine& line) {
                    final_z = std::max(final_z, static_cast<double>(self.z())); // record the highest Z point we reach
                });
                REQUIRE(final_z == Approx(20.1));
            }
            THEN("Z height resets on object change") {
                double final_z = 0.0;
                bool reset = false;
                GCodeReader reader;
                reader.apply_config(print.config());
                reader.parse_buffer(gcode, [&final_z, &reset] (GCodeReader& self, const GCodeReader::GCodeLine& line) {
                    if (final_z > 0 && std::abs(self.z() - 0.3) < 0.01 ) { // saw higher Z before this, now it's lower
                        reset = true;
                    } else {
                        final_z = std::max(final_z, static_cast<double>(self.z())); // record the highest Z point we reach
                    }
                });
                REQUIRE(reset == true);
            }
            THEN("Shorter object is printed before taller object.") {
                double final_z = 0.0;
                bool reset = false;
                GCodeReader reader;
                reader.apply_config(print.config());
                reader.parse_buffer(gcode, [&final_z, &reset] (GCodeReader& self, const GCodeReader::GCodeLine& line) {
                    if (final_z > 0 && std::abs(self.z() - 0.3) < 0.01 ) { 
                        reset = (final_z > 20.0);
                    } else {
                        final_z = std::max(final_z, static_cast<double>(self.z())); // record the highest Z point we reach
                    }
                });
                REQUIRE(reset == true);
            }
        }
        WHEN("the output is executed with support material") {
            std::string gcode = ::Test::slice({TestMesh::cube_20x20x20}, {
                { "first_layer_extrusion_width",    0 },
                { "support_material",               true },
                { "raft_layers",                    3 },
                { "gcode_comments",                 true }
                });
            THEN("Some text output is generated.") {
                REQUIRE(gcode.size() > 0);
            }
            THEN("Exported text contains extrusion statistics.") {
                REQUIRE(gcode.find("; external perimeters extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; perimeters extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; infill extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; solid infill extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; top infill extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; support material extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; first layer extrusion width") == std::string::npos);
            }
            THEN("Raft is emitted.") {
                REQUIRE(gcode.find("; raft") != std::string::npos);
            }
        }
        WHEN("the output is executed with a separate first layer extrusion width") {
			std::string gcode = ::Test::slice({ TestMesh::cube_20x20x20 }, {
                { "first_layer_extrusion_width", "0.5" }
                });
            THEN("Some text output is generated.") {
                REQUIRE(gcode.size() > 0);
            }
            THEN("Exported text contains extrusion statistics.") {
                REQUIRE(gcode.find("; external perimeters extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; perimeters extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; infill extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; solid infill extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; top infill extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; support material extrusion width") == std::string::npos);
                REQUIRE(gcode.find("; first layer extrusion width") != std::string::npos);
            }
        }
        WHEN("Cooling is enabled and the fan is disabled.") {
			std::string gcode = ::Test::slice({ TestMesh::cube_20x20x20 }, {
				{ "cooling",                    true },
                { "disable_fan_first_layers",   5 }
                });
            THEN("GCode to disable fan is emitted."){
                REQUIRE(gcode.find("M107") != std::string::npos);
            }
        }
        WHEN("end_gcode exists with layer_num and layer_z") {
			std::string gcode = ::Test::slice({ TestMesh::cube_20x20x20 }, {
				{ "end_gcode",              "; Layer_num [layer_num]\n; Layer_z [layer_z]" },
                { "layer_height",           0.1 },
                { "first_layer_height",     0.1 }
                });
            THEN("layer_num and layer_z are processed in the end gcode") {
                REQUIRE(gcode.find("; Layer_num 199") != std::string::npos);
                REQUIRE(gcode.find("; Layer_z 20") != std::string::npos);
            }
        }
        WHEN("current_extruder exists in start_gcode") {
            {
				std::string gcode = ::Test::slice({ TestMesh::cube_20x20x20 }, {
					{ "start_gcode", "; Extruder [current_extruder]" }
                });
                THEN("current_extruder is processed in the start gcode and set for first extruder") {
                    REQUIRE(gcode.find("; Extruder 0") != std::string::npos);
                }
            }
			{
                DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
                config.set_num_extruders(4);
                config.set_deserialize_strict({
                    { "start_gcode",                    "; Extruder [current_extruder]" },
                    { "infill_extruder",                2 },
                    { "solid_infill_extruder",          2 },
                    { "perimeter_extruder",             2 },
                    { "support_material_extruder",      2 },
                    { "support_material_interface_extruder", 2 }
                });
                std::string gcode = Slic3r::Test::slice({TestMesh::cube_20x20x20}, config);
                THEN("current_extruder is processed in the start gcode and set for second extruder") {
                    REQUIRE(gcode.find("; Extruder 1") != std::string::npos);
                }
            }
        }

        WHEN("layer_num represents the layer's index from z=0") {
			std::string gcode = ::Test::slice({ TestMesh::cube_20x20x20, TestMesh::cube_20x20x20 }, {
				{ "complete_objects",               true },
                { "gcode_comments",                 true },
                { "layer_gcode",                    ";Layer:[layer_num] ([layer_z] mm)" },
                { "layer_height",                   0.1 },
                { "first_layer_height",             0.1 }
                });
			// End of the 1st object.
            std::string token = ";Layer:199 ";
			size_t pos = gcode.find(token);
			THEN("First and second object last layer is emitted") {
				// First object
				REQUIRE(pos != std::string::npos);
				pos += token.size();
				REQUIRE(pos < gcode.size());
				double z = 0;
				REQUIRE((sscanf(gcode.data() + pos, "(%lf mm)", &z) == 1));
				REQUIRE(z == Approx(20.));
				// Second object
				pos = gcode.find(";Layer:399 ", pos);
				REQUIRE(pos != std::string::npos);
				pos += token.size();
				REQUIRE(pos < gcode.size());
				REQUIRE((sscanf(gcode.data() + pos, "(%lf mm)", &z) == 1));
				REQUIRE(z == Approx(20.));
			}
        }
    }
}
