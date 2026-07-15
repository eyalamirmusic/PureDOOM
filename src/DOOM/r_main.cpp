// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Renderer main / setup. Rewritten in Render/Main.{h,cpp}; this keeps the R_
//        names as shims and owns the view-state and drawer-pointer globals.
//
//-----------------------------------------------------------------------------

#include "r_local.h"

#include "Render/Main.h"
#include "Render/ViewPoint.h"
#include "Render/ViewProjection.h"

int viewangleoffset;

// increment every time a check is made
int validcount = 1;

lighttable_t* fixedcolormap;

// The screen projection is a Doom::ViewProjection owned by the Engine now; these
// vanilla names are references onto it. R_ExecuteSetViewSize (Render/Main.cpp) writes
// through them when the view size changes.
int& centerx = Doom::viewProjection().centerx;
int& centery = Doom::viewProjection().centery;

fixed_t& centerxfrac = Doom::viewProjection().centerxfrac;
fixed_t& centeryfrac = Doom::viewProjection().centeryfrac;
fixed_t& projection = Doom::viewProjection().projection;

// just for profiling purposes

int sscount;
int linecount;
int loopcount;

// The view point (camera) is a Doom::ViewPoint owned by the Engine now; these
// vanilla names are references onto it for the renderer code still reading them as
// globals. R_SetupFrame (Render/Main.cpp) writes through them each frame.
fixed_t& viewx = Doom::viewPoint().viewx;
fixed_t& viewy = Doom::viewPoint().viewy;
fixed_t& viewz = Doom::viewPoint().viewz;

angle_t& viewangle = Doom::viewPoint().viewangle;

fixed_t& viewcos = Doom::viewPoint().viewcos;
fixed_t& viewsin = Doom::viewPoint().viewsin;

player_t*& viewplayer = Doom::viewPoint().viewplayer;

// 0 = high, 1 = low
int detailshift;

//
// precalculated math tables (references onto the Engine's ViewProjection). The two
// tables are references-to-array, so their type is unchanged and every indexed read
// resolves exactly as before.
//
angle_t& clipangle = Doom::viewProjection().clipangle;

// The viewangletox[viewangle + FINEANGLES/4] lookup
// maps the visible view angles to screen X coordinates,
// flattening the arc to a flat projection plane.
// There will be many angles mapped to the same X.
int (&viewangletox)[FINEANGLES / 2] = Doom::viewProjection().viewangletox;

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.
angle_t (&xtoviewangle)[SCREENWIDTH + 1] = Doom::viewProjection().xtoviewangle;


lighttable_t* scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t* scalelightfixed[MAXLIGHTSCALE];
lighttable_t* zlight[LIGHTLEVELS][MAXLIGHTZ];

// bumped light from gun blasts
int extralight;

doom_boolean setsizeneeded;
int setblocks;




void (*colfunc) (void);
void (*basecolfunc) (void);
void (*fuzzcolfunc) (void);
void (*spanfunc) (void);


//
// R_AddPointToBox
// Expand a given bbox
// so that it encloses a given point.
//

void R_AddPointToBox(int x, int y, fixed_t* box)
{
    Doom::addPointToBox(x, y, box);
}

int R_PointOnSide(fixed_t x, fixed_t y, node_t* node)
{
    return Doom::pointOnSide(x, y, node);
}

int R_PointOnSegSide(fixed_t x, fixed_t y, seg_t* line)
{
    return Doom::pointOnSegSide(x, y, line);
}

angle_t R_PointToAngle(fixed_t x, fixed_t y)
{
    return Doom::pointToAngle(x, y);
}

angle_t R_PointToAngle2(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
    return Doom::pointToAngle2(x1, y1, x2, y2);
}

fixed_t R_PointToDist(fixed_t x, fixed_t y)
{
    return Doom::pointToDist(x, y);
}

void R_InitPointToAngle(void)
{
    Doom::initPointToAngle();
}

fixed_t R_ScaleFromGlobalAngle(angle_t visangle)
{
    return Doom::scaleFromGlobalAngle(visangle);
}

void R_InitTables(void)
{
    Doom::initTables();
}

void R_InitTextureMapping(void)
{
    Doom::initTextureMapping();
}

void R_InitLightTables(void)
{
    Doom::initLightTables();
}

void R_SetViewSize(int blocks, int detail)
{
    Doom::setViewSize(blocks, detail);
}

void R_ExecuteSetViewSize(void)
{
    Doom::executeSetViewSize();
}

void R_Init(void)
{
    Doom::renderInit();
}

subsector_t* R_PointInSubsector(fixed_t x, fixed_t y)
{
    return Doom::pointInSubsector(x, y);
}

void R_SetupFrame(player_t* player)
{
    Doom::setupFrame(player);
}

void R_RenderPlayerView(player_t* player)
{
    Doom::renderPlayerView(player);
}
