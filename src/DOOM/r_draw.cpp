// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Column / span drawing. Rewritten in Render/Draw.{h,cpp}; this keeps the
//        R_ names as shims (r_main stores their addresses in the drawer pointers)
//        and owns the view geometry and the drawer input state (dc_*, ds_*) the
//        other renderer files fill in.
//
//-----------------------------------------------------------------------------

#include "r_local.h"

#include "Render/Draw.h"
#include "Render/DrawState.h"
#include "Render/ViewWindow.h"

//
// The view window geometry (set by r_main, read across the renderer and app). The
// storage is a Doom::ViewWindow owned by the Engine now; these vanilla names are
// references onto it.
//
int& viewwidth = Doom::viewWindow().viewwidth;
int& scaledviewwidth = Doom::viewWindow().scaledviewwidth;
int& viewheight = Doom::viewWindow().viewheight;
int& viewwindowx = Doom::viewWindow().viewwindowx;
int& viewwindowy = Doom::viewWindow().viewwindowy;

//
// The column/span drawer inputs are a Doom::DrawState owned by the Engine now; these are
// references onto its members (REFACTOR.md, Step 5).
//

//
// R_DrawColumn input: the caller (r_segs/r_plane/r_things) fills these in, the
// column drawers in Render/Draw.cpp read them.
//
lighttable_t*& dc_colormap = Doom::drawState().dc_colormap;
int& dc_x = Doom::drawState().dc_x;
int& dc_yl = Doom::drawState().dc_yl;
int& dc_yh = Doom::drawState().dc_yh;
fixed_t& dc_iscale = Doom::drawState().dc_iscale;
fixed_t& dc_texturemid = Doom::drawState().dc_texturemid;

// first pixel in a column (possibly virtual)
byte*& dc_source = Doom::drawState().dc_source;

// Translation tables for player-sprite recolouring (read by r_things).
byte*& dc_translation = Doom::drawState().dc_translation;
byte*& translationtables = Doom::drawState().translationtables;

//
// R_DrawSpan input: r_plane fills these in, the span drawers read them.
//
int& ds_y = Doom::drawState().ds_y;
int& ds_x1 = Doom::drawState().ds_x1;
int& ds_x2 = Doom::drawState().ds_x2;

lighttable_t*& ds_colormap = Doom::drawState().ds_colormap;

fixed_t& ds_xfrac = Doom::drawState().ds_xfrac;
fixed_t& ds_yfrac = Doom::drawState().ds_yfrac;
fixed_t& ds_xstep = Doom::drawState().ds_xstep;
fixed_t& ds_ystep = Doom::drawState().ds_ystep;

// start of a 64*64 tile image
byte*& ds_source = Doom::drawState().ds_source;


void R_DrawColumn(void)
{
    Doom::drawColumn();
}

void R_DrawColumnLow(void)
{
    Doom::drawColumnLow();
}

void R_DrawFuzzColumn(void)
{
    Doom::drawFuzzColumn();
}

void R_DrawTranslatedColumn(void)
{
    Doom::drawTranslatedColumn();
}

void R_InitTranslationTables(void)
{
    Doom::initTranslationTables();
}

void R_DrawSpan(void)
{
    Doom::drawSpan();
}

void R_DrawSpanLow(void)
{
    Doom::drawSpanLow();
}

void R_InitBuffer(int width, int height)
{
    Doom::initBuffer(width, height);
}

void R_FillBackScreen(void)
{
    Doom::fillBackScreen();
}

void R_VideoErase(unsigned ofs, int count)
{
    Doom::videoErase(ofs, count);
}

void R_DrawViewBorder(void)
{
    Doom::drawViewBorder();
}
