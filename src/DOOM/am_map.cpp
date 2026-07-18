// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        The automap. Rewritten in UI/Automap.{h,cpp}; this keeps the AM_ names
//        as shims and owns the state the GPU automap path (EngineAccess) reads:
//        the player/thing shapes, the map window (m_x/m_y/m_w/m_h, scale_mtof),
//        the frame window (f_x/f_y/f_w/f_h), am_plr, followplayer, cheating, grid,
//        lightlev, and automapactive.
//
//-----------------------------------------------------------------------------

#include "Host/Platform.h"

#include "Game/GameDefs.h"
#include "Game/MapSpawns.h" // automapactive
#include "Sim/SimDefs.h"  // PLAYERRADIUS
#include "UI/AutomapTypes.h"

#include "Game/OverlayState.h"
#include "UI/Automap.h"

// The vector graphics for the automap (the GPU automap path reads these shapes).
#define R ((8 * PLAYERRADIUS) / 7)
Doom::MapLine player_arrow[] = {
    { { -R + R / 8, 0 }, { R, 0 } }, // -----
    { { R, 0 }, { R - R / 2, R / 4 } },  // ----->
    { { R, 0 }, { R - R / 2, -R / 4 } },
    { { -R + R / 8, 0 }, { -R - R / 8, R / 4 } }, // >---->
    { { -R + R / 8, 0 }, { -R - R / 8, -R / 4 } },
    { { -R + 3 * R / 8, 0 }, { -R + R / 8, R / 4 } }, // >>--->
    { { -R + 3 * R / 8, 0 }, { -R + R / 8, -R / 4 } }
};
#undef R

#define R ((8 * PLAYERRADIUS) / 7)
Doom::MapLine cheat_player_arrow[] = {
    { { -R + R / 8, 0 }, { R, 0 } }, // -----
    { { R, 0 }, { R - R / 2, R / 6 } },  // ----->
    { { R, 0 }, { R - R / 2, -R / 6 } },
    { { -R + R / 8, 0 }, { -R - R / 8, R / 6 } }, // >----->
    { { -R + R / 8, 0 }, { -R - R / 8, -R / 6 } },
    { { -R + 3 * R / 8, 0 }, { -R + R / 8, R / 6 } }, // >>----->
    { { -R + 3 * R / 8, 0 }, { -R + R / 8, -R / 6 } },
    { { -R / 2, 0 }, { -R / 2, -R / 6 } }, // >>-d--->
    { { -R / 2, -R / 6 }, { -R / 2 + R / 6, -R / 6 } },
    { { -R / 2 + R / 6, -R / 6 }, { -R / 2 + R / 6, R / 4 } },
    { { -R / 6, 0 }, { -R / 6, -R / 6 } }, // >>-dd-->
    { { -R / 6, -R / 6 }, { 0, -R / 6 } },
    { { 0, -R / 6 }, { 0, R / 4 } },
    { { R / 6, R / 4 }, { R / 6, -R / 7 } }, // >>-ddt->
    { { R / 6, -R / 7 }, { R / 6 + R / 32, -R / 7 - R / 32 } },
    { { R / 6 + R / 32, -R / 7 - R / 32 }, { R / 6 + R / 10, -R / 7 } }
};
#undef R

#define R (FRACUNIT)
Doom::MapLine thintriangle_guy[] = {
    { { (fixed_t)(-.5 * R), (fixed_t)(-.7 * R) }, { R, 0 } },
    { { R, 0 }, { (fixed_t)(-.5 * R), (fixed_t)(.7 * R) } },
    { { (fixed_t)(-.5 * R), (fixed_t)(.7 * R) }, { (fixed_t)(-.5 * R), (fixed_t)(-.7 * R) } }
};
#undef R

// Map-window position/size and scale (map coords), read by the GPU automap.
fixed_t m_x, m_y;
fixed_t m_w;
fixed_t m_h;

#define INITSCALEMTOF (.2 * FRACUNIT)
fixed_t scale_mtof = (fixed_t) INITSCALEMTOF;

// Frame-window position/size (screen coords).
int f_x;
int f_y;
int f_w;
int f_h;

Doom::Player* am_plr; // the player represented by an arrow
int followplayer = 1; // whether to follow the player around
int cheating = 0;
int grid = 0;
int lightlev; // used for funky strobing effect

// automapactive (with menuactive) is a Doom::OverlayState owned by the Engine now; this is a
// reference onto it (REFACTOR.md, Step 5).







