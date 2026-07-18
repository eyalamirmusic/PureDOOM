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

#include "Game/SkyState.h"
#include "Render/Sky.h"

// skyflatnum, skytexture and skytexturemid are a Doom::SkyState owned by the Engine now; these
// are references onto its members (REFACTOR.md, Step 5).
int& skyflatnum = Doom::skyState().skyflatnum;
int& skytexture = Doom::skyState().skytexture;
int& skytexturemid = Doom::skyState().skytexturemid;

void R_InitSkyMap()
{
    Doom::initSkyMap();
}
