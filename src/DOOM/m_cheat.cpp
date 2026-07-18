// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Cheat sequence checking. Rewritten in UI/Cheat.{h,cpp}; this keeps the
//        cht_ names as shims. The scramble table is file-local to the rewritten
//        unit, so there is nothing to own here.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "m_cheat.h"

#include "UI/Cheat.h"


