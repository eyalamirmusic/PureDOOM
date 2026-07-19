#pragma once

#include "../Math/FixedPoint.h" // fixed_t

// Forward declaration at global scope (where p_mobj.h declares it) - the scratch holds pointers, not
// layout. Inside namespace Doom it would be a distinct Doom:: type that would not bind to Doom::Mobj.
namespace Doom
{
struct Mobj; // Mobj
} // namespace Doom

namespace Doom
{
// The monster-AI scratch two of the A_* actions keep between their blockmap callbacks: the archvile
// resurrection (A_VileChase / PIT_VileCheck - the corpse being raised, and where it is trying to
// stand it up) and the DOOM II boss brain (A_BrainInit / A_BrainSpawn - the
// list of spawn-cube targets and the cursor into it). The const movement-direction tables beside
// them (diags / xspeed / yspeed / TRACEANGLE) stay file-local - fixed reference data.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were Sim/Enemy's
// own namespace-scope private globals, read by no other file. vileCheck, vileChase, brainAwake and
// brainSpit each hoist enemyAI() once and reach its members through it, rather than through
// file-scope reference aliases (REFACTOR.md, Step 9 strand (a)). Both mechanisms are DOOM II content
// (no archvile or boss brain in the shareware episode), so this is *not* exercised by the demos - but
// relocating the storage changes nothing observable either way, the same as it did not for the
// reference aliases this replaces. vileobj, the archvile raising the corpse, was deleted in a later
// audit: set by vileChase before the blockmap search, but vileCheck's body never referenced it, in
// vanilla too (matching AutomapView::min_w/min_h and WeaponScratch::swingx/swingy).
struct EnemyAI
{
    Mobj* corpsehit = nullptr; // the corpse A_VileChase is raising
    fixed_t viletryx {}; // where it is trying to stand the corpse up
    fixed_t viletryy {};

    Mobj* braintargets[32] = {}; // the boss brain's spawn-cube targets
    int numbraintargets = 0; // # of targets found
    int braintargeton = 0; // the next target to aim a cube at

    // A_BrainSpit's skill parity, toggled every call: on easy skill the brain spits
    // a cube only every other time (was a function-local static in brainSpit).
    int brainSpitEasy = 0;
};

// The one EnemyAI, a view onto the Engine's member - the same pattern as the other clusters
// (actionScratch(), weaponScratch(), ...).
EnemyAI& enemyAI();
} // namespace Doom
