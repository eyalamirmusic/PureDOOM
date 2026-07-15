// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Gamma correction LUT and the patch/block blitters. Rewritten in
//        Render/Video.{h,cpp}; this keeps the V_ names as shims. The core video
//        state (the five screens, the dirty box, the gamma table) is defined at
//        file scope in Video.cpp (above its namespace), so there is nothing to
//        own here.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "r_defs.h" // patch_t
#include "v_video.h"

#include "Render/Video.h"

void V_MarkRect(int x, int y, int width, int height)
{
    Doom::vMarkRect(x, y, width, height);
}

void V_CopyRect(int srcx, int srcy, int srcscrn, int width, int height, int destx,
                int desty, int destscrn)
{
    Doom::vCopyRect(srcx, srcy, srcscrn, width, height, destx, desty, destscrn);
}

void V_DrawPatch(int x, int y, int scrn, patch_t* patch)
{
    Doom::vDrawPatch(x, y, scrn, patch);
}

void V_DrawPatchFlipped(int x, int y, int scrn, patch_t* patch)
{
    Doom::vDrawPatchFlipped(x, y, scrn, patch);
}

void V_DrawPatchRectDirect(int x, int y, int scrn, patch_t* patch, int src_x,
                           int src_w)
{
    Doom::vDrawPatchRectDirect(x, y, scrn, patch, src_x, src_w);
}

void V_DrawPatchDirect(int x, int y, int scrn, patch_t* patch)
{
    Doom::vDrawPatchDirect(x, y, scrn, patch);
}

void V_DrawBlock(int x, int y, int scrn, int width, int height, byte* src)
{
    Doom::vDrawBlock(x, y, scrn, width, height, src);
}

void V_GetBlock(int x, int y, int scrn, int width, int height, byte* dest)
{
    Doom::vGetBlock(x, y, scrn, width, height, dest);
}

void V_Init(void)
{
    Doom::vInit();
}
