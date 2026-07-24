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
// P_LIGHTS
//
namespace Doom
{
struct FireFlicker : Thinker
{
    void tick() override;
    ThinkerKind kind() const override { return ThinkerKind::FireFlicker; }
    Sector* sector;
    int count;
    int maxlight;
    int minlight;
};
} // namespace Doom

namespace Doom
{
struct LightFlash : Thinker
{
    void tick() override;
    ThinkerKind kind() const override { return ThinkerKind::LightFlash; }
    Sector* sector;
    int count;
    int maxlight;
    int minlight;
    int maxtime;
    int mintime;
};
} // namespace Doom

namespace Doom
{
struct Strobe : Thinker
{
    void tick() override;
    ThinkerKind kind() const override { return ThinkerKind::StrobeFlash; }
    Sector* sector;
    int count;
    int minlight;
    int maxlight;
    int darktime;
    int brighttime;
};
} // namespace Doom

namespace Doom
{
struct Glow : Thinker
{
    void tick() override;
    ThinkerKind kind() const override { return ThinkerKind::Glow; }
    Sector* sector;
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
// P_PLATS
//
namespace Doom
{
enum class PlatState
{
    Up,
    Down,
    Waiting,
    InStasis
};
} // namespace Doom

namespace Doom
{
enum class PlatType
{
    PerpetualRaise,
    DownWaitUpStay,
    RaiseAndChange,
    RaiseToNearestAndChange,
    BlazeDWUS
};
} // namespace Doom

namespace Doom
{
struct Plat : Thinker
{
    void tick() override;
    ThinkerKind kind() const override { return ThinkerKind::Plat; }
    Sector* sector;
    Fixed speed;
    Fixed low;
    Fixed high;
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
constexpr Fixed PLATSPEED = FRACUNIT;
constexpr int MAXPLATS = 30;
} // namespace Doom

//
// P_DOORS
//
namespace Doom
{
enum class DoorType
{
    Normal,
    Close30ThenOpen,
    Close,
    Open,
    RaiseIn5Mins,
    BlazeRaise,
    BlazeOpen,
    BlazeClose
};
} // namespace Doom

namespace Doom
{
struct Door : Thinker
{
    void tick() override;
    ThinkerKind kind() const override { return ThinkerKind::Door; }
    DoorType type;
    Sector* sector;
    Fixed topheight;
    Fixed speed;

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
constexpr Fixed VDOORSPEED = FRACUNIT * 2;
constexpr int VDOORWAIT = 150;
} // namespace Doom

//
// P_CEILNG
//
namespace Doom
{
enum class CeilingType
{
    LowerToFloor,
    RaiseToHighest,
    LowerAndCrush,
    CrushAndRaise,
    FastCrushAndRaise,
    SilentCrushAndRaise
};
} // namespace Doom

namespace Doom
{
struct Ceiling : Thinker
{
    void tick() override;
    ThinkerKind kind() const override { return ThinkerKind::Ceiling; }
    CeilingType type;
    Sector* sector;
    Fixed bottomheight;
    Fixed topheight;
    Fixed speed;
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
constexpr Fixed CEILSPEED = FRACUNIT;
constexpr int MAXCEILINGS = 30;
} // namespace Doom

//
// P_FLOOR
//
namespace Doom
{
enum class FloorType
{
    // lower floor to highest surrounding floor
    LowerFloor,

    // lower floor to lowest surrounding floor
    LowerFloorToLowest,

    // lower floor to highest surrounding floor VERY FAST
    TurboLower,

    // raise floor to lowest surrounding CEILING
    RaiseFloor,

    // raise floor to next highest surrounding floor
    RaiseFloorToNearest,

    // raise floor to shortest height texture around it
    RaiseToTexture,

    // lower floor to lowest surrounding floor
    //  and change floorpic
    LowerAndChange,

    RaiseFloor24,
    RaiseFloor24AndChange,
    RaiseFloorCrush,

    // raise to next highest floor, turbo-speed
    RaiseFloorTurbo,
    DonutRaise,
    RaiseFloor512
};
} // namespace Doom

namespace Doom
{
enum class StairType
{
    Build8, // slowly build by 8
    Turbo16 // quickly build by 16
};
} // namespace Doom

namespace Doom
{
struct FloorMove : Thinker
{
    void tick() override;
    ThinkerKind kind() const override { return ThinkerKind::Floor; }
    FloorType type;
    bool crush;
    Sector* sector;
    int direction;
    int newspecial;
    short texture;
    Fixed floordestheight;
    Fixed speed;
};
} // namespace Doom

namespace Doom
{
constexpr Fixed FLOORSPEED = FRACUNIT;
} // namespace Doom

namespace Doom
{
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
