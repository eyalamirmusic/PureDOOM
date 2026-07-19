// Rewritten out of vanilla p_saveg into namespace Doom.
//
// Savegame serialisation: players, the world (sectors/lines), the thinkers (mobjs),
// and the active specials. Thinkers are identified on write and restored on read by
// their function pointer, which is why the T_* thinker functions and Doom::mobjThinker
// stay global shims - this code compares against and stores those exact addresses.
// p_saveg.cpp shims the eight vanilla names and owns the save.cursor cursor. Not covered
// by the demos (no save in a demo); migrated copy-for-copy so the byte layout is
// unchanged.

#include "../Host/Platform.h"

#include "../Game/MapSpawns.h"
#include "SimDefs.h"

#include "../Game/PlayerState.h"
#include "../Game/SaveGameState.h"
#include "SaveGame.h"
#include "ThinkerList.h"
#include "Tick.h" // levelAlloc / levelFree / freeLevelAllocations
#include "Ceilings.h"

#include "Plats.h"
#include <cstdint>
#include <new> // placement new
#include "../Host/System.h"

// save.cursor is a reference onto Doom::SaveGameState's cursor (an Engine member), bound in
#include "MapUtil.h"
#include "Mobj.h"
// the p_saveg.cpp shim; g_game, the probe and this file share it. This bare extern must
// stay a reference in lockstep with p_saveg.h's - a plain `extern byte* save.cursor` here
// would write the low half of the reference's pointer and corrupt it.

// The thinker functions stay global (p_saveg identity); declared so the spawners
// can store their address.

namespace Doom
{
// Advances the save cursor up to the next 4-byte boundary. Was PADSAVEP(p):
// p += (4 - ((long long) p & 3)) & 3 - kept bit-identical, the arithmetic is
// a serialisation format and the byte offsets must not move.
void padSaveCursor(byte*& cursor)
{
    cursor += (4 - (reinterpret_cast<std::uintptr_t>(cursor) & 3)) & 3;
}

// Forward declarations so the file's own call order needs no rearranging.
void archivePlayers();
void unArchivePlayers();
void archiveWorld();
void unArchiveWorld();
void archiveThinkers();
void unArchiveThinkers();
void archiveSpecials();
void unArchiveSpecials();

void archivePlayers()
{
    Player* dest;

    auto& save = saveGameState();
    auto& players_ = playerState();

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (!players_.playeringame[i])
            continue;

        padSaveCursor(save.cursor);

        dest = reinterpret_cast<Player*>(save.cursor);
        doom_memcpy(dest, &players_.players[i], sizeof(Player));
        save.cursor += sizeof(Player);
        for (int j = 0; j < NUMPSPRITES; j++)
        {
            if (dest->psprites[j].state)
            {
                dest->psprites[j].state =
                    reinterpret_cast<State*>(dest->psprites[j].state - states);
            }
        }
    }
}

//
// unArchivePlayers
//
void unArchivePlayers()
{
    auto& save = saveGameState();
    auto& players_ = playerState();

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (!players_.playeringame[i])
            continue;

        padSaveCursor(save.cursor);

        doom_memcpy(&players_.players[i], save.cursor, sizeof(Player));
        save.cursor += sizeof(Player);

        // will be set when unarc thinker
        players_.players[i].mo = nullptr;
        players_.players[i].message = nullptr;
        players_.players[i].attacker = nullptr;

        for (int j = 0; j < NUMPSPRITES; j++)
        {
            if (players_.players[i].psprites[j].state)
            {
                players_.players[i].psprites[j].state =
                    &states[reinterpret_cast<long long>(
                        players_.players[i].psprites[j].state)];
            }
        }
    }
}

//
// archiveWorld
//
void archiveWorld()
{
    int i;
    Sector* sec;
    Line* li;
    Side* si;
    short* put;

    auto& save = saveGameState();

    put = reinterpret_cast<short*>(save.cursor);

    // do sectors
    for (i = 0, sec = sectors; i < numsectors; i++, sec++)
    {
        // The on-disk format stores heights in WHOLE map units, as vanilla's
        // `>> fracBits` into a short did - so toInt(), not raw.
        *put++ = sec->floorheight.toInt();
        *put++ = sec->ceilingheight.toInt();
        *put++ = sec->floorpic;
        *put++ = sec->ceilingpic;
        *put++ = sec->lightlevel;
        *put++ = sec->special; // needed?
        *put++ = sec->tag; // needed?
    }

    // do lines
    for (i = 0, li = lines; i < numlines; i++, li++)
    {
        *put++ = li->flags;
        *put++ = li->special;
        *put++ = li->tag;
        for (int j = 0; j < 2; j++)
        {
            if (li->sidenum[j] == -1)
                continue;

            si = &sides[li->sidenum[j]];

            *put++ = si->textureoffset.toInt();
            *put++ = si->rowoffset.toInt();
            *put++ = si->toptexture;
            *put++ = si->bottomtexture;
            *put++ = si->midtexture;
        }
    }

    save.cursor = reinterpret_cast<byte*>(put);
}

//
// unArchiveWorld
//
void unArchiveWorld()
{
    int i;
    Sector* sec;
    Line* li;
    Side* si;
    short* get;

    auto& save = saveGameState();

    get = reinterpret_cast<short*>(save.cursor);

    // do sectors
    for (i = 0, sec = sectors; i < numsectors; i++, sec++)
    {
        sec->floorheight = Doom::Fixed::fromInt(*get++);
        sec->ceilingheight = Doom::Fixed::fromInt(*get++);
        sec->floorpic = *get++;
        sec->ceilingpic = *get++;
        sec->lightlevel = *get++;
        sec->special = *get++; // needed?
        sec->tag = *get++; // needed?
        sec->specialdata = nullptr;
        sec->soundtarget = nullptr;
    }

    // do lines
    for (i = 0, li = lines; i < numlines; i++, li++)
    {
        li->flags = *get++;
        li->special = *get++;
        li->tag = *get++;
        for (int j = 0; j < 2; j++)
        {
            if (li->sidenum[j] == -1)
                continue;
            si = &sides[li->sidenum[j]];
            si->textureoffset = Doom::Fixed::fromInt(*get++);
            si->rowoffset = Doom::Fixed::fromInt(*get++);
            si->toptexture = *get++;
            si->bottomtexture = *get++;
            si->midtexture = *get++;
        }
    }

    save.cursor = reinterpret_cast<byte*>(get);
}

//
// Thinkers
//
enum ThinkerClass
{
    tc_end,
    tc_mobj
};

// Reconstruct a saved thinker in a fresh level-pool block. placement-new sets the
// vtable (and the base's prev/next/removed/stopped), then the saved bytes are copied
// back over the object exactly as vanilla's whole-struct memcpy did - but the vtable
// pointer is preserved across the copy, since the bytes on disk carry a stale one.
// Every derived field lands byte-identical; only the (reconstructed) linkage and the
// vtable are not taken from the save. This is what lets p_saveg keep memcpy'ing a now
// polymorphic Mobj / special without corrupting its dispatch.
template <typename T>
static T* unarchiveThinker()
{
    auto& save = saveGameState();
    T* obj = new (levelAlloc(sizeof(T))) T {};
    void* vtable = *reinterpret_cast<void**>(obj);
    doom_memcpy(obj, save.cursor, sizeof(T));
    *reinterpret_cast<void**>(obj) = vtable;
    save.cursor += sizeof(T);
    return obj;
}

//
// archiveThinkers
//
void archiveThinkers()
{
    Doom::Thinker* th;
    Mobj* mobj;

    auto& save = saveGameState();
    auto& thinkers = thinkerList();

    // save off the current thinkers
    for (th = thinkers.cap.next; th != &thinkers.cap; th = th->next)
    {
        // A removed-but-not-yet-freed mobj is skipped, as vanilla did (its function
        // was the -1 sentinel, matching no archived type).
        if (th->kind() == Doom::ThinkerKind::Mobj && !th->removed)
        {
            *save.cursor++ = tc_mobj;
            padSaveCursor(save.cursor);
            mobj = reinterpret_cast<Mobj*>(save.cursor);
            doom_memcpy(mobj, th, sizeof(*mobj));
            save.cursor += sizeof(*mobj);
            mobj->state = reinterpret_cast<State*>(mobj->state - states);

            if (mobj->player)
                mobj->player = reinterpret_cast<Player*>(
                    (mobj->player - playerState().players.data()) + 1);
            continue;
        }

        // fatalError ("archiveThinkers: Unknown thinker function");
    }

    // add a terminating marker
    *save.cursor++ = tc_end;
}

//
// unArchiveThinkers
//
void unArchiveThinkers()
{
    byte tclass;
    Doom::Thinker* currentthinker;
    Doom::Thinker* next;
    Mobj* mobj;

    auto& save = saveGameState();
    auto& thinkers = thinkerList();

    // remove all the current thinkers
    currentthinker = thinkers.cap.next;
    while (currentthinker != &thinkers.cap)
    {
        next = currentthinker->next;

        if (currentthinker->kind() == Doom::ThinkerKind::Mobj)
            Doom::removeMobj(reinterpret_cast<Mobj*>(currentthinker));
        else
            levelFree(currentthinker);

        currentthinker = next;
    }
    Doom::initThinkers();

    // read in saved thinkers
    while (1)
    {
        tclass = *save.cursor++;
        switch (tclass)
        {
            case tc_end:
                return; // end of list

            case tc_mobj:
                padSaveCursor(save.cursor);
                mobj = unarchiveThinker<Mobj>();
                mobj->state = &states[reinterpret_cast<long long>(mobj->state)];
                mobj->target = nullptr;
                if (mobj->player)
                {
                    mobj->player =
                        &playerState()
                             .players[reinterpret_cast<long long>(mobj->player) - 1];
                    mobj->player->mo = mobj;
                }
                setThingPosition(*mobj);
                mobj->info = &mobjinfo[mobj->type];
                mobj->floorz = mobj->subsector->sector->floorheight;
                mobj->ceilingz = mobj->subsector->sector->ceilingheight;
                Doom::addThinker(mobj);
                break;

            default:
            {
                //fatalError("Error: Unknown tclass %i in savegame", tclass);

                doom_strcpy(error_buf, "Error: Unknown tclass ");
                doom_concat(error_buf, doom_itoa(tclass, 10));
                doom_concat(error_buf, " in savegame");
                fatalError(error_buf);
            }
        }
    }
}

//
// archiveSpecials
//
enum
{
    tc_ceiling,
    tc_door,
    tc_floor,
    tc_plat,
    tc_flash,
    tc_strobe,
    tc_glow,
    tc_endspecials
};

//
// Things to handle:
//
// T_MoveCeiling, (Ceiling: Sector * swizzle), - active list
// T_VerticalDoor, (Door: Sector * swizzle),
// T_MoveFloor, (FloorMove: Sector * swizzle),
// T_LightFlash, (LightFlash: Sector * swizzle),
// T_StrobeFlash, (Strobe: Sector *),
// T_Glow, (Glow: Sector *),
// T_PlatRaise, (Plat: Sector *), - active list
//
void archiveSpecials()
{
    Doom::Thinker* th;
    Ceiling* ceiling;
    Door* door;
    FloorMove* floor;
    Plat* plat;
    LightFlash* flash;
    Strobe* strobe;
    Glow* glow;

    auto& save = saveGameState();
    auto& thinkers = thinkerList();

    // save off the current thinkers
    for (th = thinkers.cap.next; th != &thinkers.cap; th = th->next)
    {
        // Skip a removed-but-not-yet-freed thinker (vanilla's -1 function matched
        // no type).
        if (th->removed)
            continue;

        // A crusher in stasis (vanilla nulled its function). Only a ceiling is
        // tracked this way; a stopped plat, as in vanilla, is not archived.
        if (th->stopped)
        {
            if (th->kind() == Doom::ThinkerKind::Ceiling)
            {
                *save.cursor++ = tc_ceiling;
                padSaveCursor(save.cursor);
                ceiling = reinterpret_cast<Ceiling*>(save.cursor);
                doom_memcpy(ceiling, th, sizeof(*ceiling));
                save.cursor += sizeof(*ceiling);
                ceiling->sector =
                    reinterpret_cast<Sector*>(ceiling->sector - sectors);
            }
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::Ceiling)
        {
            *save.cursor++ = tc_ceiling;
            padSaveCursor(save.cursor);
            ceiling = reinterpret_cast<Ceiling*>(save.cursor);
            doom_memcpy(ceiling, th, sizeof(*ceiling));
            save.cursor += sizeof(*ceiling);
            ceiling->sector = reinterpret_cast<Sector*>(ceiling->sector - sectors);
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::Door)
        {
            *save.cursor++ = tc_door;
            padSaveCursor(save.cursor);
            door = reinterpret_cast<Door*>(save.cursor);
            doom_memcpy(door, th, sizeof(*door));
            save.cursor += sizeof(*door);
            door->sector = reinterpret_cast<Sector*>(door->sector - sectors);
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::Floor)
        {
            *save.cursor++ = tc_floor;
            padSaveCursor(save.cursor);
            floor = reinterpret_cast<FloorMove*>(save.cursor);
            doom_memcpy(floor, th, sizeof(*floor));
            save.cursor += sizeof(*floor);
            floor->sector = reinterpret_cast<Sector*>(floor->sector - sectors);
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::Plat)
        {
            *save.cursor++ = tc_plat;
            padSaveCursor(save.cursor);
            plat = reinterpret_cast<Plat*>(save.cursor);
            doom_memcpy(plat, th, sizeof(*plat));
            save.cursor += sizeof(*plat);
            plat->sector = reinterpret_cast<Sector*>(plat->sector - sectors);
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::LightFlash)
        {
            *save.cursor++ = tc_flash;
            padSaveCursor(save.cursor);
            flash = reinterpret_cast<LightFlash*>(save.cursor);
            doom_memcpy(flash, th, sizeof(*flash));
            save.cursor += sizeof(*flash);
            flash->sector = reinterpret_cast<Sector*>(flash->sector - sectors);
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::StrobeFlash)
        {
            *save.cursor++ = tc_strobe;
            padSaveCursor(save.cursor);
            strobe = reinterpret_cast<Strobe*>(save.cursor);
            doom_memcpy(strobe, th, sizeof(*strobe));
            save.cursor += sizeof(*strobe);
            strobe->sector = reinterpret_cast<Sector*>(strobe->sector - sectors);
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::Glow)
        {
            *save.cursor++ = tc_glow;
            padSaveCursor(save.cursor);
            glow = reinterpret_cast<Glow*>(save.cursor);
            doom_memcpy(glow, th, sizeof(*glow));
            save.cursor += sizeof(*glow);
            glow->sector = reinterpret_cast<Sector*>(glow->sector - sectors);
            continue;
        }
    }

    // add a terminating marker
    *save.cursor++ = tc_endspecials;
}

//
// unArchiveSpecials
//
void unArchiveSpecials()
{
    byte tclass;
    Ceiling* ceiling;
    Door* door;
    FloorMove* floor;
    Plat* plat;
    LightFlash* flash;
    Strobe* strobe;
    Glow* glow;

    auto& save = saveGameState();

    // read in saved thinkers
    while (1)
    {
        tclass = *save.cursor++;
        switch (tclass)
        {
            case tc_endspecials:
                return; // end of list

            case tc_ceiling:
                padSaveCursor(save.cursor);
                ceiling = unarchiveThinker<Ceiling>();
                ceiling->sector =
                    &sectors[reinterpret_cast<long long>(ceiling->sector)];
                ceiling->sector->specialdata = ceiling;

                Doom::addThinker(ceiling);
                Doom::addActiveCeiling(ceiling);
                break;

            case tc_door:
                padSaveCursor(save.cursor);
                door = unarchiveThinker<Door>();
                door->sector = &sectors[reinterpret_cast<long long>(door->sector)];
                door->sector->specialdata = door;
                Doom::addThinker(door);
                break;

            case tc_floor:
                padSaveCursor(save.cursor);
                floor = unarchiveThinker<FloorMove>();
                floor->sector = &sectors[reinterpret_cast<long long>(floor->sector)];
                floor->sector->specialdata = floor;
                Doom::addThinker(floor);
                break;

            case tc_plat:
                padSaveCursor(save.cursor);
                plat = unarchiveThinker<Plat>();
                plat->sector = &sectors[reinterpret_cast<long long>(plat->sector)];
                plat->sector->specialdata = plat;

                Doom::addThinker(plat);
                Doom::addActivePlat(plat);
                break;

            case tc_flash:
                padSaveCursor(save.cursor);
                flash = unarchiveThinker<LightFlash>();
                flash->sector = &sectors[reinterpret_cast<long long>(flash->sector)];
                Doom::addThinker(flash);
                break;

            case tc_strobe:
                padSaveCursor(save.cursor);
                strobe = unarchiveThinker<Strobe>();
                strobe->sector =
                    &sectors[reinterpret_cast<long long>(strobe->sector)];
                Doom::addThinker(strobe);
                break;

            case tc_glow:
                padSaveCursor(save.cursor);
                glow = unarchiveThinker<Glow>();
                glow->sector = &sectors[reinterpret_cast<long long>(glow->sector)];
                Doom::addThinker(glow);
                break;

            default:
            {
                //fatalError("Error: P_UnarchiveSpecials:Unknown tclass %i "
                //        "in savegame", tclass);

                doom_strcpy(error_buf, "Error: P_UnarchiveSpecials:Unknown tclass ");
                doom_concat(error_buf, doom_itoa(tclass, 10));
                doom_concat(error_buf, " in savegame");
                fatalError(error_buf);
            }
        }
    }
}
} // namespace Doom
