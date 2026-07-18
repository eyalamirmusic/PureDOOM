#pragma once

#include "../doomtype.h" // doom_boolean
#include "Specials.h"

// Forward declaration at global scope (where r_defs.h declares it) - linespeciallist holds pointers,
// not layout. Inside namespace Doom it would be a distinct Doom:: type that would not bind to Doom::Line.
namespace Doom
{
struct Line; // Line
} // namespace Doom

namespace Doom
{
// One animating flat or wall texture: a run of numpics frames from basepic that Doom::updateSpecials
// cycles picnum through every speed tics (istexture picks the texture vs flat translation table).
// Moved out of Sim/Specials so anims/lastanim can be Engine members (the type was defined there - as
// a file-scope typedef and, redundantly, a namespace one; the dead file-scope copy was deleted).
struct SurfaceAnim
{
    doom_boolean istexture; // texture (else flat)
    int picnum; // the frame currently shown
    int basepic; // the first frame
    int numpics; // # of frames
    int speed; // tics between frames
};

// The level's animated surfaces, built by Doom::initPicAnims / Doom::spawnSpecials and driven by
// Doom::updateSpecials: the animating flats/textures (anims, up to lastanim) and the list of scrolling-
// texture linedefs (linespeciallist, numlinespecials of them).
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// Sim/Specials' own namespace-scope private globals, read by no other file. The vanilla names become
// references onto the members (arrays as references-to-array). lastanim points into anims but is
// reset by Doom::initPicAnims (`lastanim = anims`), not by a self-referential initializer, so it is safe
// as a member. Live simulation-golden-covered - the demos scroll skies and cross animating floors.
struct AnimatedSurfaces
{
    static constexpr int maxAnims = 32; // MAXANIMS in Sim/Specials
    static constexpr int maxLineAnims = 64; // MAXLINEANIMS in Sim/Specials

    SurfaceAnim anims[maxAnims] = {}; // the level's animating flats/textures
    SurfaceAnim* lastanim = nullptr; // one past the last animation in use
    short numlinespecials = 0; // # of scrolling-texture linedefs
    Line* linespeciallist[maxLineAnims] = {}; // those linedefs
};

// The one AnimatedSurfaces, a view onto the Engine's member - the same pattern as the other clusters.
AnimatedSurfaces& animatedSurfaces();
} // namespace Doom
