#pragma once

#include "../doomtype.h" // doom_boolean
#include "../Math/FixedPoint.h" // fixed_t

// Forward declarations at global scope (that is where p_mobj.h / r_defs.h declare them) - the
// scratch holds pointers to these, not their layout. Declaring them inside namespace Doom would make
// distinct Doom:: types that do not match the vanilla Doom::Mobj / Doom::Line the reference aliases bind to.
namespace Doom
{
struct Mobj; // Mobj
} // namespace Doom
namespace Doom
{
struct Line; // Line
} // namespace Doom

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
    fixed_t bestslidefrac {}; // closest slide so far along the move
    fixed_t secondslidefrac {}; // runner-up
    Line* bestslideline = nullptr; // the wall slid against
    Line* secondslideline = nullptr; // the runner-up wall
    Mobj* slidemo = nullptr; // the mobj sliding
    fixed_t tmxmove {}; // residual x move after the slide
    fixed_t tmymove {}; // residual y move after the slide

    // The hitscan attacks (Doom::aimLineAttack / Doom::lineAttack).
    fixed_t aimslope {}; // vertical slope to the aimed target
    Mobj* shootthing = nullptr; // the mobj firing
    fixed_t shootz {}; // z the shot leaves from
    int la_damage = 0; // hitscan damage

    // Doom::useLines.
    Mobj* usething = nullptr; // the mobj pressing use

    // Doom::radiusAttack.
    Mobj* bombsource = nullptr; // who set off the blast
    Mobj* bombspot = nullptr; // where it went off
    int bombdamage = 0; // blast damage at the centre

    // Doom::changeSector.
    doom_boolean nofit = false; // something could not fit after the move
    doom_boolean crushchange = false; // crush things that do not fit
};

// The one ActionScratch, a view onto the Engine's member - the same pattern as the other clusters
// (clip(), wallScratch(), ...).
ActionScratch& actionScratch();
} // namespace Doom
