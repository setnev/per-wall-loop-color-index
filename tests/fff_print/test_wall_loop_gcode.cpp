#include <catch2/catch_test_macros.hpp>

#include "libslic3r/Print.hpp"
#include "libslic3r/GCode/ToolOrdering.hpp"
#include "test_data.hpp"

#include <algorithm>
#include <set>
#include <sstream>
#include <string>

using namespace Slic3r;

namespace {
// Only observe wall extrusion moves, so setup, infill and purge tool changes
// cannot make a misrouted wall look correct.
std::set<int> wall_tools(const std::string& gcode, const std::string& role)
{
    std::set<int>      tools;
    std::istringstream input(gcode);
    std::string        line;
    std::string        current_role;
    int                tool = 0;
    while (std::getline(input, line)) {
        if (line.size() > 1 && line[0] == 'T' && line[1] >= '0' && line[1] <= '9')
            tool = std::stoi(line.substr(1));
        const std::string marker = "; FEATURE: ";
        if (line.compare(0, marker.size(), marker) == 0) {
            current_role = line.substr(marker.size());
            if (!current_role.empty() && current_role.back() == '\r')
                current_role.pop_back();
        }
        if (current_role == role && line.compare(0, 3, "G1 ") == 0 && line.find(" E") != std::string::npos &&
            (line.find(" X") != std::string::npos || line.find(" Y") != std::string::npos))
            tools.insert(tool);
    }
    return tools;
}
} // namespace

TEST_CASE("G-code uses the selected filament for outer and inner wall loops", "[WallLoopFilaments][GCode]")
{
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.set_num_extruders(2);
    config.set_deserialize_strict({{"filament_diameter", "1.75,1.75"},
                                   {"wall_loops", 4},
                                   {"wall_filament", 1},
                                   {"wall_loop_filaments", "2,1"},
                                   {"enable_prime_tower", false},
                                   {"skirt_loops", 0}});
    std::set<int> expected_outer{1};
    std::set<int> expected_inner{0};
    bool          expect_purge_overrides = false;
    SECTION("Classic wall generator") { config.set_deserialize_strict("wall_generator", "classic"); }
    SECTION("Arachne wall generator") { config.set_deserialize_strict("wall_generator", "arachne"); }
    SECTION("Single-entry list changes all loops")
    {
        config.set("wall_loop_filaments", std::string("2"));
        expected_inner = {1};
    }
    SECTION("Empty list preserves wall filament")
    {
        config.set("wall_loop_filaments", std::string());
        expected_outer = {0};
    }
    SECTION("Infill purge overrides leave assigned walls intact")
    {
        config.set("flush_into_infill", true);
        config.set("enable_prime_tower", true);
        config.set("single_extruder_multi_material", true);
        config.set("purge_in_prime_tower", true);
        config.set_deserialize_strict("flush_volumes_matrix", "0,80,80,0");
        config.set_deserialize_strict("flush_volumes_vector", "40,40,40,40");
        expect_purge_overrides = true;
    }
    Print print;
    Model model;
    // Leave interior layers available for actual sparse-infill purging.
    Test::init_print({make_cube(20., 20., expect_purge_overrides ? 6. : 2.)}, print, model, config);
    const std::string gcode = Test::gcode(print);
    if (expect_purge_overrides) {
        const auto& ordering = print.wipe_tower_data().tool_ordering;
        CAPTURE(print.config().enable_prime_tower.value, print.config().single_extruder_multi_material.value,
                print.config().purge_in_prime_tower.value, print.objects().front()->config().flush_into_infill.value,
                print.config().filament_diameter.size(), ordering.empty(), print.config().flush_multiplier.value,
                print.config().filament_minimal_purge_on_wipe_tower.get_at(0));
        CHECK(std::any_of(ordering.begin(), ordering.end(),
                          [](LayerTools tools) { return tools.wiping_extrusions().is_anything_overridden(); }));
    }
    CHECK(wall_tools(gcode, "Outer wall") == expected_outer);
    CHECK(wall_tools(gcode, "Inner wall") == expected_inner);
}
