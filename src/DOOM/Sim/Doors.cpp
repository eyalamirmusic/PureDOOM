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
int doLockedDoor(Line& line, DoorType type, Mobj& thing);
int doDoor(Line& line, DoorType type);
void verticalDoor(Line& line, Mobj& thing);
void spawnDoorCloseIn30(Sector& sec);
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
                    case DoorType::BlazeRaise:
                        door.direction = -1; // time to go back down
                        startSound(reinterpret_cast<Mobj*>(&door.sector->soundorg),
                                   SfxEnum::Bdcls);
                        break;

                    case DoorType::Normal:
                        door.direction = -1; // time to go back down
                        startSound(reinterpret_cast<Mobj*>(&door.sector->soundorg),
                                   SfxEnum::Dorcls);
                        break;

                    case DoorType::Close30ThenOpen:
                        door.direction = 1;
                        startSound(reinterpret_cast<Mobj*>(&door.sector->soundorg),
                                   SfxEnum::Doropn);
                        break;

                    case DoorType::Close:
                    case DoorType::Open:
                    case DoorType::RaiseIn5Mins:
                    case DoorType::BlazeOpen:
                    case DoorType::BlazeClose:
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
                    case DoorType::RaiseIn5Mins:
                        door.direction = 1;
                        door.type = DoorType::Normal;
                        startSound(reinterpret_cast<Mobj*>(&door.sector->soundorg),
                                   SfxEnum::Doropn);
                        break;

                    case DoorType::Normal:
                    case DoorType::Close30ThenOpen:
                    case DoorType::Close:
                    case DoorType::Open:
                    case DoorType::BlazeRaise:
                    case DoorType::BlazeOpen:
                    case DoorType::BlazeClose:
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
            if (res == MoveResult::PastDest)
            {
                switch (door.type)
                {
                    case DoorType::BlazeRaise:
                    case DoorType::BlazeClose:
                        door.sector->specialdata = nullptr;
                        removeThinker(door); // unlink and free
                        startSound(reinterpret_cast<Mobj*>(&door.sector->soundorg),
                                   SfxEnum::Bdcls);
                        break;

                    case DoorType::Normal:
                    case DoorType::Close:
                        door.sector->specialdata = nullptr;
                        removeThinker(door); // unlink and free
                        break;

                    case DoorType::Close30ThenOpen:
                        door.direction = 0;
                        door.topcountdown = 35 * 30;
                        break;

                    case DoorType::Open:
                    case DoorType::RaiseIn5Mins:
                    case DoorType::BlazeOpen:
                        break;
                }
            }
            else if (res == MoveResult::Crushed)
            {
                switch (door.type)
                {
                    case DoorType::BlazeClose:
                    case DoorType::Close: // DO NOT GO BACK UP!
                        break;

                    case DoorType::Normal:
                    case DoorType::Close30ThenOpen:
                    case DoorType::Open:
                    case DoorType::RaiseIn5Mins:
                    case DoorType::BlazeRaise:
                    case DoorType::BlazeOpen:
                        door.direction = 1;
                        startSound(reinterpret_cast<Mobj*>(&door.sector->soundorg),
                                   SfxEnum::Doropn);
                        break;
                }
            }
            break;

        case 1:
            // UP
            res = movePlane(
                *door.sector, door.speed, door.topheight, false, 1, door.direction);

            if (res == MoveResult::PastDest)
            {
                switch (door.type)
                {
                    case DoorType::BlazeRaise:
                    case DoorType::Normal:
                        door.direction = 0; // wait at top
                        door.topcountdown = door.topwait;
                        break;

                    case DoorType::Close30ThenOpen:
                    case DoorType::BlazeOpen:
                    case DoorType::Open:
                        door.sector->specialdata = nullptr;
                        removeThinker(door); // unlink and free
                        break;

                    case DoorType::Close:
                    case DoorType::RaiseIn5Mins:
                    case DoorType::BlazeClose:
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
int doLockedDoor(Line& line, DoorType type, Mobj& thing)
{
    Player* p = thing.player;

    if (!p)
        return 0;

    switch (line.special)
    {
        case 99: // Blue Lock
        case 133:
            if (!p)
                return 0;
            if (!p->cards[toIndex(Card::BlueCard)]
                && !p->cards[toIndex(Card::BlueSkull)])
            {
                p->message = PD_BLUEO;
                startSound(nullptr, SfxEnum::Oof);
                return 0;
            }
            break;

        case 134: // Red Lock
        case 135:
            if (!p)
                return 0;
            if (!p->cards[toIndex(Card::RedCard)]
                && !p->cards[toIndex(Card::RedSkull)])
            {
                p->message = PD_REDO;
                startSound(nullptr, SfxEnum::Oof);
                return 0;
            }
            break;

        case 136: // Yellow Lock
        case 137:
            if (!p)
                return 0;
            if (!p->cards[toIndex(Card::YellowCard)]
                && !p->cards[toIndex(Card::YellowSkull)])
            {
                p->message = PD_YELLOWO;
                startSound(nullptr, SfxEnum::Oof);
                return 0;
            }
            break;
    }

    return doDoor(line, type);
}

int doDoor(Line& line, DoorType type)
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
        addThinker(*door);
        sec->specialdata = door;

        door->sector = sec;
        door->type = type;
        door->topwait = VDOORWAIT;
        door->speed = VDOORSPEED;

        switch (type)
        {
            case DoorType::BlazeClose:
                door->topheight = findLowestCeilingSurrounding(*sec);
                door->topheight -= 4 * FRACUNIT;
                door->direction = -1;
                door->speed = VDOORSPEED * 4;
                startSound(reinterpret_cast<Mobj*>(&door->sector->soundorg),
                           SfxEnum::Bdcls);
                break;

            case DoorType::Close:
                door->topheight = findLowestCeilingSurrounding(*sec);
                door->topheight -= 4 * FRACUNIT;
                door->direction = -1;
                startSound(reinterpret_cast<Mobj*>(&door->sector->soundorg),
                           SfxEnum::Dorcls);
                break;

            case DoorType::Close30ThenOpen:
                door->topheight = sec->ceilingheight;
                door->direction = -1;
                startSound(reinterpret_cast<Mobj*>(&door->sector->soundorg),
                           SfxEnum::Dorcls);
                break;

            case DoorType::BlazeRaise:
            case DoorType::BlazeOpen:
                door->direction = 1;
                door->topheight = findLowestCeilingSurrounding(*sec);
                door->topheight -= 4 * FRACUNIT;
                door->speed = VDOORSPEED * 4;
                if (door->topheight != sec->ceilingheight)
                    startSound(reinterpret_cast<Mobj*>(&door->sector->soundorg),
                               SfxEnum::Bdopn);
                break;

            case DoorType::Normal:
            case DoorType::Open:
                door->direction = 1;
                door->topheight = findLowestCeilingSurrounding(*sec);
                door->topheight -= 4 * FRACUNIT;
                if (door->topheight != sec->ceilingheight)
                    startSound(reinterpret_cast<Mobj*>(&door->sector->soundorg),
                               SfxEnum::Doropn);
                break;

            case DoorType::RaiseIn5Mins:
                break;
        }
    }

    return rtn;
}

//
// verticalDoor : open a door manually, no tag value
//
void verticalDoor(Line& line, Mobj& thing)
{
    Door* door;

    int side = 0; // only front sides can be used

    // Check for locks
    Player* player = thing.player;

    switch (line.special)
    {
        case 26: // Blue Lock
        case 32:
            if (!player)
                return;

            if (!player->cards[toIndex(Card::BlueCard)]
                && !player->cards[toIndex(Card::BlueSkull)])
            {
                player->message = PD_BLUEK;
                startSound(nullptr, SfxEnum::Oof);
                return;
            }
            break;

        case 27: // Yellow Lock
        case 34:
            if (!player)
                return;

            if (!player->cards[toIndex(Card::YellowCard)]
                && !player->cards[toIndex(Card::YellowSkull)])
            {
                player->message = PD_YELLOWK;
                startSound(nullptr, SfxEnum::Oof);
                return;
            }
            break;

        case 28: // Red Lock
        case 33:
            if (!player)
                return;

            if (!player->cards[toIndex(Card::RedCard)]
                && !player->cards[toIndex(Card::RedSkull)])
            {
                player->message = PD_REDK;
                startSound(nullptr, SfxEnum::Oof);
                return;
            }
            break;
    }

    // if the sector has an active thinker, use it
    Sector* sec = sides[line.sidenum[side ^ 1]].sector;

    if (sec->specialdata)
    {
        door = static_cast<Door*>(sec->specialdata);
        switch (line.special)
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
                    if (!thing.player)
                        return; // JDC: bad guys never close doors

                    door->direction = -1; // start going down immediately
                }
                return;
        }
    }

    // for proper sound
    switch (line.special)
    {
        case 117: // BLAZING DOOR RAISE
        case 118: // BLAZING DOOR OPEN
            startSound(reinterpret_cast<Mobj*>(&sec->soundorg), SfxEnum::Bdopn);
            break;

        case 1: // NORMAL DOOR SOUND
        case 31:
            startSound(reinterpret_cast<Mobj*>(&sec->soundorg), SfxEnum::Doropn);
            break;

        default: // LOCKED DOOR SOUND
            startSound(reinterpret_cast<Mobj*>(&sec->soundorg), SfxEnum::Doropn);
            break;
    }

    // new door thinker
    door = new (levelAlloc(sizeof(*door))) Door {};
    addThinker(*door);
    sec->specialdata = door;
    door->sector = sec;
    door->direction = 1;
    door->speed = VDOORSPEED;
    door->topwait = VDOORWAIT;

    switch (line.special)
    {
        case 1:
        case 26:
        case 27:
        case 28:
            door->type = DoorType::Normal;
            break;

        case 31:
        case 32:
        case 33:
        case 34:
            door->type = DoorType::Open;
            line.special = 0;
            break;

        case 117: // blazing door raise
            door->type = DoorType::BlazeRaise;
            door->speed = VDOORSPEED * 4;
            break;
        case 118: // blazing door open
            door->type = DoorType::BlazeOpen;
            line.special = 0;
            door->speed = VDOORSPEED * 4;
            break;
    }

    // find the top and bottom of the movement range
    door->topheight = findLowestCeilingSurrounding(*sec);
    door->topheight -= 4 * FRACUNIT;
}

//
// Spawn a door that closes after 30 seconds
//
void spawnDoorCloseIn30(Sector& sec)
{
    Door* door = new (levelAlloc(sizeof(*door))) Door {};

    addThinker(*door);

    sec.specialdata = door;
    sec.special = 0;

    door->sector = &sec;
    door->direction = 0;
    door->type = DoorType::Normal;
    door->speed = VDOORSPEED;
    door->topcountdown = 30 * 35;
}

//
// Spawn a door that opens after 5 minutes
//
void spawnDoorRaiseIn5Mins(Sector& sec, int)
{
    Door* door = new (levelAlloc(sizeof(*door))) Door {};

    addThinker(*door);

    sec.specialdata = door;
    sec.special = 0;

    door->sector = &sec;
    door->direction = 2;
    door->type = DoorType::RaiseIn5Mins;
    door->speed = VDOORSPEED;
    door->topheight = findLowestCeilingSurrounding(sec);
    door->topheight -= 4 * FRACUNIT;
    door->topwait = VDOORWAIT;
    door->topcountdown = 5 * 60 * 35;
}
} // namespace Doom
