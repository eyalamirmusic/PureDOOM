// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Sky rendering. Rewritten in Render/Sky.{h,cpp}; this keeps R_InitSkyMap as
//        a shim and owns the sky globals (read across the renderer and the shooting
//        code).
//
//-----------------------------------------------------------------------------

#include "r_sky.h"

#include "Render/Sky.h"

int skyflatnum;
int skytexture;
int skytexturemid;

void R_InitSkyMap(void)
{
    Doom::initSkyMap();
}
