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









