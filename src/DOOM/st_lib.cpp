// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        The status bar widget code. Rewritten in UI/StatusWidgets.{h,cpp}; this
//        keeps the STlib_ names as shims. The widgets carry their state in the
//        caller's structs, so there are no globals to own here.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "st_lib.h"

#include "UI/StatusWidgets.h"


void STlib_initNum(st_number_t* n, int x, int y, patch_t** pl, int* num,
                   doom_boolean* on, int width)
{
    Doom::initNum(*n, x, y, pl, num, on, width);
}

void STlib_updateNum(st_number_t* n, doom_boolean refresh)
{
    Doom::updateNum(*n, refresh);
}

void STlib_initPercent(st_percent_t* p, int x, int y, patch_t** pl, int* num,
                       doom_boolean* on, patch_t* percent)
{
    Doom::initPercent(*p, x, y, pl, num, on, percent);
}

void STlib_updatePercent(st_percent_t* per, int refresh)
{
    Doom::updatePercent(*per, refresh);
}

void STlib_initMultIcon(st_multicon_t* i, int x, int y, patch_t** il, int* inum,
                        doom_boolean* on)
{
    Doom::initMultIcon(*i, x, y, il, inum, on);
}

void STlib_updateMultIcon(st_multicon_t* mi, doom_boolean refresh)
{
    Doom::updateMultIcon(*mi, refresh);
}

void STlib_initBinIcon(st_binicon_t* b, int x, int y, patch_t* i,
                       doom_boolean* val, doom_boolean* on)
{
    Doom::initBinIcon(*b, x, y, i, val, on);
}

void STlib_updateBinIcon(st_binicon_t* bi, doom_boolean refresh)
{
    Doom::updateBinIcon(*bi, refresh);
}
