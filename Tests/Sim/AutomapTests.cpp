#include "../AutomapReplay.h"

#include <DOOM/Math/Fixed.h>
#include <DOOM/UI/AutomapTypes.h>

#include <cstdint>

using namespace nano;
using namespace DoomTests;

// UI/Automap.cpp has no demo coverage - nothing in a recorded .lmp opens the
// automap - so it gets its own frame golden before its 30 reference aliases are
// retired, driven by synthetic key events the way m_menu's golden was.
// checkAutomapMatchesGolden loads E1M1, opens the map and walks it (follow
// on/off, hand-panning, zoom, the big overview, grid, marks) and holds every
// frame against Tests/Goldens/automap.frames; see AutomapReplay.h for the script
// and what it deliberately avoids.
auto tAutomap = test("Sim/automap") = [] { checkAutomapMatchesGolden(); };

// The vector shapes the automap draws the player and the things with. The frame
// golden above cannot see thintriangle_guy at all: drawThings is gated on
// `cheating == 2` (IDDT pressed twice) and neither the script nor any demo
// cheats. That blind spot hid a real regression from the fixed_t -> Doom::Fixed
// migration, so these pin the numbers directly rather than through a picture.
//
// The bug: thintriangle_guy scaled its vertices with `#define R (FRACUNIT)`,
// making each `-.5 * R` a `double * Fixed`. That converts -.5 to `int` 0 before
// multiplying, so every scaled vertex collapsed to the origin and the shape
// became one degenerate line. Vanilla's fixed_t was a plain int, where
// -.5 * 65536 truncated to -32768 as intended. triangle_guy next to it scales
// off FRACUNIT.raw and was always right, which is what made the difference
// visible. Values below are vanilla's, from git show 110ddbe:src/DOOM/am_map.c.
auto tShapeTables = test("Automap/shapeTablesAreScaled") = []
{
    constexpr auto unit = Doom::Fixed::fracUnit; // 65536

    // -.5 and .7 of a unit, truncated toward zero as a cast from double does.
    constexpr auto half = static_cast<std::int32_t>(-.5 * unit); // -32768
    constexpr auto lean = static_cast<std::int32_t>(.7 * unit); //  45875

    check(thintriangle_guy[0].a.x == fixed_t {half});
    check(thintriangle_guy[0].a.y == fixed_t {-lean});
    check(thintriangle_guy[0].b.x == fixed_t {unit});
    check(thintriangle_guy[0].b.y == fixed_t {});
    check(thintriangle_guy[1].b.x == fixed_t {half});
    check(thintriangle_guy[1].b.y == fixed_t {lean});
    check(thintriangle_guy[2].a.y == fixed_t {lean});
    check(thintriangle_guy[2].b.y == fixed_t {-lean});

    // The shape must enclose an area. Under the bug every vertex above except
    // b.x collapsed to zero, and this is the assertion that caught it.
    auto distinct = 0;
    for (const auto& line: thintriangle_guy)
        if (line.a.x != line.b.x || line.a.y != line.b.y)
            ++distinct;

    check(distinct == 3, "no edge of the thin triangle is degenerate");

    // The player arrows scale off PLAYERRADIUS, which is integer arithmetic on a
    // Fixed and was never at risk - checked so the whole category is pinned, not
    // just the member that broke.
    check(player_arrow[0].a.x != player_arrow[0].b.x);
    check(cheat_player_arrow[0].a.x != cheat_player_arrow[0].b.x);
};
