#include <catch2/catch_test_macros.hpp>

#include "libslic3r/WallLoopFilaments.hpp"

using namespace Slic3r;

TEST_CASE("Wall loop filament IDs are plain outside-in filament numbers", "[WallLoopFilaments]")
{
    std::vector<unsigned int> ids;
    REQUIRE(parse_wall_loop_filaments("2, 1, 3, 3", ids));
    REQUIRE(ids == std::vector<unsigned int>{2, 1, 3, 3});
    for (int index = 0; index < 4; ++index)
        CHECK(wall_loop_filament(ids, index, 4, 4) == ids[size_t(index)]);
    CHECK(wall_loop_filament(ids, 4, 4, 4) == 3);
    CHECK(wall_loop_filament(ids, 100, 4, 4) == 3);
    CHECK(wall_loop_filament(ids, -1, 4, 4) == 4);
    REQUIRE(parse_wall_loop_filaments("12,10", ids));
    CHECK(wall_loop_filament(ids, 0, 1, 12) == 12);
    CHECK(wall_loop_filament(ids, 1, 1, 12) == 10);
}

TEST_CASE("Empty wall loop list preserves regular wall behavior", "[WallLoopFilaments]")
{
    std::vector<unsigned int> ids{2};
    for (const auto* input : {"", " ", "\t\r\n"}) {
        REQUIRE(parse_wall_loop_filaments(input, ids));
        CHECK(ids.empty());
        CHECK(wall_loop_filament(ids, 0, 3, 4) == 3);
        CHECK(wall_loop_filament(ids, 8, 3, 4) == 3);
    }
}

TEST_CASE("Malformed wall loop lists cannot shift inner wall assignments", "[WallLoopFilaments]")
{
    std::vector<unsigned int> ids;
    for (const auto* input :
         {"0", "-1", "+2", "1,,2", ",1", "1,", "1, ", "1 2", "1.5", "1;a", "A,B", "1,2oops", "4294967296", "999999999999999999999999999"}) {
        INFO(input);
        CHECK_FALSE(parse_wall_loop_filaments(input, ids));
        CHECK(ids.empty());
    }
    REQUIRE(parse_wall_loop_filaments("1,4", ids));
    CHECK(wall_loop_filaments_in_range(ids, 4));
    CHECK_FALSE(wall_loop_filaments_in_range(ids, 3));
    CHECK(wall_loop_filament(ids, 1, 2, 3) == 2);
}

TEST_CASE("Filament remapping preserves repeats and wall positions", "[WallLoopFilaments][Remap]")
{
    std::string text = "2, 1, 4, 4";
    REQUIRE(remap_wall_loop_filaments(text, {0, 2, 1, 3, 4}, 4));
    CHECK(text == "1,2,4,4");
    REQUIRE(remap_wall_loop_filaments(text, {0, 1, 2, 0, 3}, 3));
    CHECK(text == "1,2,3,3");
    CHECK_FALSE(remap_wall_loop_filaments(text, {0, 1, 2, 0}, 2));
    CHECK(text == "1,2,3,3"); // No partial remap on a missing filament.
    text.clear();
    REQUIRE(remap_wall_loop_filaments(text, {0, 1}, 1));
    CHECK(text.empty()); // Explicit empty overrides must survive remapping.
}
