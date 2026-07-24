// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//        Lookup tables.
//        Do not try to look them up :-).
//        In the order of appearance:
//
//        int finetangent[4096]        - Tangens LUT.
//         Should work with BAM fairly well (12 of 16bit,
//      effectively, by shifting).
//
//        int finesine[10240]                - Sine lookup.
//         Guess what, serves as cosine, too.
//         Remarkable thing is, how to use BAMs with this?
//
//        int tantoangle[2049]        - ArcTan LUT,
//          maps tan(angle) to angle fast. Gotta search.
//
//-----------------------------------------------------------------------------

#pragma once

#include "FixedPoint.h"
#include "Angle.h"
#include "Trig.h"

// This header is the vanilla-named view of the trig tables and nothing else.
// The constants that go with them - fineAngles, fineMask, slopeRange, slopeBits,
// slopeToFixedShift, ang45/ang90/ang180/ang270, Angle::angleToFineShift - belong
// to Math/Trig.h and Math/Angle.h. Read them from there.

// finesine() / finecosine() / finetangent() / tantoangle() - the typed views onto
// the trig tables - are declared in Math/Trig.h, included above. They used to be
// four `extern const T*` globals spelled here; the engine reads them as
// finesine()[i] now, and Doom::Angle is spelled by its own name (this header used
// to alias it `angle_t`).

// Utility function,
//  called by Doom::pointToAngle.

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
