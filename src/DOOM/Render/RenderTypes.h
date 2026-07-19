// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//      Refresh/rendering module, shared data struct definitions.
//
//-----------------------------------------------------------------------------

#pragma once

// The renderer's own types: a patch and the posts/columns it is stored as, the
// silhouette-clipped drawseg a wall segment becomes, the vissprite a thing
// becomes, the sprite frame/definition tables, and the visplane a floor or
// ceiling becomes. The map geometry is in Sim/MapTypes.h.

#include "../Game/GameDefs.h"
#include "../Math/FixedPoint.h"
#include "../Sim/MapTypes.h"

#include <ea_data_structures/Structures/Vector.h>

namespace Doom
{
// Drawseg silhouette flags: which edges of a wall segment clip the sprites
// behind it. SIL_BOTH is the two OR'd together, so these combine bitwise.
constexpr int SIL_NONE = 0;
constexpr int SIL_BOTTOM = 1;
constexpr int SIL_TOP = 2;
constexpr int SIL_BOTH = 3;
} // namespace Doom

// The drawseg pool's size is BSPScratch::maxDrawSegs (Render/BSPScratch.h),
// which is what sizes the array. Bound-check against that, not against a
// separate constant of the same value.

// posts are runs of non masked source pixels
namespace Doom
{
struct Post
{
    byte topdelta; // -1 is the last post in a column
    byte length; // length data bytes follows
};
} // namespace Doom

namespace Doom
{
// A Column is a list of 0 or more Post, (byte)-1 terminated
using Column = Post;
} // namespace Doom

//
// OTHER TYPES
//

// This could be wider for >8 bit display.
// Indeed, true color support is posibble
//  precalculating 24bpp lightmap/colormap LUT.
//  from darkening PLAYPAL to all black.
// Could even us emore than 32 levels.
namespace Doom
{
using LightTable = byte;
} // namespace Doom

//
// ?
//
namespace Doom
{
struct DrawSeg
{
    Seg* curline;
    int x1;
    int x2;

    fixed_t scale1;
    fixed_t scale2;
    fixed_t scalestep;

    // 0=none, 1=bottom, 2=top, 3=both
    int silhouette;

    // do not clip sprites above this
    fixed_t bsilheight;

    // do not clip sprites below this
    fixed_t tsilheight;

    // Pointers to lists for sprite clipping,
    //  all three adjusted so [x1] is first value.
    short* sprtopclip;
    short* sprbottomclip;
    short* maskedtexturecol;
};
} // namespace Doom

// Patches.
// A patch holds one or more columns.
// Patches are used for sprites and all masked pictures,
// and we compose textures from the TEXTURE1/2 lists
// of patches.
namespace Doom
{
struct Patch
{
    short width; // bounding box size
    short height;
    short leftoffset; // pixels to the left of origin
    short topoffset; // pixels below the origin
    int columnofs[8]; // only [width] used
    // the [0] is &columnofs[width]
};
} // namespace Doom

// A Doom::VisSprite is a thing
//  that will be drawn during a refresh.
// I.e. a sprite object that is partly visible.
namespace Doom
{
struct VisSprite
{
    // Doubly linked list.
    struct VisSprite* prev;
    struct VisSprite* next;

    int x1;
    int x2;

    // for line side calculation
    fixed_t gx;
    fixed_t gy;

    // global bottom / top for silhouette clipping
    fixed_t gz;
    fixed_t gzt;

    // horizontal position of x1
    fixed_t startfrac;

    fixed_t scale;

    // negative if flipped
    fixed_t xiscale;

    fixed_t texturemid;
    int patch;

    // for color translation and shadow draw,
    //  maxbright frames as well
    LightTable* colormap;

    int mobjflags;
};
} // namespace Doom

//
// Sprites are patches with a special naming convention
//  so they can be recognized by Doom::initSprites.
// The base name is NNNNFx or NNNNFxFx, with
//  x indicating the rotation, x = 0, 1-7.
// The sprite and frame specified by a thing_t
//  is range checked at run time.
// A sprite is a Doom::Patch that is assumed to represent
//  a three dimensional object and may have multiple
//  rotations pre drawn.
// Horizontal flipping is used to save space,
//  thus NNNNF2F5 defines a mirrored patch.
// Some sprites will only have one picture used
// for all views: NNNNF0
//
namespace Doom
{
struct SpriteFrame
{
    // If 0, use lump 0 for any position - the sprite is drawn the same from
    // every angle. If 1, the eight rotations are all present.
    //
    // Not a boolean, despite vanilla's type: R_InitSpriteDefs memsets sprtemp to
    // -1, and -1 means "no lump seen for this frame yet". R_InstallSpriteLump
    // relies on the third state - it tests `== false` and `== true` separately,
    // and a frame that is still -1 must match neither. As a C++ bool the -1 would
    // read back as true, every sprite with rotations would look like a frame that
    // already had a rot=0 lump, and the engine would Doom::fatalError on the very first
    // one it loaded (TROO frame I).
    int rotate;

    // Lump to use for view angles 0-7.
    short lump[8];

    // Flip bit (1 = flip) to use for view angles 0-7.
    byte flip[8];
};
} // namespace Doom

//
// A sprite definition:
//  a number of animation frames.
//
namespace Doom
{
struct SpriteDef
{
    int numframes;
    // The animation frames, RAII-owned (Step 9): was a raw malloc'd SpriteFrame*,
    // now an owned vector freed with the sprite. Readers index it as before
    // (spriteframes[frame], &spriteframes[frame]).
    EA::Vector<SpriteFrame> spriteframes;
};
} // namespace Doom

//
// Now what is a visplane, anyway?
//
namespace Doom
{
struct VisPlane
{
    fixed_t height;
    int picnum;
    int lightlevel;
    int minx;
    int maxx;

    // leave pads for [minx-1]/[maxx+1]

    byte pad1;
    // Here lies the rub for all
    //  dynamic resize/change of resolution.
    byte top[SCREENWIDTH];
    byte pad2;
    byte pad3;
    // See above.
    byte bottom[SCREENWIDTH];
    byte pad4;
};
} // namespace Doom

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
