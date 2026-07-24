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
// cheats. That blind spot hid a real regression from the Doom::Fixed -> Doom::Fixed
// migration, so these pin the numbers directly rather than through a picture.
//
// The bug: thintriangle_guy scaled its vertices with `#define R (FRACUNIT)`,
// making each `-.5 * R` a `double * Fixed`. That converts -.5 to `int` 0 before
// multiplying, so every scaled vertex collapsed to the origin and the shape
// became one degenerate line. Vanilla's Doom::Fixed was a plain int, where
// -.5 * 65536 truncated to -32768 as intended. triangle_guy next to it scales
// off FRACUNIT.raw and was always right, which is what made the difference
// visible. Values below are vanilla's, from git show 110ddbe:src/DOOM/am_map.c.
auto tShapeTables = test("Automap/shapeTablesAreScaled") = []
{
    constexpr auto unit = Doom::Fixed::fracUnit; // 65536

    // -.5 and .7 of a unit, truncated toward zero as a cast from double does.
    constexpr auto half = static_cast<std::int32_t>(-.5 * unit); // -32768
    constexpr auto lean = static_cast<std::int32_t>(.7 * unit); //  45875

    check(Doom::mapShapes().thinTriangleGuy[0].a.x == Doom::Fixed {half});
    check(Doom::mapShapes().thinTriangleGuy[0].a.y == Doom::Fixed {-lean});
    check(Doom::mapShapes().thinTriangleGuy[0].b.x == Doom::Fixed {unit});
    check(Doom::mapShapes().thinTriangleGuy[0].b.y == Doom::Fixed {});
    check(Doom::mapShapes().thinTriangleGuy[1].b.x == Doom::Fixed {half});
    check(Doom::mapShapes().thinTriangleGuy[1].b.y == Doom::Fixed {lean});
    check(Doom::mapShapes().thinTriangleGuy[2].a.y == Doom::Fixed {lean});
    check(Doom::mapShapes().thinTriangleGuy[2].b.y == Doom::Fixed {-lean});

    // The shape must enclose an area. Under the bug every vertex above except
    // b.x collapsed to zero, and this is the assertion that caught it.
    auto distinct = 0;
    for (const auto& line: Doom::mapShapes().thinTriangleGuy)
        if (line.a.x != line.b.x || line.a.y != line.b.y)
            ++distinct;

    check(distinct == 3, "no edge of the thin triangle is degenerate");

    // The player arrows scale off PLAYERRADIUS, which is integer arithmetic on a
    // Fixed and was never at risk - checked so the whole category is pinned, not
    // just the member that broke.
    check(Doom::mapShapes().playerArrow[0].a.x
          != Doom::mapShapes().playerArrow[0].b.x);
    check(Doom::mapShapes().cheatPlayerArrow[0].a.x
          != Doom::mapShapes().cheatPlayerArrow[0].b.x);
};
