#pragma once

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

// The p_map "action" scratch - the working state Doom::slideMove sets up and reads
// back within one call, but which vanilla kept as globals because PTR_SlideTraverse,
// the blockmap iterator's callback, needs to see it: the best slide fraction and
// line, the sliding mobj, and the residual move.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); this
// was Sim/MapAction's own namespace-scope private globals, read by no other file.
// The hitscan attack's equivalent scratch (the aim slope, the shooter, the shot's z
// and damage) and the use/radius/change-sector callbacks' context are captures now
// (REFACTOR.md, Step 9 strand (a)) - only slideMove's, threaded through
// PTR_SlideTraverse's bare function pointer, still needs a home here. The vanilla
// names become references onto the members. Live simulation-golden-covered - the
// demos slide along walls constantly - so the byte-identical *.hashes are a live
// confirmation. secondslidefrac/secondslideline, the wall-slide "runner-up"
// PTR_SlideTraverse recorded alongside the best one, were deleted in a later audit:
// written every time a closer block was found, but never read - only
// bestslidefrac/bestslideline are ever consumed - in vanilla too (matching
// AutomapView::min_w/min_h and WeaponScratch::swingx/swingy).
struct ActionScratch
{
    // Doom::slideMove.
    fixed_t bestslidefrac {}; // closest slide so far along the move
    Line* bestslideline = nullptr; // the wall slid against
    Mobj* slidemo = nullptr; // the mobj sliding
    fixed_t tmxmove {}; // residual x move after the slide
    fixed_t tmymove {}; // residual y move after the slide
};

// The one ActionScratch, a view onto the Engine's member - the same pattern as the other clusters
// (clip(), wallScratch(), ...).
ActionScratch& actionScratch();
} // namespace Doom
