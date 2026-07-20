#pragma once

#include "Specials.h"

#include "../Containers.h"

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
    bool istexture; // texture (else flat)
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
// Sim/Specials' own namespace-scope private globals, read by no other file. initPicAnims,
// updateSpecials and spawnSpecials each hoist animatedSurfaces() once and reach its members
// through it, rather than through file-scope reference aliases (REFACTOR.md, Step 9 strand (a)).
// Live simulation-golden-covered - the demos scroll skies and cross animating floors.
//
// Both members are Vectors rather than capped Arrays because both are genuinely
// data-driven: anims holds only those animdefs whose textures exist in this WAD
// (initPicAnims skips the rest), and linespeciallist only the map's scrolling
// linedefs. The vanilla caps of 32 and 64 sized the worst case and the code then
// carried a cursor and a count to say how much was live - `size()` says it now.
// Neither is memcpy'd, neither hands out an interior pointer, and nothing walks
// past the live end, which is what made them safe to convert where the renderer's
// similarly-shaped pools were not.
struct AnimatedSurfaces
{
    Vector<SurfaceAnim> anims; // the level's animating flats/textures
    Vector<Line*> linespeciallist; // the scrolling-texture linedefs
};

// The one AnimatedSurfaces, a view onto the Engine's member - the same pattern as the other clusters.
AnimatedSurfaces& animatedSurfaces();
} // namespace Doom
