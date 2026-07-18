// The playsim, tested directly rather than through a demo.
//
// The demos are the real safety net for Doom::tryMove and Doom::checkPosition: they run
// thousands of collisions through them under recorded input, and a wrong answer
// desyncs the world within tics. What the demos cannot give is locality. When
// demo1 diverges at tic 48 the tic hash says "the world moved", not "Doom::tryMove
// let the player walk through a wall". These scenarios do: each pins one
// collision fact in isolation, so the rewrite of p_map has somewhere to fail
// loudly and locally before the demos even get involved.
//
// This is the Step-0 move again - widen the net before touching the code - only
// with a different driver. The harness (Tests/SimProbe) loads a level and places
// things directly, resting on the multi-scenario capability Step 4 proved: a
// level load resets the whole simulation cleanly, so a scenario runs on a fresh
// world in the same process (and NanoTest gives each test its own process anyway).
//
// The strongest assertions here need NO knowledge of the map's geometry. A solid
// thing dropped onto a spot the player legally occupies MUST make that spot
// illegal, and MF_NOCLIP MUST make it legal again, whatever the surrounding
// walls are. Only the "a legal move commits" case leans on the level, and it
// leans lightly: it discovers a clear neighbour by asking Doom::checkPosition rather
// than assuming one.

#include "../Common.h"

using namespace nano;

namespace
{
// The engine's coordinates are 16.16 fixed-point (fixed_t is int32); the harness
// takes and returns them raw. One map unit is therefore FRACUNIT.
constexpr int unit = 1 << 16;

// Shareware E1M1 at skill medium. A fixed map, so "the player's own start is a
// legal, open spot" is a safe anchor - the demos depend on the same thing.
constexpr int e1 = 1;
constexpr int m1 = 1;
constexpr int skillMedium = 2;

bool loadE1M1()
{
    return doomSimBoot(0) != 0 && doomSimLoadLevel(e1, m1, skillMedium) != 0;
}

// A solid thing standing where the player wants to be makes the spot illegal -
// and this is provable from the collision code alone, with no reference to the
// map. The player stands legally at its own start; drop a barrel exactly there
// and the same spot must become illegal; give the player MF_NOCLIP and it must
// become legal again. The walls never enter into it: only the barrel and the
// NOCLIP flag change between the three checks.
auto tThingBlocksAndNoClipBypasses =
    test("Sim/scenarioThingBlocksAndNoClipBypasses") = []
{
    check(loadE1M1(), "E1M1 loaded and the player spawned");

    auto player = doomSimPlayerHandle();
    check(player == 0, "the player is handle 0");

    auto sx = doomSimMobjX(player);
    auto sy = doomSimMobjY(player);

    check(doomSimCheckPosition(player, sx, sy) == 1,
          "the player stands legally at its own start");

    auto barrel = doomSimSpawnMobj(doomSimTypeBarrel(), sx, sy, doomSimOnFloorZ());
    check(barrel > 0, "a barrel spawned on the player's spot");

    check(doomSimCheckPosition(player, sx, sy) == 0,
          "a solid thing on the spot makes it illegal");

    auto flags = doomSimMobjFlags(player);
    doomSimSetMobjFlags(player, flags | doomSimFlagNoClip());

    check(doomSimCheckPosition(player, sx, sy) == 1,
          "MF_NOCLIP walks through the thing");

    doomSimSetMobjFlags(player, flags);
    check(doomSimCheckPosition(player, sx, sy) == 0,
          "clearing MF_NOCLIP restores the block");
};

// A blocked Doom::tryMove must answer false and must leave the thing where it was.
// This is the invariant a rewrite is most likely to break subtly - answering
// correctly but committing the move anyway, or vice versa. Geometry-free again:
// the barrel sits 40 units east (past the 26-unit combined radius, so the player
// still stands legally at its start), and the move onto it is stopped by the
// thing, whatever is or isn't behind it.
auto tBlockedTryMoveDoesNotMove = test("Sim/scenarioBlockedTryMoveDoesNotMove") = []
{
    check(loadE1M1(), "E1M1 loaded and the player spawned");

    auto player = doomSimPlayerHandle();
    auto sx = doomSimMobjX(player);
    auto sy = doomSimMobjY(player);

    auto bx = sx + 40 * unit;
    auto by = sy;

    auto barrel = doomSimSpawnMobj(doomSimTypeBarrel(), bx, by, doomSimOnFloorZ());
    check(barrel > 0, "a barrel spawned 40 units east");

    check(doomSimCheckPosition(player, sx, sy) == 1,
          "the barrel is far enough that the player still stands legally");

    check(doomSimTryMove(player, bx, by) == 0,
          "the move onto the barrel is blocked");

    check(doomSimMobjX(player) == sx && doomSimMobjY(player) == sy,
          "a blocked move left the player exactly where it was");
};

// A legal Doom::tryMove must answer true and commit the new position. Here the level
// does enter in, but only to supply a spot known to be clear: a clear neighbour
// is discovered by asking Doom::checkPosition, not assumed. Four units is small
// enough to stay inside the start sector, where a spot Doom::checkPosition accepts
// Doom::tryMove also accepts (no step, no drop-off, the same ceiling-floor gap the
// player already fits). The round trip back to the start proves the commit was a
// real move and not a no-op.
auto tLegalTryMoveCommits = test("Sim/scenarioLegalTryMoveCommits") = []
{
    check(loadE1M1(), "E1M1 loaded and the player spawned");

    auto player = doomSimPlayerHandle();
    auto sx = doomSimMobjX(player);
    auto sy = doomSimMobjY(player);

    const int d = 4 * unit;
    const int dx[] = {d, -d, 0, 0};
    const int dy[] = {0, 0, d, -d};

    auto tx = 0;
    auto ty = 0;
    auto found = false;

    for (auto i = 0; i < 4 && !found; ++i)
    {
        auto cx = sx + dx[i];
        auto cy = sy + dy[i];

        if (doomSimCheckPosition(player, cx, cy) == 1)
        {
            tx = cx;
            ty = cy;
            found = true;
        }
    }

    check(found, "the start has clear room to step into");

    check(doomSimTryMove(player, tx, ty) == 1, "the legal move succeeds");
    check(doomSimMobjX(player) == tx && doomSimMobjY(player) == ty,
          "the move committed the new position");

    check(doomSimTryMove(player, sx, sy) == 1, "stepping back succeeds");
    check(doomSimMobjX(player) == sx && doomSimMobjY(player) == sy,
          "the player is back at its start");
};

// The blockmap linking the collision code stands on, read directly. A spawned
// thing is linked into its blockmap cell by P_SetThingPosition and found there by
// P_BlockThingsIterator; unlinking it takes it back out; relinking puts it back.
// The demos exercise all three thousands of times per replay - this is the
// locality that says which one broke. Geometry-free: two barrels share the
// player's start cell and the count moves by exactly one as the second is linked,
// unlinked and relinked, whatever the surrounding map.
auto tThingLinkingAndBlockmap = test("Sim/scenarioThingLinkingAndBlockmap") = []
{
    check(loadE1M1(), "E1M1 loaded and the player spawned");

    auto player = doomSimPlayerHandle();
    auto sx = doomSimMobjX(player);
    auto sy = doomSimMobjY(player);

    auto first = doomSimSpawnMobj(doomSimTypeBarrel(), sx, sy, doomSimOnFloorZ());
    check(first > 0, "a first barrel spawned on the player's cell");

    auto base = doomSimThingsInBlockOf(first);
    check(base >= 1, "the iterator finds the barrel P_SetThingPosition linked in");

    auto second = doomSimSpawnMobj(doomSimTypeBarrel(), sx, sy, doomSimOnFloorZ());
    check(second > 0, "a second barrel spawned on the same cell");
    check(doomSimThingsInBlockOf(second) == base + 1,
          "linking the second barrel raised the cell's count by one");

    doomSimUnsetThingPosition(second);
    check(doomSimThingsInBlockOf(first) == base,
          "P_UnsetThingPosition took it back out of the cell");

    doomSimSetThingPosition(second);
    check(doomSimThingsInBlockOf(first) == base + 1,
          "P_SetThingPosition relinked it into the cell");
};
} // namespace
