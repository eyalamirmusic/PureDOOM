#pragma once

#include "../Game/GameDefs.h" // SCREENWIDTH
#include "../Math/FixedPoint.h" // Doom::Fixed
#include "../Sim/MapTypes.h"
#include "RenderTypes.h" // VisSprite

#include "../Containers.h"

namespace Doom
{
// The sprite-drawing state r_things exports for the rest of the renderer: the vissprite pool the
// frame's sprites are gathered into (vissprites / vissprite_p) and the sentinel head of their
// depth-sorted list (vsprsortedhead); the per-sprite vertical clip window Render/Segs feeds from
// the drawsegs (mfloorclip / mceilingclip / spryscale / sprtopscreen); the two constant clip
// helper arrays (negonearray = -1, screenheightarray = SCREENHEIGHT); and the player-sprite
// scale pair (pspritescale / pspriteiscale).
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these are the
// cross-read counterparts of SpriteScratch (Render/Things' file-local sprite scratch). All are
// externed in r_things.h; their r_things.cpp definitions and those externs become references onto
// these members (arrays as references-to-array). vissprite_p points into vissprites but is reset
// by clearSprites each frame; mfloorclip/mceilingclip are transient per-sprite views into a
// drawseg, always set before use - neither is a self-referential initializer. Live
// frame-golden-covered - every sprite the demos draw passes through these.
struct SpriteState
{
    static constexpr int maxVisSprites =
        128; // sizes vissprites; Render/Things guards on it

    Fixed pspritescale {}; // player-sprite horizontal scale
    Fixed pspriteiscale {}; // its inverse

    Array<short, SCREENWIDTH> negonearray = {}; // all -1 (a "no clip" top)
    Array<short, SCREENWIDTH> screenheightarray =
        {}; // all SCREENHEIGHT (a "no clip" bottom)

    Array<VisSprite, maxVisSprites> vissprites = {}; // the frame's gathered sprites
    VisSprite* vissprite_p = nullptr; // one past the last gathered
    VisSprite vsprsortedhead = {}; // sentinel head of the sorted list

    short* mfloorclip = nullptr; // per-column floor clip for the current sprite
    short* mceilingclip = nullptr; // per-column ceiling clip for the current sprite
    Fixed spryscale {}; // vertical scale of the current sprite column
    Fixed sprtopscreen {}; // screen y of the current sprite's top
};

SpriteState& spriteState();
} // namespace Doom
