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
// DESCRIPTION:  none
//        Implements special effects:
//        Doom::Texture animation, height or lighting changes
//         according to adjacent sectors, respective
//         utility functions, etc.
//
//-----------------------------------------------------------------------------

#pragma once

#include "MobjTypes.h"
#include "MapTypes.h"
#include "../Render/RenderTypes.h"

// The special thinkers themselves (their structs, enums and timing constants) live
// one-per-file under Thinkers/, each carrying its own tick(). This header stays the
// umbrella the p_spec family (Sim/Lights, Plats, Doors, Ceilings, Floors and their
// callers) includes, so it re-exports them and adds the shared helpers below.
#include "../Thinkers/FireFlicker.h"
#include "../Thinkers/LightFlash.h"
#include "../Thinkers/Strobe.h"
#include "../Thinkers/Glow.h"
#include "../Thinkers/Plat.h"
#include "../Thinkers/Door.h"
#include "../Thinkers/Ceiling.h"
#include "../Thinkers/FloorMove.h"

#include "../Containers.h"

#include <string_view>

//
// End-level timer (-TIMER option)
//
// The end-level timer is a Doom::EndLevelTimer owned by the Engine now; these are references
// onto its members (REFACTOR.md, Step 5).

// at game start

// at map load

// every tic

// when needed

int twoSided(int sector, int line);
Doom::Sector* getSector(int currentSector, int line, int side);
Doom::Side* getSide(int currentSector, int line, int side);
Doom::Sector* getNextSector(Doom::Line& line, Doom::Sector& sec);

//
// SPECIAL
//

//
// P_SWITCH
//
namespace Doom
{
struct SwitchListEntry
{
    std::string_view name1;
    std::string_view name2;
    short episode;
};
} // namespace Doom

namespace Doom
{
enum class ButtonWhere
{
    Top,
    Middle,
    Bottom
};
} // namespace Doom

namespace Doom
{
struct Button
{
    Line* line;
    ButtonWhere where;
    int btexture;
    int btimer;
    Mobj* soundorg;
};
} // namespace Doom

namespace Doom
{
// max # of wall switches in a level

// 4 players, 4 buttons each at once, max.
constexpr int MAXBUTTONS = 16;

// 1 second, in ticks.
constexpr int BUTTONTIME = 35;
} // namespace Doom

// The active-special registries are a Doom::ActiveSpecials owned by the Engine now; these
// (and activeplats/activeceilings below) are references onto its members (REFACTOR.md, Step 5).

//
// P_FLOOR
//
namespace Doom
{
// The result of one movePlane step, shared by the floor, ceiling, plat and door
// thinkers (Floors.cpp defines movePlane; the movers read its verdict).
enum class MoveResult
{
    Ok,
    Crushed,
    PastDest
};
} // namespace Doom

//
// P_TELEPT
//

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
