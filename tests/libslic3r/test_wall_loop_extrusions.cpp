#include <catch2/catch_test_macros.hpp>

#include "libslic3r/Print.hpp"
#include "libslic3r/GCode/ToolOrdering.hpp"
#include "libslic3r/Layer.hpp"
#include "libslic3r/MixedFilamentConfigRemap.hpp"
#include "libslic3r/TriangleSelector.hpp"
#include "libslic3r/WallLoopExtrusions.hpp"

#include <algorithm>
#include <cmath>

using namespace Slic3r;

namespace {
ExtrusionPath wall_path(int index, ExtrusionRole role = erPerimeter)
{
    ExtrusionPath path(role, 0.08, 0.4f, 0.2f);
    path.inset_idx = index;
    path.polyline  = Polyline({Point::new_scale(1., 1.), Point::new_scale(19., 1.), Point::new_scale(19., 19.), Point::new_scale(1., 19.),
                               Point::new_scale(1., 1.)});
    return path;
}

DynamicPrintConfig four_filament_config()
{
    DynamicPrintConfig cfg = DynamicPrintConfig::full_print_config();
    cfg.set_num_extruders(4);
    cfg.set_deserialize_strict({{"filament_diameter", "1.75,1.75,1.75,1.75"},
                                {"filament_colour", "#FF0000;#00FF00;#0000FF;#FFFFFF"},
                                {"wall_filament", 4},
                                {"wall_loops", 4},
                                {"wall_loop_filaments", "2,1,3,3"}});
    return cfg;
}
} // namespace

TEST_CASE("Wall loop splitting preserves complete loops and all fallback extrusions", "[WallLoopFilaments][Extrusions]")
{
    ExtrusionEntityCollection source;
    source.no_sort = true;
    auto* nested   = new ExtrusionEntityCollection;
    for (int index = 0; index < 5; ++index) {
        auto path = wall_path(index, index == 0 ? erExternalPerimeter : erPerimeter);
        nested->append(ExtrusionLoop(path));
    }
    source.entities.emplace_back(nested);
    source.append(wall_path(-1));           // Thin wall: no loop index.
    source.append(wall_path(0, erGapFill)); // Even a spurious gap-fill index must not assign a loop color.
    std::vector<std::unique_ptr<ExtrusionEntityCollection>> buckets;
    REQUIRE(split_extrusion_collection_for_wall_loop_filaments(source, {2, 1, 3, 3}, 4, 4, buckets));
    REQUIRE(buckets.size() == 4);
    REQUIRE(buckets[0]);
    REQUIRE(buckets[1]);
    REQUIRE(buckets[2]);
    REQUIRE(buckets[3]);
    CHECK(buckets[0]->entities.size() == 1);
    CHECK(buckets[0]->entities.front()->inset_idx == 1);
    CHECK(buckets[1]->entities.front()->inset_idx == 0);
    CHECK(buckets[2]->entities.size() == 3);
    CHECK(buckets[3]->entities.size() == 2);
    double volume = 0.;
    for (const auto& bucket : buckets) {
        CHECK(bucket->no_sort);
        volume += bucket->total_volume();
    }
    CHECK(std::abs(volume - source.total_volume()) < 1e-9);
    CHECK(buckets[1]->entities.front()->is_loop());
    CHECK(nested->entities.size() == 5); // The source remains owned and unchanged.
}

TEST_CASE("Single-filament lists route every indexed wall to the selected tool", "[WallLoopFilaments][Extrusions]")
{
    ExtrusionEntityCollection source;
    source.append(ExtrusionLoop(wall_path(0, erExternalPerimeter)));
    source.append(ExtrusionLoop(wall_path(3, erOverhangPerimeter)));
    std::vector<std::unique_ptr<ExtrusionEntityCollection>> buckets;
    REQUIRE(split_extrusion_collection_for_wall_loop_filaments(source, {2}, 1, 4, buckets));
    REQUIRE(buckets[1]);
    CHECK(buckets[1]->entities.size() == 2);
    CHECK_FALSE(buckets[0]);
    CHECK_FALSE(split_extrusion_collection_for_wall_loop_filaments(source, {}, 1, 4, buckets));
    CHECK_FALSE(split_extrusion_collection_for_wall_loop_filaments(source, {2}, 5, 4, buckets));
}

TEST_CASE("Model and preset wall lists follow filament remaps", "[WallLoopFilaments][Remap]")
{
    ModelConfig        model_cfg;
    DynamicPrintConfig preset_cfg;
    model_cfg.set_key_value("wall_loop_filaments", new ConfigOptionString("2,1,4,4"));
    preset_cfg.set_key_value("wall_loop_filaments", new ConfigOptionString("2,1,4,4"));
    const std::vector<unsigned int> reorder{0, 2, 1, 3, 4};
    remap_model_config_filament_ids(model_cfg, reorder, 4);
    remap_dynamic_config_feature_filament_ids(preset_cfg, reorder, 4);
    CHECK(model_cfg.get().opt_string("wall_loop_filaments") == "1,2,4,4");
    CHECK(preset_cfg.opt_string("wall_loop_filaments") == "1,2,4,4");
    const std::vector<unsigned int> deletion{0, 1, 2, 3, 0};
    remap_model_config_filament_ids(model_cfg, deletion, 3);
    remap_dynamic_config_feature_filament_ids(preset_cfg, deletion, 3);
    CHECK_FALSE(model_cfg.has("wall_loop_filaments"));
    CHECK_FALSE(preset_cfg.has("wall_loop_filaments"));
}

TEST_CASE("Painted regions ignore wall lists when created and updated", "[WallLoopFilaments][PrintApply]")
{
    Model        model;
    ModelObject* object = model.add_object();
    ModelVolume* volume = object->add_volume(make_cube(20., 20., 2.));
    object->add_instance();
    object->ensure_on_bed();
    TriangleSelector selector(volume->mesh());
    selector.set_facet(0, EnforcerBlockerType(2));
    REQUIRE(volume->mmu_segmentation_facets.set(selector));
    DynamicPrintConfig cfg = four_filament_config();
    Print              print;
    print.set_status_silent();
    for (const auto* text : {"2,1,3,3", "1,3"}) {
        cfg.set("wall_loop_filaments", std::string(text));
        print.apply(model, cfg);
        const auto* regions = print.objects().front()->shared_regions();
        REQUIRE(regions);
        REQUIRE_FALSE(regions->layer_ranges.empty());
        const auto& painted = regions->layer_ranges.front().painted_regions;
        REQUIRE_FALSE(painted.empty());
        for (const auto& region : painted) {
            CHECK(region.region->config().wall_filament.value == int(region.extruder_id));
            CHECK(region.region->config().wall_loop_filaments.value.empty());
        }
    }
}

TEST_CASE("Object wall list validation rejects unavailable filaments", "[WallLoopFilaments][Config]")
{
    Model        model;
    ModelObject* object = model.add_object();
    object->add_volume(make_cube(20., 20., 2.));
    object->add_instance();
    object->ensure_on_bed();
    object->config.set_key_value("wall_loop_filaments", new ConfigOptionString("2,5"));
    Print print;
    print.set_status_silent();
    print.apply(model, four_filament_config());
    CHECK(print.validate().opt_key == "wall_loop_filaments");
}

TEST_CASE("Mixed wall filaments ignore plain wall lists", "[WallLoopFilaments][MixedFilament]")
{
    PrintConfig cfg                    = static_cast<const PrintConfig&>(FullPrintConfig::defaults());
    cfg.filament_diameter.values       = {1.75, 1.75};
    PrintRegionConfig region           = static_cast<const PrintRegionConfig&>(FullPrintConfig::defaults());
    region.wall_filament.value         = 3; // Virtual mixed filament, beyond the physical slots.
    region.wall_loop_filaments.value   = "2";
    region.sparse_infill_density.value = 0.;
    region.top_shell_layers.value = region.bottom_shell_layers.value = 0;
    std::vector<unsigned int> tools;
    PrintRegion::collect_object_printing_extruders(cfg, region, false, tools);
    CHECK(tools == std::vector<unsigned int>{0}); // Existing fallback; list must not add physical slot 2.
}

TEST_CASE("Wall lists inherit through object and part settings", "[WallLoopFilaments][Config]")
{
    Model        model;
    ModelObject* object = model.add_object();
    ModelVolume* volume = object->add_volume(make_cube(20., 20., 2.));
    object->add_instance();
    object->ensure_on_bed();
    DynamicPrintConfig cfg      = four_filament_config();
    std::string        expected = "2,1,3,3";
    SECTION("Global default") {}
    SECTION("Object override")
    {
        expected = "1,2";
        object->config.set_key_value("wall_loop_filaments", new ConfigOptionString(expected));
    }
    SECTION("Part override")
    {
        object->config.set_key_value("wall_loop_filaments", new ConfigOptionString("1,2"));
        expected = "3,2";
        volume->config.set_key_value("wall_loop_filaments", new ConfigOptionString(expected));
    }
    SECTION("Part explicitly disables inherited list")
    {
        expected.clear();
        volume->config.set_key_value("wall_loop_filaments", new ConfigOptionString(expected));
    }
    Print print;
    print.set_status_silent();
    print.apply(model, cfg);
    REQUIRE(print.objects().size() == 1);
    const auto regions = print.objects().front()->all_regions();
    REQUIRE(regions.size() == 1);
    CHECK(regions.front().get().config().wall_loop_filaments.value == expected);
}

TEST_CASE("Layer tool ordering includes wall list tools and protects assigned walls from purge overrides",
          "[WallLoopFilaments][ToolOrdering]")
{
    Model        model;
    ModelObject* model_object = model.add_object();
    model_object->add_volume(make_cube(20., 20., 2.));
    model_object->add_instance();
    model_object->ensure_on_bed();
    DynamicPrintConfig cfg = four_filament_config();
    cfg.set("flush_into_objects", true);
    Print print;
    print.set_status_silent();
    print.apply(model, cfg);
    PrintObject*      object             = *print.objects().begin();
    PrintRegionConfig region_cfg         = static_cast<const PrintRegionConfig&>(FullPrintConfig::defaults());
    region_cfg.wall_filament.value       = 4;
    region_cfg.wall_loop_filaments.value = "2,1,3,3";
    PrintRegion  region(region_cfg);
    Layer*       layer            = object->add_layer(0, 0.2, 0.2, 0.1);
    LayerRegion* layer_region     = layer->add_region(&region);
    auto*        walls            = new ExtrusionEntityCollection;
    bool         has_indexed_wall = true;
    SECTION("Indexed walls with gap fill") {}
    SECTION("Only thin walls and gap fill") { has_indexed_wall = false; }
    if (has_indexed_wall)
        walls->append(ExtrusionLoop(wall_path(0, erExternalPerimeter)));
    else
        walls->append(wall_path(-1));
    walls->append(wall_path(-1, erGapFill));
    layer_region->perimeters.entities.emplace_back(walls);
    ToolOrdering ordering(print, 0);
    auto&        tools = ordering.tools_for_layer(0.2);
    for (unsigned int index = 0; index < 4; ++index)
        CHECK(tools.has_extruder(index) == (has_indexed_wall || index == 3));
    CHECK_FALSE(tools.wiping_extrusions().is_overriddable(*walls, print.config(), *object, region));
    ExtrusionEntityCollection solid_fill;
    solid_fill.append(wall_path(-1, erSolidInfill));
    CHECK(tools.wiping_extrusions().is_overriddable(solid_fill, print.config(), *object, region));
    const auto used = print.object_extruders();
    for (unsigned int index = 0; index < 4; ++index)
        CHECK(std::find(used.begin(), used.end(), index) != used.end());
}

TEST_CASE("Incoming wall lists keep the prime tower enabled before regions are created", "[WallLoopFilaments][ToolOrdering]")
{
    Model        model;
    ModelObject* object = model.add_object();
    ModelVolume* volume = object->add_volume(make_cube(20., 20., 6.));
    object->add_instance();
    object->ensure_on_bed();
    DynamicPrintConfig cfg = four_filament_config();
    cfg.set("wall_filament", 1);
    cfg.set("wall_loop_filaments", std::string());
    cfg.set("enable_prime_tower", false);
    Print existing;
    existing.set_status_silent();
    existing.apply(model, cfg);
    cfg.set("enable_prime_tower", true);
    SECTION("Global assignment") { cfg.set("wall_loop_filaments", std::string("2,1")); }
    SECTION("Object assignment") { object->config.set_key_value("wall_loop_filaments", new ConfigOptionString("2,1")); }
    SECTION("Part assignment") { volume->config.set_key_value("wall_loop_filaments", new ConfigOptionString("2,1")); }
    Print fresh;
    fresh.set_status_silent();
    fresh.apply(model, cfg);
    CHECK(fresh.config().enable_prime_tower.value);
    CHECK(fresh.object_extruders().size() == 2);
    existing.apply(model, cfg);
    CHECK(existing.config().enable_prime_tower.value);
    CHECK(existing.object_extruders().size() == 2);
}
