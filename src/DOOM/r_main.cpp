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

#include "Render/Lighting.h"
#include "Render/Main.h"
#include "Render/RenderScratch.h"
#include "Render/ViewWindow.h"
#include "Render/ViewPoint.h"
#include "Render/ViewProjection.h"
#include "Sim/ValidCount.h"

// increment every time a check is made - a Doom::ValidCount owned by the Engine now
// (the one scalar owned by no subsystem); this vanilla name is a reference onto it.
int& validcount = Doom::validCount().validcount;

// The light selection is a Doom::Lighting owned by the Engine now; these vanilla names
// are references onto it. fixedcolormap/extralight are set per frame by R_SetupFrame,
// the scalelight/zlight tables built once by R_InitLightTables.
lighttable_t*& fixedcolormap = Doom::lighting().fixedcolormap;

// The screen projection is a Doom::ViewProjection owned by the Engine now; these
// vanilla names are references onto it. R_ExecuteSetViewSize (Render/Main.cpp) writes
// through them when the view size changes.
int& centerx = Doom::viewProjection().centerx;
int& centery = Doom::viewProjection().centery;

fixed_t& centerxfrac = Doom::viewProjection().centerxfrac;
fixed_t& centeryfrac = Doom::viewProjection().centeryfrac;
fixed_t& projection = Doom::viewProjection().projection;

// The subsector counter is a Doom::RenderScratch member (an Engine member) now; a
// reference onto it.
int& sscount = Doom::renderScratch().sscount;

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

// 0 = high, 1 = low. Part of the view-sizing state in Doom::ViewWindow now; a reference
// onto it.
int& detailshift = Doom::viewWindow().detailshift;

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


// References-to-array onto Doom::Lighting, so the type and every indexed read (the
// walllights = scalelight[light] row assignment included) are unchanged.
lighttable_t* (&scalelight)[LIGHTLEVELS][MAXLIGHTSCALE] = Doom::lighting().scalelight;
lighttable_t* (&scalelightfixed)[MAXLIGHTSCALE] = Doom::lighting().scalelightfixed;
lighttable_t* (&zlight)[LIGHTLEVELS][MAXLIGHTZ] = Doom::lighting().zlight;

// bumped light from gun blasts
int& extralight = Doom::lighting().extralight;

// The pending view-size request, stashed by R_SetViewSize (Render/Main.cpp) for
// R_ExecuteSetViewSize. Part of Doom::ViewWindow now; references onto it.
doom_boolean& setsizeneeded = Doom::viewWindow().setsizeneeded;
int& setblocks = Doom::viewWindow().setblocks;




void (*colfunc)();
void (*basecolfunc)();
void (*fuzzcolfunc)();
void (*spanfunc)();


//
// R_AddPointToBox
// Expand a given bbox
// so that it encloses a given point.
//
















void R_SetupFrame(player_t* player)
{
    Doom::setupFrame(*player);
}

void R_RenderPlayerView(player_t* player)
{
    Doom::renderPlayerView(*player);
}
