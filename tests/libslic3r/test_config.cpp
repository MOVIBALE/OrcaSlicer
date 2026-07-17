#include <catch2/catch.hpp>

#include "libslic3r/PrintConfig.hpp"
#include "libslic3r/LocalesUtils.hpp"
#include "libslic3r/Preset.hpp"

#include <boost/filesystem.hpp>
#include <nlohmann/json.hpp>

#include <fstream>

#include <cereal/types/polymorphic.hpp>
#include <cereal/types/string.hpp> 
#include <cereal/types/vector.hpp> 
#include <cereal/archives/binary.hpp>

using namespace Slic3r;

SCENARIO("Generic config validation performs as expected.", "[Config]") {
    GIVEN("A config generated from default options") {
        Slic3r::DynamicPrintConfig config = Slic3r::DynamicPrintConfig::full_print_config();
        WHEN( "perimeter_extrusion_width is set to 250%, a valid value") {
            config.set_deserialize_strict("perimeter_extrusion_width", "250%");
            THEN( "The config is read as valid.") {
                REQUIRE(config.validate().empty());
            }
        }
        WHEN( "perimeter_extrusion_width is set to -10, an invalid value") {
            config.set("perimeter_extrusion_width", -10);
            THEN( "Validate returns error") {
                REQUIRE(! config.validate().empty());
            }
        }

        WHEN( "perimeters is set to -10, an invalid value") {
            config.set("perimeters", -10);
            THEN( "Validate returns error") {
                REQUIRE(! config.validate().empty());
            }
        }
    }
}

SCENARIO("Config accessor functions perform as expected.", "[Config]") {
    GIVEN("A config generated from default options") {
        Slic3r::DynamicPrintConfig config = Slic3r::DynamicPrintConfig::full_print_config();
        WHEN("A boolean option is set to a boolean value") {
            REQUIRE_NOTHROW(config.set("gcode_comments", true));
            THEN("The underlying value is set correctly.") {
                REQUIRE(config.opt<ConfigOptionBool>("gcode_comments")->getBool() == true);
            }
        }
        WHEN("A boolean option is set to a string value representing a 0 or 1") {
            CHECK_NOTHROW(config.set_deserialize_strict("gcode_comments", "1"));
            THEN("The underlying value is set correctly.") {
                REQUIRE(config.opt<ConfigOptionBool>("gcode_comments")->getBool() == true);
            }
        }
        WHEN("A boolean option is set to a string value representing something other than 0 or 1") {
            THEN("A BadOptionTypeException exception is thrown.") {
                REQUIRE_THROWS_AS(config.set("gcode_comments", "Z"), BadOptionTypeException);
            }
            AND_THEN("Value is unchanged.") {
                REQUIRE(config.opt<ConfigOptionBool>("gcode_comments")->getBool() == false);
            }
        }
        WHEN("A boolean option is set to an int value") {
            THEN("A BadOptionTypeException exception is thrown.") {
                REQUIRE_THROWS_AS(config.set("gcode_comments", 1), BadOptionTypeException);
            }
        }
        WHEN("A numeric option is set from serialized string") {
            config.set_deserialize_strict("bed_temperature", "100");
            THEN("The underlying value is set correctly.") {
                REQUIRE(config.opt<ConfigOptionInts>("bed_temperature")->get_at(0) == 100);
            }
        }
#if 0
		//FIXME better design accessors for vector elements.
		WHEN("An integer-based option is set through the integer interface") {
            config.set("bed_temperature", 100);
            THEN("The underlying value is set correctly.") {
                REQUIRE(config.opt<ConfigOptionInts>("bed_temperature")->get_at(0) == 100);
            }
        }
#endif
        WHEN("An floating-point option is set through the integer interface") {
            config.set("perimeter_speed", 10);
            THEN("The underlying value is set correctly.") {
                REQUIRE(config.opt<ConfigOptionFloat>("perimeter_speed")->getFloat() == 10.0);
            }
        }
        WHEN("A floating-point option is set through the double interface") {
            config.set("perimeter_speed", 5.5);
            THEN("The underlying value is set correctly.") {
                REQUIRE(config.opt<ConfigOptionFloat>("perimeter_speed")->getFloat() == 5.5);
            }
        }
        WHEN("An integer-based option is set through the double interface") {
            THEN("A BadOptionTypeException exception is thrown.") {
                REQUIRE_THROWS_AS(config.set("bed_temperature", 5.5), BadOptionTypeException);
            }
        }
        WHEN("A numeric option is set to a non-numeric value.") {
            THEN("A BadOptionTypeException exception is thown.") {
                REQUIRE_THROWS_AS(config.set_deserialize_strict("perimeter_speed", "zzzz"), BadOptionValueException);
            }
            THEN("The value does not change.") {
                REQUIRE(config.opt<ConfigOptionFloat>("perimeter_speed")->getFloat() == 60.0);
            }
        }
        WHEN("A string option is set through the string interface") {
            config.set("end_gcode", "100");
            THEN("The underlying value is set correctly.") {
                REQUIRE(config.opt<ConfigOptionString>("end_gcode")->value == "100");
            }
        }
        WHEN("A string option is set through the integer interface") {
            config.set("end_gcode", 100);
            THEN("The underlying value is set correctly.") {
                REQUIRE(config.opt<ConfigOptionString>("end_gcode")->value == "100");
            }
        }
        WHEN("A string option is set through the double interface") {
            config.set("end_gcode", 100.5);
            THEN("The underlying value is set correctly.") {
                REQUIRE(config.opt<ConfigOptionString>("end_gcode")->value == float_to_string_decimal_point(100.5));
            }
        }
        WHEN("A float or percent is set as a percent through the string interface.") {
            config.set_deserialize_strict("first_layer_extrusion_width", "100%");
            THEN("Value and percent flag are 100/true") {
                auto tmp = config.opt<ConfigOptionFloatOrPercent>("first_layer_extrusion_width");
                REQUIRE(tmp->percent == true);
                REQUIRE(tmp->value == 100);
            }
        }
        WHEN("A float or percent is set as a float through the string interface.") {
            config.set_deserialize_strict("first_layer_extrusion_width", "100");
            THEN("Value and percent flag are 100/false") {
                auto tmp = config.opt<ConfigOptionFloatOrPercent>("first_layer_extrusion_width");
                REQUIRE(tmp->percent == false);
                REQUIRE(tmp->value == 100);
            }
        }
        WHEN("A float or percent is set as a float through the int interface.") {
            config.set("first_layer_extrusion_width", 100);
            THEN("Value and percent flag are 100/false") {
                auto tmp = config.opt<ConfigOptionFloatOrPercent>("first_layer_extrusion_width");
                REQUIRE(tmp->percent == false);
                REQUIRE(tmp->value == 100);
            }
        }
        WHEN("A float or percent is set as a float through the double interface.") {
            config.set("first_layer_extrusion_width", 100.5);
            THEN("Value and percent flag are 100.5/false") {
                auto tmp = config.opt<ConfigOptionFloatOrPercent>("first_layer_extrusion_width");
                REQUIRE(tmp->percent == false);
                REQUIRE(tmp->value == 100.5);
            }
        }
        WHEN("An invalid option is requested during set.") {
            THEN("A BadOptionTypeException exception is thrown.") {
                REQUIRE_THROWS_AS(config.set("deadbeef_invalid_option", 1), UnknownOptionException);
                REQUIRE_THROWS_AS(config.set("deadbeef_invalid_option", 1.0), UnknownOptionException);
                REQUIRE_THROWS_AS(config.set("deadbeef_invalid_option", "1"), UnknownOptionException);
                REQUIRE_THROWS_AS(config.set("deadbeef_invalid_option", true), UnknownOptionException);
            }
        }

        WHEN("An invalid option is requested during get.") {
            THEN("A UnknownOptionException exception is thrown.") {
                REQUIRE_THROWS_AS(config.option_throw<ConfigOptionString>("deadbeef_invalid_option", false), UnknownOptionException);
                REQUIRE_THROWS_AS(config.option_throw<ConfigOptionFloat>("deadbeef_invalid_option", false), UnknownOptionException);
                REQUIRE_THROWS_AS(config.option_throw<ConfigOptionInt>("deadbeef_invalid_option", false), UnknownOptionException);
                REQUIRE_THROWS_AS(config.option_throw<ConfigOptionBool>("deadbeef_invalid_option", false), UnknownOptionException);
            }
        }
        WHEN("An invalid option is requested during opt.") {
            THEN("A UnknownOptionException exception is thrown.") {
                REQUIRE_THROWS_AS(config.option_throw<ConfigOptionString>("deadbeef_invalid_option", false), UnknownOptionException);
                REQUIRE_THROWS_AS(config.option_throw<ConfigOptionFloat>("deadbeef_invalid_option", false), UnknownOptionException);
                REQUIRE_THROWS_AS(config.option_throw<ConfigOptionInt>("deadbeef_invalid_option", false), UnknownOptionException);
                REQUIRE_THROWS_AS(config.option_throw<ConfigOptionBool>("deadbeef_invalid_option", false), UnknownOptionException);
            }
        }

        WHEN("getX called on an unset option.") {
            THEN("The default is returned.") {
                REQUIRE(config.opt_float("layer_height") == 0.3);
                REQUIRE(config.opt_int("raft_layers") == 0);
                REQUIRE(config.opt_bool("support_material") == false);
            }
        }

        WHEN("getFloat called on an option that has been set.") {
            config.set("layer_height", 0.5);
            THEN("The set value is returned.") {
                REQUIRE(config.opt_float("layer_height") == 0.5);
            }
        }
    }
}

SCENARIO("Config ini load/save interface", "[Config]") {
    WHEN("new_from_ini is called") {
		Slic3r::DynamicPrintConfig config;
		std::string path = std::string(TEST_DATA_DIR) + "/test_config/new_from_ini.ini";
		config.load_from_ini(path, ForwardCompatibilitySubstitutionRule::Disable);
        THEN("Config object contains ini file options.") {
			REQUIRE(config.option_throw<ConfigOptionStrings>("filament_colour", false)->values.size() == 1);
			REQUIRE(config.option_throw<ConfigOptionStrings>("filament_colour", false)->values.front() == "#ABCD");
        }
    }
}

SCENARIO("DynamicPrintConfig serialization", "[Config]") {
    WHEN("DynamicPrintConfig is serialized and deserialized") {
        FullPrintConfig full_print_config;
        DynamicPrintConfig cfg;
        cfg.apply(full_print_config, false);

        std::string serialized;
        try {
            std::ostringstream ss;
            cereal::BinaryOutputArchive oarchive(ss);
            oarchive(cfg);
            serialized = ss.str();
        } catch (const std::runtime_error & /* e */) {
            // e.what();
        }

        THEN("Config object contains ini file options.") {
            DynamicPrintConfig cfg2;
            try {
                std::stringstream ss(serialized);
                cereal::BinaryInputArchive iarchive(ss);
                iarchive(cfg2);
            } catch (const std::runtime_error & /* e */) {
                // e.what();
            }
            REQUIRE(cfg == cfg2);
        }
    }
}

TEST_CASE("DynamicPrintConfig normalizes support filament types from filament_ids", "[Config]")
{
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.option<ConfigOptionStrings>("filament_type", true)->values      = { "PLA", "PA" };
    config.option<ConfigOptionStrings>("filament_ids", true)->values       = { "GFS00", "GFS01" };
    config.option<ConfigOptionBools>("filament_is_support", true)->values  = { true, true };

    std::string display_type;
    CHECK(config.get_filament_type(display_type, 0) == "PLA-S");
    CHECK(display_type == "Sup.PLA");

    CHECK(config.get_filament_type(display_type, 1) == "PA-S");
    CHECK(display_type == "Sup.PA");
}

TEST_CASE("DynamicPrintConfig keeps ordinary filament types unchanged", "[Config]")
{
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.option<ConfigOptionStrings>("filament_type", true)->values      = { "PLA" };
    config.option<ConfigOptionStrings>("filament_ids", true)->values       = { "GFSL99" };
    config.option<ConfigOptionBools>("filament_is_support", true)->values  = { false };

    std::string display_type;
    CHECK(config.get_filament_type(display_type, 0) == "PLA");
    CHECK(display_type == "PLA");
}

TEST_CASE("Timelapse enum values map to the reordered GUI choices", "[Config][Timelapse][ESP32]")
{
    const ConfigOptionDef* timelapse = print_config_def.get("timelapse_type");
    REQUIRE(timelapse != nullptr);

    SECTION("stored enum values select Off, Traditional, and Smooth in display order") {
        CHECK(timelapse->enum_value_to_gui_index(TimelapseType::tlOff) == 0);
        CHECK(timelapse->enum_value_to_gui_index(TimelapseType::tlTraditional) == 1);
        CHECK(timelapse->enum_value_to_gui_index(TimelapseType::tlSmooth) == 2);
    }

    SECTION("display choices write the historical enum values") {
        CHECK(timelapse->gui_index_to_enum_value(0) == TimelapseType::tlOff);
        CHECK(timelapse->gui_index_to_enum_value(1) == TimelapseType::tlTraditional);
        CHECK(timelapse->gui_index_to_enum_value(2) == TimelapseType::tlSmooth);
    }

    SECTION("Bambu legacy values retain their serialized keys") {
        ConfigOptionEnum<TimelapseType> traditional(TimelapseType::tlTraditional);
        ConfigOptionEnum<TimelapseType> smooth(TimelapseType::tlSmooth);
        ConfigOptionEnum<TimelapseType> off(TimelapseType::tlOff);

        CHECK(traditional.serialize() == "0");
        CHECK(smooth.serialize() == "1");
        CHECK(off.serialize() == "2");

        DynamicPrintConfig numeric_off = DynamicPrintConfig::full_print_config();
        numeric_off.set_deserialize_strict("timelapse_type", "2");
        CHECK(numeric_off.opt_enum<TimelapseType>("timelapse_type") == TimelapseType::tlOff);

        DynamicPrintConfig named_off = DynamicPrintConfig::full_print_config();
        named_off.set_deserialize_strict("timelapse_type", "off");
        CHECK(named_off.opt_enum<TimelapseType>("timelapse_type") == TimelapseType::tlOff);
    }

    SECTION("ESP32 Timelapse Box dwell exposes the receiver-safe minimum") {
        const ConfigOptionDef* dwell = print_config_def.get("esp32_timelapse_dwell_ms");
        REQUIRE(dwell != nullptr);
        CHECK(dwell->min == ESP32_TIMELAPSE_MIN_DWELL_MS);
    }

    SECTION("ESP32 frame commands require executable G-code") {
        CHECK_FALSE(gcode_has_executable_lines(""));
        CHECK_FALSE(gcode_has_executable_lines(" \t\r\n"));
        CHECK_FALSE(gcode_has_executable_lines("; disabled\r\n ; ESP_TIMELAPSE_SHOT"));
        CHECK(gcode_has_executable_lines("; camera trigger follows\n  esp_timelapse_shot  "));
    }

    SECTION("the development command name migrates to the U1-compatible macro") {
        DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
        config.set_deserialize_strict("esp32_timelapse_gcode", "ESP32_TIMELAPSE_SHOT");
        CHECK(config.opt_string("esp32_timelapse_gcode") == "ESP_TIMELAPSE_SHOT");
    }

    SECTION("plate metadata and spiral vase compatibility preserve the Off state") {
        CHECK(timelapse_type_to_plate_metadata(TimelapseType::tlTraditional) == 0);
        CHECK(timelapse_type_to_plate_metadata(TimelapseType::tlSmooth) == 1);
        CHECK(timelapse_type_to_plate_metadata(TimelapseType::tlOff) == -1);
        CHECK(timelapse_type_compatible_with_spiral_vase(TimelapseType::tlTraditional));
        CHECK_FALSE(timelapse_type_compatible_with_spiral_vase(TimelapseType::tlSmooth));
        CHECK(timelapse_type_compatible_with_spiral_vase(TimelapseType::tlOff));

        DynamicPrintConfig off_config = DynamicPrintConfig::full_print_config();
        off_config.set_deserialize_strict("timelapse_type", "2");
        normalize_timelapse_for_spiral_vase(off_config);
        CHECK(off_config.opt_enum<TimelapseType>("timelapse_type") == TimelapseType::tlOff);

        DynamicPrintConfig smooth_config = DynamicPrintConfig::full_print_config();
        smooth_config.set_deserialize_strict("timelapse_type", "1");
        normalize_timelapse_for_spiral_vase(smooth_config);
        CHECK(smooth_config.opt_enum<TimelapseType>("timelapse_type") == TimelapseType::tlTraditional);
    }
}

TEST_CASE("Legacy ESP32 timelapse dwell values migrate before range validation", "[Config][Preset][3MF][Timelapse][ESP32]")
{
    const std::array<std::pair<int, int>, 5> cases = {{
        { 1000, 2000 }, { 1200, 2000 }, { 1999, 2000 }, { 2000, 2000 }, { 2345, 2345 }
    }};

    SECTION("direct configuration loading migrates only historical low values") {
        for (const auto& [stored, expected] : cases) {
            INFO("stored dwell " << stored);
            DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
            REQUIRE_NOTHROW(config.set_deserialize_strict("esp32_timelapse_dwell_ms", std::to_string(stored)));
            CHECK(config.opt_int("esp32_timelapse_dwell_ms") == expected);
        }
    }

    SECTION("printer preset reload applies the same migration") {
        DynamicPrintConfig full = DynamicPrintConfig::full_print_config();
        DynamicPrintConfig printer_defaults;
        printer_defaults.apply_only(full, Preset::printer_options(), true);
        Preset parent(Preset::TYPE_PRINTER, "Legacy ESP32 Parent", true);
        parent.config = printer_defaults;

        for (const auto& [stored, expected] : cases) {
            INFO("stored dwell " << stored);
            Preset legacy(Preset::TYPE_PRINTER, "Legacy ESP32 Dwell");
            legacy.config = parent.config;
            legacy.config.set_key_value("esp32_timelapse_dwell_ms", new ConfigOptionInt(stored));
            const boost::filesystem::path path = boost::filesystem::temp_directory_path() /
                boost::filesystem::unique_path("legacy-esp32-dwell-%%%%-%%%%.json");
            legacy.file = path.string();
            legacy.save(&parent.config);

            Preset loaded(Preset::TYPE_PRINTER, "Legacy ESP32 Dwell");
            loaded.file = path.string();
            REQUIRE_NOTHROW(loaded.reload(parent));
            CHECK(loaded.config.opt_int("esp32_timelapse_dwell_ms") == expected);

            boost::filesystem::remove(path);
            boost::filesystem::path info_path(path);
            info_path.replace_extension(".info");
            boost::filesystem::remove(info_path);
        }
    }
}

TEST_CASE("Primary nozzle variant follows logical T0", "[Config][Preset][Nozzle]")
{
    CHECK(primary_nozzle_variant({0.4, 0.2, 0.6, 0.8}) == "0.4");
    CHECK(primary_nozzle_variant({0.2, 0.4, 0.4, 0.4}) == "0.2");
    CHECK(primary_nozzle_variant({0.8}) == "0.8");
    CHECK(primary_nozzle_variant({}).empty());
}

TEST_CASE("Configured ESP32 Timelapse Box is an additive capability", "[Config][Timelapse][ESP32][GUI]")
{
    DynamicPrintConfig printer = DynamicPrintConfig::full_print_config();
    printer.set_deserialize_strict({
        { "gcode_flavor", "klipper" },
        { "supports_esp32_timelapse", true },
        { "esp32_timelapse_gcode", "ESP_TIMELAPSE_SHOT" },
        { "esp32_timelapse_park_x", 250.0 },
        { "esp32_timelapse_park_y", 240.0 },
        { "esp32_timelapse_dwell_ms", 2000 }
    });

    CHECK(dynamic_print_config_supports_esp32_timelapse(printer));

    DynamicPrintConfig process = DynamicPrintConfig::full_print_config();
    process.set_deserialize_strict("timelapse_type", "1");
    CHECK(dynamic_print_config_uses_legacy_smooth_timelapse(process, printer));
    CHECK(dynamic_print_config_uses_smooth_timelapse_tower(process, printer));

    printer.set("supports_esp32_timelapse", false);
    CHECK_FALSE(dynamic_print_config_supports_esp32_timelapse(printer));
    CHECK(dynamic_print_config_uses_legacy_smooth_timelapse(process, printer));

    printer.set_deserialize_strict("gcode_flavor", "marlin");
    CHECK_FALSE(dynamic_print_config_supports_esp32_timelapse(printer));
}

TEST_CASE("Snapmaker U1 profiles enable additive ESP32 timelapse and retain native hooks", "[Config][Profile][Timelapse][ESP32]")
{
    const boost::filesystem::path source_root = boost::filesystem::path(TEST_DATA_DIR).parent_path().parent_path();
    const boost::filesystem::path machine_dir = source_root / "resources" / "profiles" / "Snapmaker" / "machine";

    {
        std::ifstream stream((machine_dir / "fdm_U1.json").string());
        REQUIRE(stream.good());
        nlohmann::json profile;
        stream >> profile;
        REQUIRE(profile.contains("supports_esp32_timelapse"));
        CHECK(profile.at("supports_esp32_timelapse").get<std::string>() == "1");
        REQUIRE(profile.contains("esp32_timelapse_gcode"));
        CHECK(profile.at("esp32_timelapse_gcode").get<std::string>() == "ESP_TIMELAPSE_SHOT");
    }

    for (const char* diameter : { "0.2", "0.4", "0.6", "0.8" }) {
        const boost::filesystem::path path = machine_dir / (std::string("Snapmaker U1 (") + diameter + " nozzle).json");
        INFO(path.string());
        std::ifstream stream(path.string());
        REQUIRE(stream.good());
        nlohmann::json profile;
        stream >> profile;
        REQUIRE(profile.contains("before_layer_change_gcode"));
        const std::string before_layer = profile.at("before_layer_change_gcode").get<std::string>();
        CHECK(before_layer.find("TIMELAPSE_TAKE_FRAME") != std::string::npos);
        CHECK(before_layer.find("{if timelapse_type != 2}") != std::string::npos);
        CHECK(before_layer.find("supports_esp32_timelapse") == std::string::npos);
    }
}

TEST_CASE("Generic Klipper printer presets retain ESP32 Timelapse Box fields", "[Config][Preset][Timelapse][ESP32]")
{
    DynamicPrintConfig full = DynamicPrintConfig::full_print_config();
    CHECK(full.opt_float("esp32_timelapse_park_x") == Approx(-99999.0));
    CHECK(full.opt_float("esp32_timelapse_park_y") == Approx(-99999.0));
    DynamicPrintConfig printer_defaults;
    printer_defaults.apply_only(full, Preset::printer_options(), true);

    Preset parent(Preset::TYPE_PRINTER, "Synthetic Klipper Parent", true);
    parent.config = printer_defaults;

    Preset child(Preset::TYPE_PRINTER, "Synthetic Klipper ESP32 Timelapse Box");
    child.config = parent.config;
    child.config.set_deserialize_strict({
        { "gcode_flavor", "klipper" },
        { "supports_esp32_timelapse", true },
        { "esp32_timelapse_gcode", "SYNTHETIC_ESP_TIMELAPSE_SHOT" },
        { "esp32_timelapse_park_x", 17.5 },
        { "esp32_timelapse_park_y", 231.25 },
        { "esp32_timelapse_travel_speed", 12345.0 },
        { "esp32_timelapse_dwell_ms", 2345 }
    });
    child.config.set_key_value("invalid_synthetic_printer_key", new ConfigOptionString("remove me"));

    const std::string removed = Preset::remove_invalid_keys(child.config, parent.config);
    CHECK(removed.find("invalid_synthetic_printer_key") != std::string::npos);
    CHECK_FALSE(child.config.has("invalid_synthetic_printer_key"));

    const std::array<const char*, 6> esp32_keys = {
        "supports_esp32_timelapse",
        "esp32_timelapse_gcode",
        "esp32_timelapse_park_x",
        "esp32_timelapse_park_y",
        "esp32_timelapse_travel_speed",
        "esp32_timelapse_dwell_ms"
    };
    for (const char* key : esp32_keys) {
        INFO("printer option " << key);
        REQUIRE(child.config.has(key));
        CHECK(std::find(Preset::printer_options().begin(), Preset::printer_options().end(), key) != Preset::printer_options().end());
    }

    const boost::filesystem::path path = boost::filesystem::temp_directory_path() /
        boost::filesystem::unique_path("synthetic-klipper-esp32-timelapse-%%%%-%%%%.json");
    child.file = path.string();
    child.save(&parent.config);

    Preset loaded(Preset::TYPE_PRINTER, "Synthetic Klipper ESP32 Timelapse Box");
    loaded.file = path.string();
    loaded.reload(parent);

    CHECK(loaded.config.opt_enum<GCodeFlavor>("gcode_flavor") == gcfKlipper);
    CHECK(loaded.config.opt_bool("supports_esp32_timelapse"));
    CHECK(loaded.config.opt_string("esp32_timelapse_gcode") == "SYNTHETIC_ESP_TIMELAPSE_SHOT");
    CHECK(loaded.config.opt_float("esp32_timelapse_park_x") == Approx(17.5));
    CHECK(loaded.config.opt_float("esp32_timelapse_park_y") == Approx(231.25));
    CHECK(loaded.config.opt_float("esp32_timelapse_travel_speed") == Approx(12345.0));
    CHECK(loaded.config.opt_int("esp32_timelapse_dwell_ms") == 2345);

    boost::filesystem::remove(path);
    boost::filesystem::path info_path(path);
    info_path.replace_extension(".info");
    boost::filesystem::remove(info_path);
}

TEST_CASE("Smooth timelapse tower semantics include active ESP32 Timelapse Box", "[Config][Timelapse][ESP32]")
{
    DynamicPrintConfig process = DynamicPrintConfig::full_print_config();
    process.set_deserialize_strict("timelapse_type", "1");

    DynamicPrintConfig esp32_printer = DynamicPrintConfig::full_print_config();
    esp32_printer.set_deserialize_strict({
        { "gcode_flavor", "klipper" },
        { "supports_esp32_timelapse", true }
    });
    DynamicPrintConfig bambu_printer = DynamicPrintConfig::full_print_config();
    bambu_printer.set_deserialize_strict({
        { "gcode_flavor", "marlin" },
        { "supports_esp32_timelapse", false }
    });

    CHECK(dynamic_print_config_uses_legacy_smooth_timelapse(process, esp32_printer));
    CHECK(dynamic_print_config_uses_legacy_smooth_timelapse(process, bambu_printer));
    CHECK(dynamic_print_config_uses_smooth_timelapse_tower(process, esp32_printer));
    CHECK(dynamic_print_config_uses_smooth_timelapse_tower(process, bambu_printer));

    CHECK_FALSE(should_reserve_wipe_tower(false, true, 1));
    CHECK_FALSE(should_reserve_wipe_tower(true, false, 1));
    CHECK(should_reserve_wipe_tower(true, false, 2));
    CHECK(should_reserve_wipe_tower(true, true, 1));

    DynamicPrintConfig normalized_esp32 = esp32_printer;
    normalized_esp32.set_deserialize_strict({
        { "timelapse_type", "1" },
        { "enable_prime_tower", false }
    });

    SECTION("both normalization entry points force the single-tool ESP32 tower on") {
        DynamicPrintConfig normalized_legacy_path = normalized_esp32;
        normalized_legacy_path.normalize_fdm(1);
        CHECK(normalized_legacy_path.opt_bool("enable_prime_tower"));

        DynamicPrintConfig normalized_split_path = normalized_esp32;
        normalized_split_path.normalize_fdm_1();
        CHECK(normalized_split_path.opt_bool("enable_prime_tower"));
        const t_config_option_keys changed = normalized_split_path.normalize_fdm_2(1, 1);
        CHECK(normalized_split_path.opt_bool("enable_prime_tower"));
        CHECK(std::find(changed.begin(), changed.end(), "enable_prime_tower") == changed.end());
    }

    SECTION("split normalization reports the automatic Smooth tower change") {
        const t_config_option_keys changed = normalized_esp32.normalize_fdm_2(1, 1);
        CHECK(normalized_esp32.opt_bool("enable_prime_tower"));
        CHECK(std::find(changed.begin(), changed.end(), "enable_prime_tower") != changed.end());
    }

    SECTION("Off and Traditional do not retain a forced single-tool tower") {
        for (const char* type : { "2", "0" }) {
            DynamicPrintConfig switched = esp32_printer;
            switched.set_deserialize_strict({
                { "timelapse_type", type },
                { "enable_prime_tower", true }
            });
            switched.normalize_fdm_2(1, 1);
            CHECK_FALSE(switched.opt_bool("enable_prime_tower"));
        }
    }

    DynamicPrintConfig normalized_bambu = bambu_printer;
    normalized_bambu.set_deserialize_strict({
        { "timelapse_type", "1" },
        { "enable_prime_tower", true }
    });
    normalized_bambu.normalize_fdm_2(1, 1);
    CHECK(normalized_bambu.opt_bool("enable_prime_tower"));
}
