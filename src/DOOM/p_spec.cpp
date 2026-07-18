// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Special effects coordinator. Rewritten in Sim/Specials.{h,cpp}; this keeps
//        the vanilla names as shims and owns levelTimer/levelTimeCount.
//
//-----------------------------------------------------------------------------

#include "p_local.h"

#include "Sim/EndLevelTimer.h"
#include "Sim/Specials.h"

// levelTimer/levelTimeCount are declared in p_spec.h (the -TIMER option); they are members of
// the Doom::EndLevelTimer owned by the Engine now, and these vanilla names references onto it.
doom_boolean& levelTimer = Doom::endLevelTimer().levelTimer;
int& levelTimeCount = Doom::endLevelTimer().levelTimeCount;














