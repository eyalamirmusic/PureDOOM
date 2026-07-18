// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Level setup / loading. Rewritten in Sim/Setup.{h,cpp}; this keeps
//        P_SetupLevel and P_Init as shims and owns the geometry view-global storage.
//
//-----------------------------------------------------------------------------

#include "p_local.h"
#include "Sim/MobjTypes.h"
#include "r_defs.h"

#include "Game/MapSpawns.h"
#include "Sim/Setup.h"

#define MAX_DEATHMATCH_STARTS 10

// The level geometry view-globals (numvertexes/vertexes/... over Doom::Level's
// vectors), the blockmap views and the player/deathmatch starts. Read across the
// renderer and playsim; refreshed by the loaders in Sim/Setup.cpp. Storage here.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//
int numvertexes;
Doom::Vertex* vertexes;

int numsegs;
Doom::Seg* segs;

int numsectors;
Doom::Sector* sectors;

int numsubsectors;
Doom::SubSector* subsectors;

int numnodes;
Doom::Node* nodes;

int numlines;
Doom::Line* lines;

int numsides;
Doom::Side* sides;

// BLOCKMAP
// Created from axis aligned bounding box
// of the map, a rectangular array of
// blocks of size ...
// Used to speed up collision detection
// by spatial subdivision in 2D.
//
// Blockmap size.
int bmapwidth;
int bmapheight; // size in mapblocks
short* blockmap; // int for larger maps
// offsets in blockmap are from here
short* blockmaplump;
// origin of block map
fixed_t bmaporgx;
fixed_t bmaporgy;
// for thing chains
Doom::Mobj** blocklinks;

// REJECT
// For fast sight rejection.
// Speeds up enemy AI by skipping detailed
//  LineOf Sight calculation.
// Without special effect, this could be
//  used as a PVS lookup as well.
//
byte* rejectmatrix;

// The map's spawn spots are a Doom::MapSpawns owned by the Engine now; these are references
// onto it, the arrays as references-to-array (REFACTOR.md, Step 5).












