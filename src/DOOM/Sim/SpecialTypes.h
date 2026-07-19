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

#include <ea_data_structures/Structures/Array.h>

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
Doom::Sector* getNextSector(Doom::Line* line, Doom::Sector* sec);

//
// SPECIAL
//

//
// P_LIGHTS
//
namespace Doom
{
struct FireFlicker : Doom::Thinker
{
    void tick() override;
    Doom::ThinkerKind kind() const override
    {
        return Doom::ThinkerKind::FireFlicker;
    }
    Doom::Sector* sector;
    int count;
    int maxlight;
    int minlight;
};
} // namespace Doom

namespace Doom
{
struct LightFlash : Doom::Thinker
{
    void tick() override;
    Doom::ThinkerKind kind() const override { return Doom::ThinkerKind::LightFlash; }
    Doom::Sector* sector;
    int count;
    int maxlight;
    int minlight;
    int maxtime;
    int mintime;
};
} // namespace Doom

namespace Doom
{
struct Strobe : Doom::Thinker
{
    void tick() override;
    Doom::ThinkerKind kind() const override
    {
        return Doom::ThinkerKind::StrobeFlash;
    }
    Doom::Sector* sector;
    int count;
    int minlight;
    int maxlight;
    int darktime;
    int brighttime;
};
} // namespace Doom

namespace Doom
{
struct Glow : Doom::Thinker
{
    void tick() override;
    Doom::ThinkerKind kind() const override { return Doom::ThinkerKind::Glow; }
    Doom::Sector* sector;
    int minlight;
    int maxlight;
    int direction;
};
} // namespace Doom

namespace Doom
{
constexpr int GLOWSPEED = 8;
constexpr int STROBEBRIGHT = 5;
constexpr int FASTDARK = 15;
constexpr int SLOWDARK = 35;
} // namespace Doom

//
// P_SWITCH
//
namespace Doom
{
struct SwitchListEntry
{
    EA::Array<char, 9> name1;
    EA::Array<char, 9> name2;
    short episode;
};
} // namespace Doom

namespace Doom
{
enum ButtonWhere
{
    top,
    middle,
    bottom
};
} // namespace Doom

namespace Doom
{
struct Button
{
    Doom::Line* line;
    ButtonWhere where;
    int btexture;
    int btimer;
    Doom::Mobj* soundorg;
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
// P_PLATS
//
namespace Doom
{
enum PlatState
{
    up,
    down,
    waiting,
    in_stasis
};
} // namespace Doom

namespace Doom
{
enum PlatType
{
    perpetualRaise,
    downWaitUpStay,
    raiseAndChange,
    raiseToNearestAndChange,
    blazeDWUS
};
} // namespace Doom

namespace Doom
{
struct Plat : Doom::Thinker
{
    void tick() override;
    Doom::ThinkerKind kind() const override { return Doom::ThinkerKind::Plat; }
    Doom::Sector* sector;
    fixed_t speed;
    fixed_t low;
    fixed_t high;
    int wait;
    int count;
    PlatState status;
    PlatState oldstatus;
    bool crush;
    int tag;
    PlatType type;
};
} // namespace Doom

namespace Doom
{
constexpr int PLATWAIT = 3;
constexpr fixed_t PLATSPEED = FRACUNIT;
constexpr int MAXPLATS = 30;
} // namespace Doom

//
// P_DOORS
//
namespace Doom
{
enum DoorType
{
    door_normal,
    close30ThenOpen,
    door_close,
    door_open,
    raiseIn5Mins,
    blazeRaise,
    blazeOpen,
    blazeClose
};
} // namespace Doom

namespace Doom
{
struct Door : Doom::Thinker
{
    void tick() override;
    Doom::ThinkerKind kind() const override { return Doom::ThinkerKind::Door; }
    DoorType type;
    Doom::Sector* sector;
    fixed_t topheight;
    fixed_t speed;

    // 1 = up, 0 = waiting at top, -1 = down
    int direction;

    // tics to wait at the top
    int topwait;

    // (keep in case a door going down is reset)
    // when it reaches 0, start going down
    int topcountdown;
};
} // namespace Doom

namespace Doom
{
constexpr fixed_t VDOORSPEED = FRACUNIT * 2;
constexpr int VDOORWAIT = 150;
} // namespace Doom

//
// P_CEILNG
//
namespace Doom
{
enum CeilingType
{
    lowerToFloor,
    raiseToHighest,
    lowerAndCrush,
    crushAndRaise,
    fastCrushAndRaise,
    silentCrushAndRaise
};
} // namespace Doom

namespace Doom
{
struct Ceiling : Doom::Thinker
{
    void tick() override;
    Doom::ThinkerKind kind() const override { return Doom::ThinkerKind::Ceiling; }
    CeilingType type;
    Doom::Sector* sector;
    fixed_t bottomheight;
    fixed_t topheight;
    fixed_t speed;
    bool crush;

    // 1 = up, 0 = waiting, -1 = down
    int direction;

    // ID
    int tag;
    int olddirection;
};
} // namespace Doom

namespace Doom
{
constexpr fixed_t CEILSPEED = FRACUNIT;
constexpr int MAXCEILINGS = 30;
} // namespace Doom

//
// P_FLOOR
//
namespace Doom
{
enum FloorType
{
    // lower floor to highest surrounding floor
    lowerFloor,

    // lower floor to lowest surrounding floor
    lowerFloorToLowest,

    // lower floor to highest surrounding floor VERY FAST
    turboLower,

    // raise floor to lowest surrounding CEILING
    raiseFloor,

    // raise floor to next highest surrounding floor
    raiseFloorToNearest,

    // raise floor to shortest height texture around it
    raiseToTexture,

    // lower floor to lowest surrounding floor
    //  and change floorpic
    lowerAndChange,

    raiseFloor24,
    raiseFloor24AndChange,
    raiseFloorCrush,

    // raise to next highest floor, turbo-speed
    raiseFloorTurbo,
    donutRaise,
    raiseFloor512
};
} // namespace Doom

namespace Doom
{
enum StairType
{
    build8, // slowly build by 8
    turbo16 // quickly build by 16
};
} // namespace Doom

namespace Doom
{
struct FloorMove : Doom::Thinker
{
    void tick() override;
    Doom::ThinkerKind kind() const override { return Doom::ThinkerKind::Floor; }
    FloorType type;
    bool crush;
    Doom::Sector* sector;
    int direction;
    int newspecial;
    short texture;
    fixed_t floordestheight;
    fixed_t speed;
};
} // namespace Doom

namespace Doom
{
constexpr fixed_t FLOORSPEED = FRACUNIT;
} // namespace Doom

namespace Doom
{
enum MoveResult
{
    ok,
    crushed,
    pastdest
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
