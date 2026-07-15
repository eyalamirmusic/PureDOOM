#pragma once

#include "../r_defs.h" // patch_t

namespace Doom
{
// Low-level framebuffer drawing; v_video.cpp keeps the vanilla V_ names as shims.
void vMarkRect(int x, int y, int width, int height);
void vCopyRect(int srcx,
               int srcy,
               int srcscrn,
               int width,
               int height,
               int destx,
               int desty,
               int destscrn);
void vDrawPatch(int x, int y, int scrn, patch_t* patch);
void vDrawPatchFlipped(int x, int y, int scrn, patch_t* patch);
void vDrawPatchRectDirect(
    int x, int y, int scrn, patch_t* patch, int src_x, int src_w);
void vDrawPatchDirect(int x, int y, int scrn, patch_t* patch);
void vDrawBlock(int x, int y, int scrn, int width, int height, byte* src);
void vGetBlock(int x, int y, int scrn, int width, int height, byte* dest);
void vInit(void);
} // namespace Doom
