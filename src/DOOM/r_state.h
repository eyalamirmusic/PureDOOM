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
//        Refresh/render internal state variables (global).
//
//-----------------------------------------------------------------------------

#ifndef __R_STATE__
#define __R_STATE__


#include "d_player.h" // Need data structure definitions.
#include "r_defs.h"


//
// Refresh internal data structures,
// for rendering.
//

// The renderer's loaded graphics data lives in Doom::GraphicsData (an Engine member)
// now; these are references onto it (REFACTOR.md, Step 5).

// needed for texture pegging
extern fixed_t*& textureheight;

// needed for pre rendering (fracs). Plain-pointer views onto GraphicsData's owned
// EA::Vectors (Step 9), refreshed by initSpriteLumps - like vertexes/segs onto Level.
extern fixed_t* spritewidth;

extern fixed_t* spriteoffset;
extern fixed_t* spritetopoffset;

extern lighttable_t*& colormaps;

// View window geometry: references onto Doom::ViewWindow (an Engine member).
extern int& viewwidth;
extern int& scaledviewwidth;
extern int& viewheight;

extern int& firstflat;

// for global animation
extern int*& flattranslation;
extern int*& texturetranslation;

// Sprite....
extern int& firstspritelump;
extern int& lastspritelump;
extern int& numspritelumps;

//
// Lookup tables for map data.
//
extern int& numsprites;
extern spritedef_t*& sprites;

extern int numvertexes;
extern vertex_t* vertexes;

extern int numsegs;
extern seg_t* segs;

extern int numsectors;
extern sector_t* sectors;

extern int numsubsectors;
extern subsector_t* subsectors;

extern int numnodes;
extern node_t* nodes;

extern int numlines;
extern line_t* lines;

extern int numsides;
extern side_t* sides;

//
// POV data. The storage lives in Doom::ViewPoint (an Engine member); these names are
// references onto it while the renderer still reads them as globals (REFACTOR.md,
// Step 5). They resolve to viewPoint().<member> once each reader takes an Engine&.
//
extern fixed_t& viewx;
extern fixed_t& viewy;
extern fixed_t& viewz;

extern angle_t& viewangle;
extern player_t*& viewplayer;

// The screen projection also lives in Doom::ViewProjection (an Engine member); these
// are references onto it, the two tables as references-to-array so their type and every
// indexed read are unchanged.
extern angle_t& clipangle;

extern int (&viewangletox)[FINEANGLES / 2];
extern angle_t (&xtoviewangle)[SCREENWIDTH + 1];

// The frame's transient render scratch lives in Doom::RenderScratch (an Engine member)
// now; these are references onto it (REFACTOR.md, Step 5).
extern fixed_t& rw_distance;
extern angle_t& rw_normalangle;

// angle to line origin
extern int& rw_angle1;

// Segs count?
extern int& sscount;

extern visplane_t*& floorplane;
extern visplane_t*& ceilingplane;


#endif

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
