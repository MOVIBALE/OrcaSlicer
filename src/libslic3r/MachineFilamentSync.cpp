#include "MachineFilamentSync.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace Slic3r {
namespace {

static std::string trim_copy(std::string value)
{
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

static std::string json_string_at(const nlohmann::json& object, const char* key, size_t index, const std::string& fallback = {})
{
    if (!object.is_object() || !object.contains(key) || !object[key].is_array() || index >= object[key].size())
        return fallback;

    const nlohmann::json& value = object[key][index];
    if (value.is_string())
        return value.get<std::string>();
    if (value.is_number_integer())
        return std::to_string(value.get<long long>());
    if (value.is_number_unsigned())
        return std::to_string(value.get<unsigned long long>());
    if (value.is_number_float()) {
        std::ostringstream ss;
        ss << value.get<double>();
        return ss.str();
    }

    return fallback;
}

static bool json_bool_at(const nlohmann::json& object, const char* key, size_t index, bool fallback)
{
    if (!object.is_object() || !object.contains(key) || !object[key].is_array() || index >= object[key].size())
        return fallback;

    const nlohmann::json& value = object[key][index];
    if (value.is_boolean())
        return value.get<bool>();
    if (value.is_number_integer())
        return value.get<int>() != 0;

    return fallback;
}

static const nlohmann::json* find_status_object(const nlohmann::json& response)
{
    const nlohmann::json* root = &response;
    if (root->is_object() && root->contains("result"))
        root = &(*root)["result"];
    if (root->is_object() && root->contains("status"))
        return &(*root)["status"];
    return root->is_object() ? root : nullptr;
}

static std::string format_nozzle_diameter(double diameter)
{
    if (!std::isfinite(diameter) || diameter <= 0.0)
        return {};

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << diameter;
    std::string out = ss.str();
    while (!out.empty() && out.back() == '0')
        out.pop_back();
    if (!out.empty() && out.back() == '.')
        out.pop_back();
    return out;
}

static std::string nozzle_name_for_index(size_t index)
{
    return index == 0 ? "extruder" : "extruder" + std::to_string(index);
}

static std::string nozzle_diameter_for_slot(const nlohmann::json& status, const nlohmann::json& print_config, size_t index)
{
    const std::string nozzle_key = nozzle_name_for_index(index);
    if (status.is_object() && status.contains(nozzle_key) && status[nozzle_key].is_object()
        && status[nozzle_key].contains("nozzle_diameter")) {
        const nlohmann::json& value = status[nozzle_key]["nozzle_diameter"];
        if (value.is_number())
            return format_nozzle_diameter(value.get<double>());
        if (value.is_string())
            return trim_copy(value.get<std::string>());
    }

    return json_string_at(print_config, "nozzle_diameters", index);
}

static std::vector<size_t> logical_to_physical_map(const nlohmann::json& print_config, size_t slot_count)
{
    std::vector<size_t> map(slot_count);
    for (size_t i = 0; i < slot_count; ++i)
        map[i] = i;

    if (!print_config.is_object() || !print_config.contains("extruder_map_table") || !print_config["extruder_map_table"].is_array())
        return map;

    const nlohmann::json& table = print_config["extruder_map_table"];
    for (size_t logical_index = 0; logical_index < slot_count && logical_index < table.size(); ++logical_index) {
        const nlohmann::json& value = table[logical_index];
        if (!value.is_number_integer())
            continue;

        const int physical_index = value.get<int>();
        if (physical_index >= 0 && static_cast<size_t>(physical_index) < slot_count)
            map[logical_index] = static_cast<size_t>(physical_index);
    }

    return map;
}

static bool is_none_subtype(const std::string& subtype)
{
    std::string upper = subtype;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return char(std::toupper(c)); });
    return upper.empty() || upper == "NONE";
}

static std::string machine_filament_display_name(std::string vendor, std::string type, std::string subtype)
{
    vendor  = trim_copy(std::move(vendor));
    type    = trim_copy(std::move(type));
    subtype = trim_copy(std::move(subtype));

    if (vendor.empty())
        vendor = "Generic";
    if (type.empty())
        return vendor;

    if (type == "TPU")
        return vendor + " " + type + ((subtype == "95A HF") ? " " + subtype : "");
    if (subtype == "Support")
        return vendor + " Support For " + type;
    return vendor + " " + type + (is_none_subtype(subtype) ? "" : " " + subtype);
}

static bool is_hex_string(const std::string& value)
{
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isxdigit(c) != 0;
    });
}

static std::string color_from_hex_rgba(std::string value)
{
    value = trim_copy(std::move(value));
    if (!value.empty() && value.front() == '#')
        value.erase(value.begin());
    if (value.size() >= 6 && is_hex_string(value))
        return "#" + value.substr(0, 6);
    return "#FFFFFF";
}

static std::string color_from_number(unsigned long long value)
{
    std::ostringstream ss;
    ss << "#" << std::uppercase << std::setfill('0') << std::setw(6) << std::hex << (value & 0x00FFFFFF);
    return ss.str();
}

static std::string color_for_slot(const nlohmann::json& print_config, size_t index)
{
    if (print_config.is_object() && print_config.contains("filament_color_rgba") && print_config["filament_color_rgba"].is_array()
        && index < print_config["filament_color_rgba"].size()) {
        const nlohmann::json& value = print_config["filament_color_rgba"][index];
        if (value.is_string())
            return color_from_hex_rgba(value.get<std::string>());
    }

    if (print_config.is_object() && print_config.contains("filament_color") && print_config["filament_color"].is_array()
        && index < print_config["filament_color"].size()) {
        const nlohmann::json& value = print_config["filament_color"][index];
        if (value.is_number_unsigned())
            return color_from_number(value.get<unsigned long long>());
        if (value.is_number_integer())
            return color_from_number(static_cast<unsigned long long>(value.get<long long>()));
        if (value.is_string())
            return color_from_hex_rgba(value.get<std::string>());
    }

    return "#FFFFFF";
}

} // namespace

std::vector<MachineFilamentSyncSlot> build_machine_filament_sync_slots(const nlohmann::json& response)
{
    const nlohmann::json* status = find_status_object(response);
    if (status == nullptr || !status->is_object() || !status->contains("print_task_config")
        || !(*status)["print_task_config"].is_object())
        return {};

    const nlohmann::json& print_config = (*status)["print_task_config"];
    if (!print_config.contains("filament_type") || !print_config["filament_type"].is_array())
        return {};

    const size_t slot_count = print_config["filament_type"].size();
    const std::vector<size_t> extruder_map = logical_to_physical_map(print_config, slot_count);

    std::vector<MachineFilamentSyncSlot> slots;
    slots.reserve(slot_count);

    for (size_t i = 0; i < slot_count; ++i) {
        const size_t physical_index = extruder_map[i];
        MachineFilamentSyncSlot slot;
        slot.slot_index      = i;
        slot.physical_index  = physical_index;
        slot.filament_type   = trim_copy(json_string_at(print_config, "filament_type", physical_index));
        slot.display_name    = machine_filament_display_name(
            json_string_at(print_config, "filament_vendor", physical_index, "Generic"),
            slot.filament_type,
            json_string_at(print_config, "filament_sub_type", physical_index));
        slot.color           = color_for_slot(print_config, physical_index);
        slot.nozzle_diameter = nozzle_diameter_for_slot(*status, print_config, physical_index);
        slot.exists          = json_bool_at(print_config, "filament_exist", physical_index, true);
        slots.emplace_back(std::move(slot));
    }

    return slots;
}

} // namespace Slic3r
