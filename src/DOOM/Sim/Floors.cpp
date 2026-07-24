// Rewritten out of vanilla p_floor into namespace Doom.
//
// Floor movement: movePlane (the shared height-mover the ceiling, plat and door
// thinkers all call), the moveFloor thinker, and the EV_ floor/stairs handlers.
// T_MovePlane stays global because other specials call it. p_floor.cpp shims every
// name. Golden-neutral - the demos lower floors and build stairs.

#include "../Host/Platform.h"

#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h"
#include "SimDefs.h"
#include "../Game/SoundData.h"

#include "Floors.h"
#include "Tick.h" // levelAlloc / levelFree / freeLevelAllocations
#include "Specials.h"

#include <new>

#include "../Game/LevelStats.h"
#include "../Game/Sound.h"
#include "MapAction.h"

#include "../Render/GraphicsData.h"
#include "../Sim/Level.h"
namespace Doom
{
// Forward declarations so the file's own call order needs no rearranging.
MoveResult movePlane(Sector& sector,
                     Fixed speed,
                     Fixed dest,
                     bool crush,
                     int floorOrCeiling,
                     int direction);
int doFloor(Line& line, FloorType floortype);
int buildStairs(Line& line, StairType type);

MoveResult movePlane(Sector& sector,
                     Fixed speed,
                     Fixed dest,
                     bool crush,
                     int floorOrCeiling,
                     int direction)
{
    bool flag;
    Fixed lastpos;

    switch (floorOrCeiling)
    {
        case 0:
            // FLOOR
            switch (direction)
            {
                case -1:
                    // DOWN
                    if (sector.floorheight - speed < dest)
                    {
                        lastpos = sector.floorheight;
                        sector.floorheight = dest;
                        flag = sector.changeSector(crush);
                        if (flag == true)
                        {
                            sector.floorheight = lastpos;
                            sector.changeSector(crush);
                            //return MoveResult::Crushed;
                        }
                        return MoveResult::PastDest;
                    }
                    else
                    {
                        lastpos = sector.floorheight;
                        sector.floorheight -= speed;
                        flag = sector.changeSector(crush);
                        if (flag == true)
                        {
                            sector.floorheight = lastpos;
                            sector.changeSector(crush);
                            return MoveResult::Crushed;
                        }
                    }
                    break;

                case 1:
                    // UP
                    if (sector.floorheight + speed > dest)
                    {
                        lastpos = sector.floorheight;
                        sector.floorheight = dest;
                        flag = sector.changeSector(crush);
                        if (flag == true)
                        {
                            sector.floorheight = lastpos;
                            sector.changeSector(crush);
                            //return MoveResult::Crushed;
                        }
                        return MoveResult::PastDest;
                    }
                    else
                    {
                        // COULD GET CRUSHED
                        lastpos = sector.floorheight;
                        sector.floorheight += speed;
                        flag = sector.changeSector(crush);
                        if (flag == true)
                        {
                            if (crush == true)
                                return MoveResult::Crushed;
                            sector.floorheight = lastpos;
                            sector.changeSector(crush);
                            return MoveResult::Crushed;
                        }
                    }
                    break;
            }
            break;

        case 1:
            // CEILING
            switch (direction)
            {
                case -1:
                    // DOWN
                    if (sector.ceilingheight - speed < dest)
                    {
                        lastpos = sector.ceilingheight;
                        sector.ceilingheight = dest;
                        flag = sector.changeSector(crush);

                        if (flag == true)
                        {
                            sector.ceilingheight = lastpos;
                            sector.changeSector(crush);
                            //return MoveResult::Crushed;
                        }
                        return MoveResult::PastDest;
                    }
                    else
                    {
                        // COULD GET CRUSHED
                        lastpos = sector.ceilingheight;
                        sector.ceilingheight -= speed;
                        flag = sector.changeSector(crush);

                        if (flag == true)
                        {
                            if (crush == true)
                                return MoveResult::Crushed;
                            sector.ceilingheight = lastpos;
                            sector.changeSector(crush);
                            return MoveResult::Crushed;
                        }
                    }
                    break;

                case 1:
                    // UP
                    if (sector.ceilingheight + speed > dest)
                    {
                        lastpos = sector.ceilingheight;
                        sector.ceilingheight = dest;
                        flag = sector.changeSector(crush);
                        if (flag == true)
                        {
                            sector.ceilingheight = lastpos;
                            sector.changeSector(crush);
                            //return MoveResult::Crushed;
                        }
                        return MoveResult::PastDest;
                    }
                    else
                    {
                        lastpos = sector.ceilingheight;
                        sector.ceilingheight += speed;
                        flag = sector.changeSector(crush);
                        // UNUSED
#if 0
                        if (flag == true)
                        {
                            sector.ceilingheight = lastpos;
                            (&sector).changeSector(crush);
                            return MoveResult::Crushed;
                        }
#endif
                    }
                    break;
            }
            break;
    }

    return MoveResult::Ok;
}

//
// HANDLE FLOOR TYPES
//
int doFloor(Line& line, FloorType floortype)
{
    int secnum = -1;
    int rtn = 0;
    while ((secnum = findSectorFromLineTag(line, secnum)) >= 0)
    {
        Sector* sec = &level().sectors[secnum];

        // ALREADY MOVING?  IF SO, KEEP GOING...
        if (sec->specialdata)
            continue;

        // new floor thinker
        rtn = 1;
        FloorMove* floor = new (levelAlloc(sizeof(*floor))) FloorMove {};
        addThinker(*floor);
        sec->specialdata = floor;
        floor->type = floortype;
        floor->crush = false;

        switch (floortype)
        {
            case FloorType::LowerFloor:
                floor->direction = -1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                floor->floordestheight = findHighestFloorSurrounding(*sec);
                break;

            case FloorType::LowerFloorToLowest:
                floor->direction = -1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                floor->floordestheight = findLowestFloorSurrounding(*sec);
                break;

            case FloorType::TurboLower:
                floor->direction = -1;
                floor->sector = sec;
                floor->speed = FLOORSPEED * 4;
                floor->floordestheight = findHighestFloorSurrounding(*sec);
                if (floor->floordestheight != sec->floorheight)
                    floor->floordestheight += 8 * FRACUNIT;
                break;

            case FloorType::RaiseFloorCrush:
                floor->crush = true;
                [[fallthrough]];
            case FloorType::RaiseFloor:
                floor->direction = 1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                floor->floordestheight = findLowestCeilingSurrounding(*sec);
                if (floor->floordestheight > sec->ceilingheight)
                    floor->floordestheight = sec->ceilingheight;
                floor->floordestheight -=
                    (8 * FRACUNIT) * (floortype == FloorType::RaiseFloorCrush);
                break;

            case FloorType::RaiseFloorTurbo:
                floor->direction = 1;
                floor->sector = sec;
                floor->speed = FLOORSPEED * 4;
                floor->floordestheight =
                    findNextHighestFloor(*sec, sec->floorheight);
                break;

            case FloorType::RaiseFloorToNearest:
                floor->direction = 1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                floor->floordestheight =
                    findNextHighestFloor(*sec, sec->floorheight);
                break;

            case FloorType::RaiseFloor24:
                floor->direction = 1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                floor->floordestheight = floor->sector->floorheight + 24 * FRACUNIT;
                break;
            case FloorType::RaiseFloor512:
                floor->direction = 1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                floor->floordestheight = floor->sector->floorheight + 512 * FRACUNIT;
                break;

            case FloorType::RaiseFloor24AndChange:
                floor->direction = 1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                floor->floordestheight = floor->sector->floorheight + 24 * FRACUNIT;
                sec->floorpic = line.frontsector->floorpic;
                sec->special = line.frontsector->special;
                break;

            case FloorType::RaiseToTexture:
            {
                Fixed minsize {DOOM_MAXINT};

                floor->direction = 1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                for (int i = 0; i < sec->linecount; i++)
                {
                    if (twoSided(secnum, i))
                    {
                        Side* side = getSide(secnum, i, 0);
                        if (side->bottomtexture >= 0)
                            if (graphicsData().textureheight[side->bottomtexture]
                                < minsize)
                                minsize = graphicsData()
                                              .textureheight[side->bottomtexture];
                        side = getSide(secnum, i, 1);
                        if (side->bottomtexture >= 0)
                            if (graphicsData().textureheight[side->bottomtexture]
                                < minsize)
                                minsize = graphicsData()
                                              .textureheight[side->bottomtexture];
                    }
                }
                floor->floordestheight = floor->sector->floorheight + minsize;
            }
            break;

            case FloorType::LowerAndChange:
                floor->direction = -1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                floor->floordestheight = findLowestFloorSurrounding(*sec);
                floor->texture = sec->floorpic;

                for (int i = 0; i < sec->linecount; i++)
                {
                    if (twoSided(secnum, i))
                    {
                        if (getSide(secnum, i, 0)->sector - level().sectors.data()
                            == secnum)
                        {
                            sec = getSector(secnum, i, 1);

                            if (sec->floorheight == floor->floordestheight)
                            {
                                floor->texture = sec->floorpic;
                                floor->newspecial = sec->special;
                                break;
                            }
                        }
                        else
                        {
                            sec = getSector(secnum, i, 0);

                            if (sec->floorheight == floor->floordestheight)
                            {
                                floor->texture = sec->floorpic;
                                floor->newspecial = sec->special;
                                break;
                            }
                        }
                    }
                }
                break;

            case FloorType::DonutRaise:
                break;
        }
    }

    return rtn;
}

//
// BUILD A STAIRCASE!
//
int buildStairs(Line& line, StairType type)
{
    int ok;

    Fixed stairsize;
    Fixed speed;

    int secnum = -1;
    int rtn = 0;
    while ((secnum = findSectorFromLineTag(line, secnum)) >= 0)
    {
        Sector* sec = &level().sectors[secnum];

        // ALREADY MOVING?  IF SO, KEEP GOING...
        if (sec->specialdata)
            continue;

        // new floor thinker
        rtn = 1;
        FloorMove* floor = new (levelAlloc(sizeof(*floor))) FloorMove {};
        addThinker(*floor);
        sec->specialdata = floor;
        floor->direction = 1;
        floor->sector = sec;
        switch (type)
        {
            case StairType::Build8:
                speed = FLOORSPEED / 4;
                stairsize = 8 * FRACUNIT;
                break;
            case StairType::Turbo16:
                speed = FLOORSPEED * 4;
                stairsize = 16 * FRACUNIT;
                break;
        }
        floor->speed = speed;
        Fixed height = sec->floorheight + stairsize;
        floor->floordestheight = height;

        int texture = sec->floorpic;

        // Find next sector to raise
        // 1.        Find 2-sided line with same sector side[0]
        // 2.        Other side is the next sector to raise
        do
        {
            ok = 0;
            for (int i = 0; i < sec->linecount; i++)
            {
                if (!((sec->lines[i])->flags & ML_TWOSIDED))
                    continue;

                Sector* tsec = (sec->lines[i])->frontsector;
                int newsecnum = static_cast<int>(tsec - level().sectors.data());

                if (secnum != newsecnum)
                    continue;

                tsec = (sec->lines[i])->backsector;
                newsecnum = static_cast<int>(tsec - level().sectors.data());

                if (tsec->floorpic != texture)
                    continue;

                height += stairsize;

                if (tsec->specialdata)
                    continue;

                sec = tsec;
                secnum = newsecnum;
                floor = new (levelAlloc(sizeof(*floor))) FloorMove {};

                addThinker(*floor);

                sec->specialdata = floor;
                floor->direction = 1;
                floor->sector = sec;
                floor->speed = speed;
                floor->floordestheight = height;
                ok = 1;
                break;
            }
        } while (ok);
    }

    return rtn;
}
} // namespace Doom
