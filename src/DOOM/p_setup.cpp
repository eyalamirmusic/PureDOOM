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
#include "p_mobj.h"
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
vertex_t* vertexes;

int numsegs;
seg_t* segs;

int numsectors;
sector_t* sectors;

int numsubsectors;
subsector_t* subsectors;

int numnodes;
node_t* nodes;

int numlines;
line_t* lines;

int numsides;
side_t* sides;

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
mobj_t** blocklinks;

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
mapthing_t (&deathmatchstarts)[MAX_DEATHMATCH_STARTS] = Doom::mapSpawns().deathmatchstarts;
mapthing_t*& deathmatch_p = Doom::mapSpawns().deathmatch_p;
mapthing_t (&playerstarts)[MAXPLAYERS] = Doom::mapSpawns().playerstarts;

void P_LoadVertexes(int lump)
{
    Doom::loadVertexes(lump);
}

void P_LoadSegs(int lump)
{
    Doom::loadSegs(lump);
}

void P_LoadSubsectors(int lump)
{
    Doom::loadSubsectors(lump);
}

void P_LoadSectors(int lump)
{
    Doom::loadSectors(lump);
}

void P_LoadNodes(int lump)
{
    Doom::loadNodes(lump);
}

void P_LoadThings(int lump)
{
    Doom::loadThings(lump);
}

void P_LoadLineDefs(int lump)
{
    Doom::loadLineDefs(lump);
}

void P_LoadSideDefs(int lump)
{
    Doom::loadSideDefs(lump);
}

void P_LoadBlockMap(int lump)
{
    Doom::loadBlockMap(lump);
}

void P_GroupLines()
{
    Doom::groupLines();
}

void P_SetupLevel(int episode, int map, int playermask, skill_t skill)
{
    Doom::setupLevel(episode, map, playermask, skill);
}

void P_Init()
{
    Doom::init();
}
