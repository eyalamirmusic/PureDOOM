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

// Views onto the tables in Math/Trig.h, which owns them. Pointers rather than
// arrays, because there is one copy of the data and this is the vanilla name for
// it - indexing them reads exactly what it always did.

// Effective size is 10240.
extern const fixed_t* finesine;

// Re-use data, is just PI/2 pahse shift.
extern const fixed_t* finecosine;

// Effective size is 4096.
extern const fixed_t* finetangent;

// angle_t IS Doom::Angle - Binary Angle Measurement, a whole turn in 2^32 units
// so it wraps by itself. The ang* constants (Math/Angle.h) are Angles, which is
// what lets the engine keep writing `ang45 * (thing->angle / 45)` unchanged.
using angle_t = Doom::Angle;

// Effective size is 2049;
// The +1 size is to handle the case when x==y
//  without additional checking.
extern const angle_t* tantoangle;

// Utility function,
//  called by Doom::pointToAngle.

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
