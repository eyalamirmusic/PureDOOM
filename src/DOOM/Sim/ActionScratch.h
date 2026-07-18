#pragma once

#include "../doomtype.h" // doom_boolean
#include "../m_fixed.h" // fixed_t

// Forward declarations at global scope (that is where p_mobj.h / r_defs.h declare them) - the
// scratch holds pointers to these, not their layout. Declaring them inside namespace Doom would make
// distinct Doom:: types that do not match the vanilla mobj_t / line_t the reference aliases bind to.
struct mobj_t; // mobj_t
struct line_t; // line_t

namespace Doom
{

// The p_map "action" scratch - the working state Doom::slideMove, the hitscan attacks, Doom::useLines,
// Doom::radiusAttack and Doom::changeSector each set up and read back within one call, but which vanilla
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
    // Doom::slideMove.
    fixed_t bestslidefrac = 0; // closest slide so far along the move
    fixed_t secondslidefrac = 0; // runner-up
    line_t* bestslideline = nullptr; // the wall slid against
    line_t* secondslideline = nullptr; // the runner-up wall
    mobj_t* slidemo = nullptr; // the mobj sliding
    fixed_t tmxmove = 0; // residual x move after the slide
    fixed_t tmymove = 0; // residual y move after the slide

    // The hitscan attacks (Doom::aimLineAttack / Doom::lineAttack).
    fixed_t aimslope = 0; // vertical slope to the aimed target
    mobj_t* shootthing = nullptr; // the mobj firing
    fixed_t shootz = 0; // z the shot leaves from
    int la_damage = 0; // hitscan damage

    // Doom::useLines.
    mobj_t* usething = nullptr; // the mobj pressing use

    // Doom::radiusAttack.
    mobj_t* bombsource = nullptr; // who set off the blast
    mobj_t* bombspot = nullptr; // where it went off
    int bombdamage = 0; // blast damage at the centre

    // Doom::changeSector.
    doom_boolean nofit = false; // something could not fit after the move
    doom_boolean crushchange = false; // crush things that do not fit
};

// The one ActionScratch, a view onto the Engine's member - the same pattern as the other clusters
// (clip(), wallScratch(), ...).
ActionScratch& actionScratch();
} // namespace Doom
