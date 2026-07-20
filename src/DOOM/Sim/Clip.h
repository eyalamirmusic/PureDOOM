#pragma once

#include "SimDefs.h"

#include "MapGeometry.h" // DivLine

#include "../Containers.h"

namespace Doom
{
// The transient movement/collision scratch Sim/MapUtil builds and the clipping
// code reads back. Two clusters:
//
//   - pathTraverse's intercept list: the lines and things a trace crosses, the
//     pointer into it, the early-out flag the line-intercept callback honours, and
//     the trace itself (the directed segment being walked). Rebuilt from scratch
//     on every traverse.
//   - updateLineOpening's vertical window: the gap a two-sided line leaves
//     (opentop / openbottom / openrange) plus the lower of the two floors
//     (lowfloor). Written as each line is contacted, read by the mover in
//     Sim/Movement, Sim/MapAction and Sim/Enemy.
//
// None of it is hashed - a demo's world is mobj fields, not this scratch - so
// gathering it here is golden-neutral. Every reader reaches it through clip()
// now; the vanilla-named references onto these members went with p_maputl.cpp.
struct Clip
{
    Array<Intercept, MAXINTERCEPTS> intercepts;
    Intercept* interceptPtr = nullptr;
    bool earlyOut = false;

    // pathTraverse's trace, read back by the shooting code in MapAction.
    DivLine trace = {};

    // updateLineOpening's window.
    fixed_t opentop {};
    fixed_t openbottom {};
    fixed_t openrange {};
    fixed_t lowfloor {};

    // Doom::checkPosition / Doom::tryMove clipping state (vanilla's tm*). The mover being
    // clipped, its flags and centre, and the bounding box its radius sweeps.
    Mobj* tmthing = nullptr;
    int tmflags = 0;
    fixed_t tmx {};
    fixed_t tmy {};
    Array<fixed_t, 4> tmbbox = {};

    // floatok: the move would fit if the mobj sat between tmfloorz and tmceilingz.
    bool floatok = false;

    // The floor/ceiling the contacted lines leave for the mover, and the lowest
    // floor under it (a dropoff a monster refuses to walk off).
    fixed_t tmfloorz {};
    fixed_t tmceilingz {};
    fixed_t tmdropoffz {};

    // The line that lowered the ceiling, kept so missiles don't explode against
    // sky-hack walls.
    Line* ceilingline = nullptr;

    // Special lines contacted during a move, held until the move is proven valid
    // and then crossed. Not sorted - two specials 8 units apart cross in either
    // order, a vanilla quirk.
    static constexpr int maxSpecialCross = 8;
    Array<Line*, maxSpecialCross> spechit = {};
    int numspechit = 0;

    // The range of the shot in progress. This looks like a result and is not: it is
    // an INPUT that Doom::lineAttack sets before its pathTraverse runs, and
    // Sim/Mobj.cpp's spawnPuff reads it from *inside* that same traversal's call
    // stack (nested, not stale) to give a punch's puff its S_PUFF3. aimTraverse and
    // shootTraverse's actual results (the aim slope, the locked-on target) are
    // returned by value from Doom::aimLineAttack now - see AimResult in MapAction.h.
    //
    // attackrange also carries a genuine cross-tic leak that is load-bearing vanilla
    // behaviour: spawnPuff has a third caller in no hitscan chain at all - the
    // revenant's homing rocket spawns a smoke puff every 4th tic (Sim/Enemy.cpp's
    // tracer) - which reads whatever attackrange the last hitscan left behind, so a
    // recent punch can flip that smoke to S_PUFF3 too. Threading an explicit "no
    // range" through the tracer would be a behaviour change wearing a refactor's
    // clothes, not a fix - leave it leaking.
    fixed_t attackrange {};
};

// The one Clip, a view onto the Engine's member - the same pattern as level(),
// wad() and randomness().
Clip& clip();
} // namespace Doom
