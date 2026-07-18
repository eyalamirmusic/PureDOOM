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


#include "Render/Lighting.h"
#include "Render/Main.h"
#include "Render/RenderScratch.h"
#include "Render/ViewWindow.h"
#include "Render/ViewPoint.h"
#include "Render/ViewProjection.h"
#include "Sim/ValidCount.h"

// increment every time a check is made - a Doom::ValidCount owned by the Engine now
// (the one scalar owned by no subsystem); this vanilla name is a reference onto it.

// The light selection is a Doom::Lighting owned by the Engine now; these vanilla names
// are references onto it. fixedcolormap/extralight are set per frame by R_SetupFrame,
// the scalelight/zlight tables built once by R_InitLightTables.

// The screen projection is a Doom::ViewProjection owned by the Engine now; these
// vanilla names are references onto it. R_ExecuteSetViewSize (Render/Main.cpp) writes
// through them when the view size changes.


// The subsector counter is a Doom::RenderScratch member (an Engine member) now; a
// reference onto it.

// The view point (camera) is a Doom::ViewPoint owned by the Engine now; these
// vanilla names are references onto it for the renderer code still reading them as
// globals. R_SetupFrame (Render/Main.cpp) writes through them each frame.




// 0 = high, 1 = low. Part of the view-sizing state in Doom::ViewWindow now; a reference
// onto it.

//
// precalculated math tables (references onto the Engine's ViewProjection). The two
// tables are references-to-array, so their type is unchanged and every indexed read
// resolves exactly as before.
//

// The viewangletox[viewangle + FINEANGLES/4] lookup
// maps the visible view angles to screen X coordinates,
// flattening the arc to a flat projection plane.
// There will be many angles mapped to the same X.

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.


// References-to-array onto Doom::Lighting, so the type and every indexed read (the
// walllights = scalelight[light] row assignment included) are unchanged.

// bumped light from gun blasts

// The pending view-size request, stashed by R_SetViewSize (Render/Main.cpp) for
// R_ExecuteSetViewSize. Part of Doom::ViewWindow now; references onto it.




void (*colfunc)();
void (*basecolfunc)();
void (*fuzzcolfunc)();
void (*spanfunc)();


//
// R_AddPointToBox
// Expand a given bbox
// so that it encloses a given point.
//

















