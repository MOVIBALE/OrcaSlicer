#include <catch2/catch.hpp>

#include "libslic3r/libslic3r.h"
#include "libslic3r/Print.hpp"
#include "libslic3r/Layer.hpp"

#include "test_data.hpp"

using namespace Slic3r;
using namespace Slic3r::Test;

SCENARIO("PrintObject: object layer heights", "[PrintObject]") {
    GIVEN("20mm cube and default initial config, initial layer height of 2mm") {
        WHEN("generate_object_layers() is called for 2mm layer heights and nozzle diameter of 3mm") {
            Slic3r::Print print;
            Slic3r::Test::init_and_process_print({TestMesh::cube_20x20x20}, print, {
        		{ "first_layer_height", 2 },
				{ "layer_height", 		2 },
	            { "nozzle_diameter", 	3 }
	        });
            ConstLayerPtrsAdaptor layers = print.objects().front()->layers();
            THEN("The output vector has 10 entries") {
                REQUIRE(layers.size() == 10);
            }
            AND_THEN("Each layer is approximately 2mm above the previous Z") {
                coordf_t last = 0.0;
                for (size_t i = 0; i < layers.size(); ++ i) {
                    REQUIRE((layers[i]->print_z - last) == Approx(2.0));
                    last = layers[i]->print_z;
                }
            }
        }
        WHEN("generate_object_layers() is called for 10mm layer heights and nozzle diameter of 11mm") {
            Slic3r::Print print;
            Slic3r::Test::init_and_process_print({TestMesh::cube_20x20x20}, print, {
        		{ "first_layer_height", 2 },
				{ "layer_height", 		10 },
	            { "nozzle_diameter", 	11 }
	        });
            ConstLayerPtrsAdaptor layers = print.objects().front()->layers();
			THEN("The output vector has 3 entries") {
                REQUIRE(layers.size() == 3);
            }
            AND_THEN("Layer 0 is at 2mm") {
                REQUIRE(layers.front()->print_z == Approx(2.0));
            }
            AND_THEN("Layer 1 is at 12mm") {
                REQUIRE(layers[1]->print_z == Approx(12.0));
            }
        }
        WHEN("generate_object_layers() is called for 15mm layer heights and nozzle diameter of 16mm") {
            Slic3r::Print print;
            Slic3r::Test::init_and_process_print({TestMesh::cube_20x20x20}, print, {
        		{ "first_layer_height", 2 },
				{ "layer_height", 		15 },
	            { "nozzle_diameter", 	16 }
	        });
            ConstLayerPtrsAdaptor layers = print.objects().front()->layers();
			THEN("The output vector has 2 entries") {
                REQUIRE(layers.size() == 2);
            }
            AND_THEN("Layer 0 is at 2mm") {
                REQUIRE(layers[0]->print_z == Approx(2.0));
            }
            AND_THEN("Layer 1 is at 17mm") {
                REQUIRE(layers[1]->print_z == Approx(17.0));
            }
        }
#if 0
        WHEN("generate_object_layers() is called for 15mm layer heights and nozzle diameter of 5mm") {
            Slic3r::Print print;
            Slic3r::Test::init_and_process_print({TestMesh::cube_20x20x20}, print, {
        		{ "first_layer_height", 2 },
				{ "layer_height", 		15 },
	            { "nozzle_diameter", 	5 }
	        });
			const std::vector<Slic3r::Layer*> &layers = print.objects().front()->layers();
			THEN("The layer height is limited to 5mm.") {
                CHECK(layers.size() == 5);
                coordf_t last = 2.0;
                for (size_t i = 1; i < layers.size(); i++) {
                    REQUIRE((layers[i]->print_z - last) == Approx(5.0));
                    last = layers[i]->print_z;
                }
            }
        }
#endif
    }
}

SCENARIO("PrintObject: default object extruder preserves role filament mapping", "[PrintObject]") {
    GIVEN("A 20mm cube with the object defaulted to extruder 1 and role-specific filament settings") {
        Slic3r::DynamicPrintConfig config = Slic3r::DynamicPrintConfig::full_print_config();
        config.set_deserialize_strict({
            { "wall_loops", 3 },
            { "sparse_infill_density", 15 },
            { "outer_wall_filament", 1 },
            { "wall_filament", 2 },
            { "sparse_infill_filament", 2 },
            { "solid_infill_filament", 2 }
        });
        config.option<ConfigOptionFloats>("filament_diameter")->values = { 1.75, 1.75 };
        config.option<ConfigOptionFloats>("nozzle_diameter")->values = { 0.2, 0.4 };
        config.option<ConfigOptionStrings>("filament_colour")->values = { "#111111", "#222222" };

        Slic3r::Print print;
        Slic3r::Model model;
        Slic3r::ModelObject *object = model.add_object();
        object->name = "cube";
        object->add_volume(Slic3r::Test::mesh(TestMesh::cube_20x20x20));
        object->add_instance();
        object->config.set_key_value("extruder", new ConfigOptionInt(1));
        object->ensure_on_bed();

        print.apply(model, config);
        print.validate();
        print.set_status_silent();
        print.process();

        THEN("The generated print region keeps the process-level role filament split") {
            auto regions = print.objects().front()->all_regions();
            REQUIRE(!regions.empty());
            const PrintRegionConfig &region_config = regions.front().get().config();
            CHECK(region_config.outer_wall_filament.value == 1);
            CHECK(region_config.wall_filament.value == 2);
            CHECK(region_config.sparse_infill_filament.value == 2);
            CHECK(region_config.solid_infill_filament.value == 2);
        }
    }
}
