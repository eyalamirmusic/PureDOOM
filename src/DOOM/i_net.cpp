// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        DOOM network seam. Rewritten in Host/Net.{h,cpp}; this keeps the
//        vanilla I_ names as shims.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "i_net.h"

#include "Host/Net.h"

void I_InitNetwork()
{
    Doom::I_InitNetwork();
}

void I_NetCmd()
{
    Doom::I_NetCmd();
}
