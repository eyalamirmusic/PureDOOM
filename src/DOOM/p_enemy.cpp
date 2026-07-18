// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// DESCRIPTION:
//        Enemy thinking, AI. Rewritten in Sim/Enemy.{h,cpp}; this file now only
//        owns the soundtarget global that p_saveg archives. The A_* actions moved
//        to Sim/Actions.{h,cpp} (Doom::Actions::*), unprefixed, once nothing
//        outside Sim/Info.cpp needed their global names.
//
//-----------------------------------------------------------------------------

#include "d_player.h"
#include "p_local.h"
#include "p_mobj.h"

#include "Sim/Enemy.h"
#include "Sim/SoundTarget.h"

// The last thing that made noise, propagated to nearby monsters. A Doom::SoundTarget owned by the
// Engine now (Sim/SoundTarget.h); this is a reference onto it. (The old "p_saveg archives it" note
// was wrong - p_saveg only touches Doom::Sector::soundtarget, never this global.) Written by
// Doom::noiseAlert / recursiveSound.
Doom::Mobj*& soundtarget = Doom::soundTarget().soundtarget;
