#pragma once

#include "../doomtype.h" // doom_boolean
#include "../m_fixed.h" // fixed_t

// Forward declarations at global scope (that is where p_mobj.h / r_defs.h declare them) - the
// scratch holds pointers to these, not their layout. Declaring them inside namespace Doom would make
// distinct Doom:: types that do not match the vanilla mobj_t / line_t the reference aliases bind to.
struct mobj_s; // mobj_t
struct line_s; // line_t

namespace Doom
{

// The p_map "action" scratch - the working state P_SlideMove, the hitscan attacks, P_UseLines,
// P_RadiusAttack and P_ChangeSector each set up and read back within one call, but which vanilla
// kept as globals because the PIT_* callbacks the blockmap iterators invoke need to see them:
//  - slide: the best/second slide fractions and lines, the sliding mobj, and the residual move;
//  - hitscan: the aim slope, the shooter, the shot's z and its damage;
//  - use: the mobj working a line;
//  - radius: the blast's source, centre and damage;
//  - change-sector: whether anything failed to fit and whether to crush.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// Sim/MapAction's own namespace-scope private globals, read by no other file (only linetarget and
// attackrange, which p_mobj/p_pspr read, stayed - as Clip members). The vanilla names become
// references onto the members. Live simulation-golden-covered - the demos slide along walls, fire
// hitscans, use doors and set off barrels - so the byte-identical *.hashes are a live confirmation.
struct ActionScratch
{
    // P_SlideMove.
    fixed_t bestslidefrac = 0; // closest slide so far along the move
    fixed_t secondslidefrac = 0; // runner-up
    line_s* bestslideline = nullptr; // the wall slid against
    line_s* secondslideline = nullptr; // the runner-up wall
    mobj_s* slidemo = nullptr; // the mobj sliding
    fixed_t tmxmove = 0; // residual x move after the slide
    fixed_t tmymove = 0; // residual y move after the slide

    // The hitscan attacks (P_AimLineAttack / P_LineAttack).
    fixed_t aimslope = 0; // vertical slope to the aimed target
    mobj_s* shootthing = nullptr; // the mobj firing
    fixed_t shootz = 0; // z the shot leaves from
    int la_damage = 0; // hitscan damage

    // P_UseLines.
    mobj_s* usething = nullptr; // the mobj pressing use

    // P_RadiusAttack.
    mobj_s* bombsource = nullptr; // who set off the blast
    mobj_s* bombspot = nullptr; // where it went off
    int bombdamage = 0; // blast damage at the centre

    // P_ChangeSector.
    doom_boolean nofit = false; // something could not fit after the move
    doom_boolean crushchange = false; // crush things that do not fit
};

// The one ActionScratch, a view onto the Engine's member - the same pattern as the other clusters
// (clip(), wallScratch(), ...).
ActionScratch& actionScratch();
} // namespace Doom
