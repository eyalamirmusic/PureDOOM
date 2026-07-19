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
                   fixed_t speed,
                   fixed_t dest,
                   doom_boolean crush,
                   int floorOrCeiling,
                   int direction);
void moveFloor(FloorMove& floor);
int doFloor(Line* line, FloorType floortype);
int buildStairs(Line* line, StairType type);

MoveResult movePlane(Sector& sector,
                   fixed_t speed,
                   fixed_t dest,
                   doom_boolean crush,
                   int floorOrCeiling,
                   int direction)
{
    doom_boolean flag;
    fixed_t lastpos;

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
                        flag = Doom::changeSector(&sector, crush);
                        if (flag == true)
                        {
                            sector.floorheight = lastpos;
                            Doom::changeSector(&sector, crush);
                            //return crushed;
                        }
                        return pastdest;
                    }
                    else
                    {
                        lastpos = sector.floorheight;
                        sector.floorheight -= speed;
                        flag = Doom::changeSector(&sector, crush);
                        if (flag == true)
                        {
                            sector.floorheight = lastpos;
                            Doom::changeSector(&sector, crush);
                            return crushed;
                        }
                    }
                    break;

                case 1:
                    // UP
                    if (sector.floorheight + speed > dest)
                    {
                        lastpos = sector.floorheight;
                        sector.floorheight = dest;
                        flag = Doom::changeSector(&sector, crush);
                        if (flag == true)
                        {
                            sector.floorheight = lastpos;
                            Doom::changeSector(&sector, crush);
                            //return crushed;
                        }
                        return pastdest;
                    }
                    else
                    {
                        // COULD GET CRUSHED
                        lastpos = sector.floorheight;
                        sector.floorheight += speed;
                        flag = Doom::changeSector(&sector, crush);
                        if (flag == true)
                        {
                            if (crush == true)
                                return crushed;
                            sector.floorheight = lastpos;
                            Doom::changeSector(&sector, crush);
                            return crushed;
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
                        flag = Doom::changeSector(&sector, crush);

                        if (flag == true)
                        {
                            sector.ceilingheight = lastpos;
                            Doom::changeSector(&sector, crush);
                            //return crushed;
                        }
                        return pastdest;
                    }
                    else
                    {
                        // COULD GET CRUSHED
                        lastpos = sector.ceilingheight;
                        sector.ceilingheight -= speed;
                        flag = Doom::changeSector(&sector, crush);

                        if (flag == true)
                        {
                            if (crush == true)
                                return crushed;
                            sector.ceilingheight = lastpos;
                            Doom::changeSector(&sector, crush);
                            return crushed;
                        }
                    }
                    break;

                case 1:
                    // UP
                    if (sector.ceilingheight + speed > dest)
                    {
                        lastpos = sector.ceilingheight;
                        sector.ceilingheight = dest;
                        flag = Doom::changeSector(&sector, crush);
                        if (flag == true)
                        {
                            sector.ceilingheight = lastpos;
                            Doom::changeSector(&sector, crush);
                            //return crushed;
                        }
                        return pastdest;
                    }
                    else
                    {
                        lastpos = sector.ceilingheight;
                        sector.ceilingheight += speed;
                        flag = Doom::changeSector(&sector, crush);
                        // UNUSED
#if 0
                        if (flag == true)
                        {
                            sector.ceilingheight = lastpos;
                            Doom::changeSector(&sector, crush);
                            return crushed;
                        }
#endif
                    }
                    break;
            }
            break;
    }

    return ok;
}

//
// MOVE A FLOOR TO IT'S DESTINATION (UP OR DOWN)
//
void moveFloor(FloorMove& floor)
{
    MoveResult res;

    res = movePlane(*floor.sector,
                    floor.speed,
                    floor.floordestheight,
                    floor.crush,
                    0,
                    floor.direction);

    if (!(levelStats().leveltime & 7))
        Doom::startSound(reinterpret_cast<Mobj*>(&floor.sector->soundorg),
                     sfx_stnmov);

    if (res == pastdest)
    {
        floor.sector->specialdata = nullptr;

        if (floor.direction == 1)
        {
            switch (floor.type)
            {
                case donutRaise:
                    floor.sector->special = floor.newspecial;
                    floor.sector->floorpic = floor.texture;
                default:
                    break;
            }
        }
        else if (floor.direction == -1)
        {
            switch (floor.type)
            {
                case lowerAndChange:
                    floor.sector->special = floor.newspecial;
                    floor.sector->floorpic = floor.texture;
                default:
                    break;
            }
        }
        Doom::removeThinker(&floor);

        Doom::startSound(reinterpret_cast<Mobj*>(&floor.sector->soundorg), sfx_pstop);
    }
}

//
// HANDLE FLOOR TYPES
//
int doFloor(Line* line, FloorType floortype)
{
    int secnum;
    int rtn;
    Sector* sec;
    FloorMove* floor;

    secnum = -1;
    rtn = 0;
    while ((secnum = Doom::findSectorFromLineTag(line, secnum)) >= 0)
    {
        sec = &sectors[secnum];

        // ALREADY MOVING?  IF SO, KEEP GOING...
        if (sec->specialdata)
            continue;

        // new floor thinker
        rtn = 1;
        floor = new (levelAlloc(sizeof(*floor))) FloorMove {};
        Doom::addThinker(floor);
        sec->specialdata = floor;
        floor->type = floortype;
        floor->crush = false;

        switch (floortype)
        {
            case lowerFloor:
                floor->direction = -1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                floor->floordestheight = Doom::findHighestFloorSurrounding(sec);
                break;

            case lowerFloorToLowest:
                floor->direction = -1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                floor->floordestheight = Doom::findLowestFloorSurrounding(sec);
                break;

            case turboLower:
                floor->direction = -1;
                floor->sector = sec;
                floor->speed = FLOORSPEED * 4;
                floor->floordestheight = Doom::findHighestFloorSurrounding(sec);
                if (floor->floordestheight != sec->floorheight)
                    floor->floordestheight += 8 * FRACUNIT;
                break;

            case raiseFloorCrush:
                floor->crush = true;
            case raiseFloor:
                floor->direction = 1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                floor->floordestheight = Doom::findLowestCeilingSurrounding(sec);
                if (floor->floordestheight > sec->ceilingheight)
                    floor->floordestheight = sec->ceilingheight;
                floor->floordestheight -=
                    (8 * FRACUNIT) * (floortype == raiseFloorCrush);
                break;

            case raiseFloorTurbo:
                floor->direction = 1;
                floor->sector = sec;
                floor->speed = FLOORSPEED * 4;
                floor->floordestheight =
                    Doom::findNextHighestFloor(sec, sec->floorheight);
                break;

            case raiseFloorToNearest:
                floor->direction = 1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                floor->floordestheight =
                    Doom::findNextHighestFloor(sec, sec->floorheight);
                break;

            case raiseFloor24:
                floor->direction = 1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                floor->floordestheight = floor->sector->floorheight + 24 * FRACUNIT;
                break;
            case raiseFloor512:
                floor->direction = 1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                floor->floordestheight = floor->sector->floorheight + 512 * FRACUNIT;
                break;

            case raiseFloor24AndChange:
                floor->direction = 1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                floor->floordestheight = floor->sector->floorheight + 24 * FRACUNIT;
                sec->floorpic = line->frontsector->floorpic;
                sec->special = line->frontsector->special;
                break;

            case raiseToTexture:
            {
                fixed_t minsize {DOOM_MAXINT};
                Side* side;

                floor->direction = 1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                for (int i = 0; i < sec->linecount; i++)
                {
                    if (twoSided(secnum, i))
                    {
                        side = getSide(secnum, i, 0);
                        if (side->bottomtexture >= 0)
                            if (textureheight[side->bottomtexture] < minsize)
                                minsize = textureheight[side->bottomtexture];
                        side = getSide(secnum, i, 1);
                        if (side->bottomtexture >= 0)
                            if (textureheight[side->bottomtexture] < minsize)
                                minsize = textureheight[side->bottomtexture];
                    }
                }
                floor->floordestheight = floor->sector->floorheight + minsize;
            }
            break;

            case lowerAndChange:
                floor->direction = -1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                floor->floordestheight = Doom::findLowestFloorSurrounding(sec);
                floor->texture = sec->floorpic;

                for (int i = 0; i < sec->linecount; i++)
                {
                    if (twoSided(secnum, i))
                    {
                        if (getSide(secnum, i, 0)->sector - sectors == secnum)
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
            default:
                break;
        }
    }

    return rtn;
}

//
// BUILD A STAIRCASE!
//
int buildStairs(Line* line, StairType type)
{
    int secnum;
    fixed_t height;
    int newsecnum;
    int texture;
    int ok;
    int rtn;

    Sector* sec;
    Sector* tsec;

    FloorMove* floor;

    fixed_t stairsize;
    fixed_t speed;

    secnum = -1;
    rtn = 0;
    while ((secnum = Doom::findSectorFromLineTag(line, secnum)) >= 0)
    {
        sec = &sectors[secnum];

        // ALREADY MOVING?  IF SO, KEEP GOING...
        if (sec->specialdata)
            continue;

        // new floor thinker
        rtn = 1;
        floor = new (levelAlloc(sizeof(*floor))) FloorMove {};
        Doom::addThinker(floor);
        sec->specialdata = floor;
        floor->direction = 1;
        floor->sector = sec;
        switch (type)
        {
            case build8:
                speed = FLOORSPEED / 4;
                stairsize = 8 * FRACUNIT;
                break;
            case turbo16:
                speed = FLOORSPEED * 4;
                stairsize = 16 * FRACUNIT;
                break;
        }
        floor->speed = speed;
        height = sec->floorheight + stairsize;
        floor->floordestheight = height;

        texture = sec->floorpic;

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

                tsec = (sec->lines[i])->frontsector;
                newsecnum = static_cast<int>(tsec - sectors);

                if (secnum != newsecnum)
                    continue;

                tsec = (sec->lines[i])->backsector;
                newsecnum = static_cast<int>(tsec - sectors);

                if (tsec->floorpic != texture)
                    continue;

                height += stairsize;

                if (tsec->specialdata)
                    continue;

                sec = tsec;
                secnum = newsecnum;
                floor = new (levelAlloc(sizeof(*floor))) FloorMove {};

                Doom::addThinker(floor);

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
