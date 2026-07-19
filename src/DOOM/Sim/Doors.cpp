// Rewritten out of vanilla p_doors into namespace Doom.
//
// Vertical doors: the verticalDoor thinker and the EV_/spawn handlers that raise,
// lower and time them. p_doors.cpp shims every name. Golden-neutral - the demos open
// doors.

#include "../Host/Platform.h"

#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h"
#include "../Game/Strings.h"
#include "SimDefs.h"
#include "../Game/SoundData.h"

#include "Doors.h"
#include "Tick.h" // levelAlloc / levelFree / freeLevelAllocations
#include "Specials.h"

#include <new>

#include "../Game/Sound.h"

#include "Floors.h"
#include "../Sim/Level.h"
namespace Doom
{
// Forward declarations so the file's own call order needs no rearranging.
void verticalDoor(Door& door);
int doLockedDoor(Line* line, DoorType type, Mobj* thing);
int doDoor(Line* line, DoorType type);
void verticalDoor(Line* line, Mobj* thing);
void spawnDoorCloseIn30(Sector* sec);
void spawnDoorRaiseIn5Mins(Sector* sec, int secnum);

void verticalDoor(Door& door)
{
    MoveResult res;

    switch (door.direction)
    {
        case 0:
            // WAITING
            if (!--door.topcountdown)
            {
                switch (door.type)
                {
                    case blazeRaise:
                        door.direction = -1; // time to go back down
                        startSound(
                            reinterpret_cast<Mobj*>(&door.sector->soundorg),
                            sfx_bdcls);
                        break;

                    case door_normal:
                        door.direction = -1; // time to go back down
                        startSound(
                            reinterpret_cast<Mobj*>(&door.sector->soundorg),
                            sfx_dorcls);
                        break;

                    case close30ThenOpen:
                        door.direction = 1;
                        startSound(
                            reinterpret_cast<Mobj*>(&door.sector->soundorg),
                            sfx_doropn);
                        break;

                    default:
                        break;
                }
            }
            break;

        case 2:
            //  INITIAL WAIT
            if (!--door.topcountdown)
            {
                switch (door.type)
                {
                    case raiseIn5Mins:
                        door.direction = 1;
                        door.type = door_normal;
                        startSound(
                            reinterpret_cast<Mobj*>(&door.sector->soundorg),
                            sfx_doropn);
                        break;

                    default:
                        break;
                }
            }
            break;

        case -1:
            // DOWN
            res = movePlane(*door.sector,
                              door.speed,
                              door.sector->floorheight,
                              false,
                              1,
                              door.direction);
            if (res == pastdest)
            {
                switch (door.type)
                {
                    case blazeRaise:
                    case blazeClose:
                        door.sector->specialdata = nullptr;
                        removeThinker(&door); // unlink and free
                        startSound(
                            reinterpret_cast<Mobj*>(&door.sector->soundorg),
                            sfx_bdcls);
                        break;

                    case door_normal:
                    case door_close:
                        door.sector->specialdata = nullptr;
                        removeThinker(&door); // unlink and free
                        break;

                    case close30ThenOpen:
                        door.direction = 0;
                        door.topcountdown = 35 * 30;
                        break;

                    default:
                        break;
                }
            }
            else if (res == crushed)
            {
                switch (door.type)
                {
                    case blazeClose:
                    case door_close: // DO NOT GO BACK UP!
                        break;

                    default:
                        door.direction = 1;
                        startSound(
                            reinterpret_cast<Mobj*>(&door.sector->soundorg),
                            sfx_doropn);
                        break;
                }
            }
            break;

        case 1:
            // UP
            res = movePlane(*door.sector,
                              door.speed,
                              door.topheight,
                              false,
                              1,
                              door.direction);

            if (res == pastdest)
            {
                switch (door.type)
                {
                    case blazeRaise:
                    case door_normal:
                        door.direction = 0; // wait at top
                        door.topcountdown = door.topwait;
                        break;

                    case close30ThenOpen:
                    case blazeOpen:
                    case door_open:
                        door.sector->specialdata = nullptr;
                        removeThinker(&door); // unlink and free
                        break;

                    default:
                        break;
                }
            }
            break;
    }
}

//
// doLockedDoor
// Move a locked door up/down
//
int doLockedDoor(Line* line, DoorType type, Mobj* thing)
{
    Player* p = thing->player;

    if (!p)
        return 0;

    switch (line->special)
    {
        case 99: // Blue Lock
        case 133:
            if (!p)
                return 0;
            if (!p->cards[it_bluecard] && !p->cards[it_blueskull])
            {
                p->message = PD_BLUEO;
                startSound(nullptr, sfx_oof);
                return 0;
            }
            break;

        case 134: // Red Lock
        case 135:
            if (!p)
                return 0;
            if (!p->cards[it_redcard] && !p->cards[it_redskull])
            {
                p->message = PD_REDO;
                startSound(nullptr, sfx_oof);
                return 0;
            }
            break;

        case 136: // Yellow Lock
        case 137:
            if (!p)
                return 0;
            if (!p->cards[it_yellowcard] && !p->cards[it_yellowskull])
            {
                p->message = PD_YELLOWO;
                startSound(nullptr, sfx_oof);
                return 0;
            }
            break;
    }

    return doDoor(line, type);
}

int doDoor(Line* line, DoorType type)
{
    int secnum = -1;
    int rtn = 0;

    while ((secnum = findSectorFromLineTag(line, secnum)) >= 0)
    {
        Sector* sec = &sectors[secnum];
        if (sec->specialdata)
            continue;

        // new door thinker
        rtn = 1;
        Door* door = new (levelAlloc(sizeof(*door))) Door {};
        addThinker(door);
        sec->specialdata = door;

        door->sector = sec;
        door->type = type;
        door->topwait = VDOORWAIT;
        door->speed = VDOORSPEED;

        switch (type)
        {
            case blazeClose:
                door->topheight = findLowestCeilingSurrounding(sec);
                door->topheight -= 4 * FRACUNIT;
                door->direction = -1;
                door->speed = VDOORSPEED * 4;
                startSound(reinterpret_cast<Mobj*>(&door->sector->soundorg),
                             sfx_bdcls);
                break;

            case door_close:
                door->topheight = findLowestCeilingSurrounding(sec);
                door->topheight -= 4 * FRACUNIT;
                door->direction = -1;
                startSound(reinterpret_cast<Mobj*>(&door->sector->soundorg),
                             sfx_dorcls);
                break;

            case close30ThenOpen:
                door->topheight = sec->ceilingheight;
                door->direction = -1;
                startSound(reinterpret_cast<Mobj*>(&door->sector->soundorg),
                             sfx_dorcls);
                break;

            case blazeRaise:
            case blazeOpen:
                door->direction = 1;
                door->topheight = findLowestCeilingSurrounding(sec);
                door->topheight -= 4 * FRACUNIT;
                door->speed = VDOORSPEED * 4;
                if (door->topheight != sec->ceilingheight)
                    startSound(reinterpret_cast<Mobj*>(&door->sector->soundorg),
                                 sfx_bdopn);
                break;

            case door_normal:
            case door_open:
                door->direction = 1;
                door->topheight = findLowestCeilingSurrounding(sec);
                door->topheight -= 4 * FRACUNIT;
                if (door->topheight != sec->ceilingheight)
                    startSound(reinterpret_cast<Mobj*>(&door->sector->soundorg),
                                 sfx_doropn);
                break;

            default:
                break;
        }
    }

    return rtn;
}

//
// verticalDoor : open a door manually, no tag value
//
void verticalDoor(Line* line, Mobj* thing)
{
    Door* door;

    int side = 0; // only front sides can be used

    // Check for locks
    Player* player = thing->player;

    switch (line->special)
    {
        case 26: // Blue Lock
        case 32:
            if (!player)
                return;

            if (!player->cards[it_bluecard] && !player->cards[it_blueskull])
            {
                player->message = PD_BLUEK;
                startSound(nullptr, sfx_oof);
                return;
            }
            break;

        case 27: // Yellow Lock
        case 34:
            if (!player)
                return;

            if (!player->cards[it_yellowcard] && !player->cards[it_yellowskull])
            {
                player->message = PD_YELLOWK;
                startSound(nullptr, sfx_oof);
                return;
            }
            break;

        case 28: // Red Lock
        case 33:
            if (!player)
                return;

            if (!player->cards[it_redcard] && !player->cards[it_redskull])
            {
                player->message = PD_REDK;
                startSound(nullptr, sfx_oof);
                return;
            }
            break;
    }

    // if the sector has an active thinker, use it
    Sector* sec = sides[line->sidenum[side ^ 1]].sector;

    if (sec->specialdata)
    {
        door = static_cast<Door*>(sec->specialdata);
        switch (line->special)
        {
            case 1: // ONLY FOR "RAISE" DOORS, NOT "OPEN"s
            case 26:
            case 27:
            case 28:
            case 117:
                if (door->direction == -1)
                    door->direction = 1; // go back up
                else
                {
                    if (!thing->player)
                        return; // JDC: bad guys never close doors

                    door->direction = -1; // start going down immediately
                }
                return;
        }
    }

    // for proper sound
    switch (line->special)
    {
        case 117: // BLAZING DOOR RAISE
        case 118: // BLAZING DOOR OPEN
            startSound(reinterpret_cast<Mobj*>(&sec->soundorg), sfx_bdopn);
            break;

        case 1: // NORMAL DOOR SOUND
        case 31:
            startSound(reinterpret_cast<Mobj*>(&sec->soundorg), sfx_doropn);
            break;

        default: // LOCKED DOOR SOUND
            startSound(reinterpret_cast<Mobj*>(&sec->soundorg), sfx_doropn);
            break;
    }

    // new door thinker
    door = new (levelAlloc(sizeof(*door))) Door {};
    addThinker(door);
    sec->specialdata = door;
    door->sector = sec;
    door->direction = 1;
    door->speed = VDOORSPEED;
    door->topwait = VDOORWAIT;

    switch (line->special)
    {
        case 1:
        case 26:
        case 27:
        case 28:
            door->type = door_normal;
            break;

        case 31:
        case 32:
        case 33:
        case 34:
            door->type = door_open;
            line->special = 0;
            break;

        case 117: // blazing door raise
            door->type = blazeRaise;
            door->speed = VDOORSPEED * 4;
            break;
        case 118: // blazing door open
            door->type = blazeOpen;
            line->special = 0;
            door->speed = VDOORSPEED * 4;
            break;
    }

    // find the top and bottom of the movement range
    door->topheight = findLowestCeilingSurrounding(sec);
    door->topheight -= 4 * FRACUNIT;
}

//
// Spawn a door that closes after 30 seconds
//
void spawnDoorCloseIn30(Sector* sec)
{
    Door* door = new (levelAlloc(sizeof(*door))) Door {};

    addThinker(door);

    sec->specialdata = door;
    sec->special = 0;

    door->sector = sec;
    door->direction = 0;
    door->type = door_normal;
    door->speed = VDOORSPEED;
    door->topcountdown = 30 * 35;
}

//
// Spawn a door that opens after 5 minutes
//
void spawnDoorRaiseIn5Mins(Sector* sec, int)
{
    Door* door = new (levelAlloc(sizeof(*door))) Door {};

    addThinker(door);

    sec->specialdata = door;
    sec->special = 0;

    door->sector = sec;
    door->direction = 2;
    door->type = raiseIn5Mins;
    door->speed = VDOORSPEED;
    door->topheight = findLowestCeilingSurrounding(sec);
    door->topheight -= 4 * FRACUNIT;
    door->topwait = VDOORWAIT;
    door->topcountdown = 5 * 60 * 35;
}
} // namespace Doom
