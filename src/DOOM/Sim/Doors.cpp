// Rewritten out of vanilla p_doors into namespace Doom.
//
// Vertical doors: the line handlers that raise, lower and time them, now
// Line::doDoor / Line::doLockedDoor / Line::verticalDoor and the timed Sector
// spawners. Golden-neutral - the demos open doors.

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
// The door handlers are Line methods (doLockedDoor/doDoor/verticalDoor) and the
// timed spawners are Sector methods (spawnDoorCloseIn30/spawnDoorRaiseIn5Mins), all
// declared on the types in MapTypes.h; no file-scope forward declarations needed.

//
// doLockedDoor
// Move a locked door up/down
//
int Line::doLockedDoor(DoorType type, Mobj& thing)
{
    Player* p = thing.player;

    if (!p)
        return 0;

    switch (special)
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

    return doDoor(type);
}

int Line::doDoor(DoorType type)
{
    int secnum = -1;
    int rtn = 0;

    while ((secnum = findSectorFromLineTag(secnum)) >= 0)
    {
        Sector* sec = &level().sectors[secnum];
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
                door->topheight = sec->findLowestCeilingSurrounding();
                door->topheight -= 4 * FRACUNIT;
                door->direction = -1;
                door->speed = VDOORSPEED * 4;
                startSound(reinterpret_cast<Mobj*>(&door->sector->soundorg),
                           SfxEnum::Bdcls);
                break;

            case DoorType::Close:
                door->topheight = sec->findLowestCeilingSurrounding();
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
                door->topheight = sec->findLowestCeilingSurrounding();
                door->topheight -= 4 * FRACUNIT;
                door->speed = VDOORSPEED * 4;
                if (door->topheight != sec->ceilingheight)
                    startSound(reinterpret_cast<Mobj*>(&door->sector->soundorg),
                               SfxEnum::Bdopn);
                break;

            case DoorType::Normal:
            case DoorType::Open:
                door->direction = 1;
                door->topheight = sec->findLowestCeilingSurrounding();
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
void Line::verticalDoor(Mobj& thing)
{
    Door* door;

    int side = 0; // only front sides can be used

    // Check for locks
    Player* player = thing.player;

    switch (special)
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
    Sector* sec = level().sides[sidenum[side ^ 1]].sector;

    if (sec->specialdata)
    {
        door = static_cast<Door*>(sec->specialdata);
        switch (special)
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
    switch (special)
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

    switch (special)
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
            special = 0;
            break;

        case 117: // blazing door raise
            door->type = DoorType::BlazeRaise;
            door->speed = VDOORSPEED * 4;
            break;
        case 118: // blazing door open
            door->type = DoorType::BlazeOpen;
            special = 0;
            door->speed = VDOORSPEED * 4;
            break;
    }

    // find the top and bottom of the movement range
    door->topheight = sec->findLowestCeilingSurrounding();
    door->topheight -= 4 * FRACUNIT;
}

//
// Spawn a door that closes after 30 seconds
//
void Sector::spawnDoorCloseIn30()
{
    Door* door = new (levelAlloc(sizeof(*door))) Door {};

    addThinker(*door);

    specialdata = door;
    special = 0;

    door->sector = this;
    door->direction = 0;
    door->type = DoorType::Normal;
    door->speed = VDOORSPEED;
    door->topcountdown = 30 * 35;
}

//
// Spawn a door that opens after 5 minutes
//
void Sector::spawnDoorRaiseIn5Mins(int)
{
    Door* door = new (levelAlloc(sizeof(*door))) Door {};

    addThinker(*door);

    specialdata = door;
    special = 0;

    door->sector = this;
    door->direction = 2;
    door->type = DoorType::RaiseIn5Mins;
    door->speed = VDOORSPEED;
    door->topheight = findLowestCeilingSurrounding();
    door->topheight -= 4 * FRACUNIT;
    door->topwait = VDOORWAIT;
    door->topcountdown = 5 * 60 * 35;
}
} // namespace Doom
