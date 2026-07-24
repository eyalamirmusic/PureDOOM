// The DOOM II / episode-3 cast call (Doom::castTicker), driven directly.
//
// castTicker is the one finale path with no golden over it: an attract demo never
// completes an episode, and the shareware finale the FinaleReplay golden drives
// stops at the credits screen (finalestage 1) - it never enters the cast
// (finalestage 2). So its two behaviours are pinned here instead, through the
// cast-call harness in Tests/SimProbe: the "gross hack" that keeps the player's
// attack frame from looping forever, and the ordinary walk-cycle advance around
// it. Both were written before castTicker's goto was removed, so the removal had
// somewhere to fail loudly and locally.
//
// The state is configured field by field rather than through Doom::startCast()
// because startCast switches the music to D_EVIL, a lump the shareware WAD does
// not carry; see SimProbe.h.

#include "../Common.h"

using namespace nano;

namespace
{
// The hack: when the player's own attack frame (PLAY_ATK1) comes up, castTicker
// must jump straight to "stop attacking" - clear the attack, zero the frame
// counter, and return to the see frame - instead of advancing the animation the
// ordinary way, which for this state would loop forever. The state alone decides
// it, not which member is on screen, so any valid castnum exercises the branch;
// castnum 0 keeps stopattack's see-state lookup in range.
auto tCastPlayerAttackHack = test("Sim/castPlayerAttackHack") = []
{
    check(doomSimBoot() != 0, "engine booted");

    const auto playAtk1 = doomSimStateIndexPlayerAttack();

    // Mid-attack, on the player's attack frame, one tic from advancing.
    doomSimCastConfigure(playAtk1, 0, 1, 12, 1);

    doomSimCastTick();

    check(doomSimCastAttacking() == 0, "the hack cleared the attack");
    check(doomSimCastFrames() == 0, "the hack reset the frame counter");
    check(doomSimCastStateIndex() != playAtk1,
          "the hack left the attack frame for the see frame");
};

// The ordinary path around the hack: a member in its walk cycle steps to the
// next animation state and bumps the frame counter, without becoming an attack.
// The zombieman's see state is a plain walk loop, so one step advances it and
// touches no attack-only sound.
auto tCastNormalAdvance = test("Sim/castNormalAdvance") = []
{
    check(doomSimBoot() != 0, "engine booted");

    const auto see = doomSimStateIndexZombiemanSee();

    doomSimCastConfigure(see, 0, 1, 0, 0);

    doomSimCastTick();

    check(doomSimCastFrames() == 1, "a normal step advanced the frame counter");
    check(doomSimCastStateIndex() != see, "a normal step advanced the state");
    check(doomSimCastAttacking() == 0, "a normal walk step is not an attack");
};
} // namespace
