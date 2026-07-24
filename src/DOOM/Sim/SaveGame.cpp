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
    auto& save = saveGameState();
    auto& players_ = playerState();

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (!players_.playeringame[i])
            continue;

        padSaveCursor(save.cursor);

        Player* dest = reinterpret_cast<Player*>(save.cursor);
        doom_memcpy(dest, &players_.players[i], sizeof(Player));
        save.cursor += sizeof(Player);
        for (int j = 0; j < numPSprites; j++)
        {
            if (dest->psprites[j].state)
            {
                dest->psprites[j].state =
                    reinterpret_cast<State*>(dest->psprites[j].state - states());
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
        players_.players[i].message = {};
        players_.players[i].attacker = nullptr;

        for (int j = 0; j < numPSprites; j++)
        {
            if (players_.players[i].psprites[j].state)
            {
                players_.players[i].psprites[j].state =
                    &states()[reinterpret_cast<long long>(
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

    auto& save = saveGameState();

    short* put = reinterpret_cast<short*>(save.cursor);

    // do sectors
    for (i = 0, sec = level().sectors.data(); i < level().sectors.size(); i++, sec++)
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
    for (i = 0, li = level().lines.data(); i < level().lines.size(); i++, li++)
    {
        *put++ = li->flags;
        *put++ = li->special;
        *put++ = li->tag;
        for (short sidenum: li->sidenum)
        {
            if (sidenum == -1)
                continue;

            Side* si = &level().sides[sidenum];

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

    auto& save = saveGameState();

    short* get = reinterpret_cast<short*>(save.cursor);

    // do sectors
    for (i = 0, sec = level().sectors.data(); i < level().sectors.size(); i++, sec++)
    {
        sec->floorheight = Fixed::fromInt(*get++);
        sec->ceilingheight = Fixed::fromInt(*get++);
        sec->floorpic = *get++;
        sec->ceilingpic = *get++;
        sec->lightlevel = *get++;
        sec->special = *get++; // needed?
        sec->tag = *get++; // needed?
        sec->specialdata = nullptr;
        sec->soundtarget = nullptr;
    }

    // do lines
    for (i = 0, li = level().lines.data(); i < level().lines.size(); i++, li++)
    {
        li->flags = *get++;
        li->special = *get++;
        li->tag = *get++;
        for (short sidenum: li->sidenum)
        {
            if (sidenum == -1)
                continue;
            Side* si = &level().sides[sidenum];
            si->textureoffset = Fixed::fromInt(*get++);
            si->rowoffset = Fixed::fromInt(*get++);
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
enum class ThinkerClass
{
    End,
    Mobj
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

// The archive counterpart, for the specials: the record memcpy'd whole, its
// sector pointer rewritten to an index. It is composed in an aligned local and
// copied out finished, rather than fixed up in place: the save buffer is only
// 4-aligned (padSaveCursor reproduces vanilla's PADSAVEP) while the record's
// pointers want 8 on a 64-bit host, so writing through a pointer into the
// buffer was UB (UBSan flagged each such store). The bytes written are
// identical.
template <typename T>
static void archiveSectorThinker(const Thinker* th)
{
    auto& save = saveGameState();
    padSaveCursor(save.cursor);

    T record {};
    doom_memcpy(&record, th, sizeof(record));
    record.sector =
        reinterpret_cast<Sector*>(record.sector - level().sectors.data());

    doom_memcpy(save.cursor, &record, sizeof(record));
    save.cursor += sizeof(record);
}

//
// archiveThinkers
//
void archiveThinkers()
{
    auto& save = saveGameState();
    auto& thinkers = thinkerList();

    // save off the current thinkers
    for (Thinker* th = thinkers.cap.next; th != &thinkers.cap; th = th->next)
    {
        // A removed-but-not-yet-freed mobj is skipped, as vanilla did (its function
        // was the -1 sentinel, matching no archived type).
        if (th->kind() == ThinkerKind::Mobj && !th->removed)
        {
            *save.cursor++ = static_cast<byte>(ThinkerClass::Mobj);
            padSaveCursor(save.cursor);

            // Composed in an aligned local for the reason archiveSectorThinker
            // gives; the state and player pointers become indices in the copy.
            Mobj mobj {};
            doom_memcpy(&mobj, th, sizeof(mobj));
            mobj.state = reinterpret_cast<State*>(mobj.state - states());

            if (mobj.player)
                mobj.player = reinterpret_cast<Player*>(
                    (mobj.player - playerState().players.data()) + 1);

            doom_memcpy(save.cursor, &mobj, sizeof(mobj));
            save.cursor += sizeof(mobj);
            continue;
        }

        // fatalError ("archiveThinkers: Unknown thinker function");
    }

    // add a terminating marker
    *save.cursor++ = static_cast<byte>(ThinkerClass::End);
}

//
// unArchiveThinkers
//
void unArchiveThinkers()
{
    Mobj* mobj;

    auto& save = saveGameState();
    auto& thinkers = thinkerList();

    // remove all the current thinkers
    Thinker* currentthinker = thinkers.cap.next;
    while (currentthinker != &thinkers.cap)
    {
        Thinker* next = currentthinker->next;

        if (currentthinker->kind() == ThinkerKind::Mobj)
            removeMobj(*reinterpret_cast<Mobj*>(currentthinker));
        else
            levelFree(currentthinker);

        currentthinker = next;
    }
    initThinkers();

    // read in saved thinkers
    while (1)
    {
        byte tclass = *save.cursor++;
        switch (static_cast<ThinkerClass>(tclass))
        {
            case ThinkerClass::End:
                return; // end of list

            case ThinkerClass::Mobj:
                padSaveCursor(save.cursor);
                mobj = unarchiveThinker<Mobj>();
                mobj->state = &states()[reinterpret_cast<long long>(mobj->state)];
                mobj->target = nullptr;
                if (mobj->player)
                {
                    mobj->player = &playerState().players[static_cast<int>(
                        reinterpret_cast<long long>(mobj->player) - 1)];
                    mobj->player->mo = mobj;
                }
                setThingPosition(*mobj);
                mobj->info = &mobjinfo()[toIndex(mobj->type)];
                mobj->floorz = mobj->subsector->sector->floorheight;
                mobj->ceilingz = mobj->subsector->sector->ceilingheight;
                addThinker(*mobj);
                break;

            default:
            {
                //fatalError("Error: Unknown tclass %i in savegame", tclass);

                fatalError("Error: Unknown tclass ", tclass, " in savegame");
            }
        }
    }
}

//
// archiveSpecials
//
enum class SpecialClass
{
    Ceiling,
    Door,
    Floor,
    Plat,
    Flash,
    Strobe,
    Glow,
    EndSpecials
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
    auto& save = saveGameState();
    auto& thinkers = thinkerList();

    // save off the current thinkers
    for (Thinker* th = thinkers.cap.next; th != &thinkers.cap; th = th->next)
    {
        // Skip a removed-but-not-yet-freed thinker (vanilla's -1 function matched
        // no type).
        if (th->removed)
            continue;

        // A crusher in stasis (vanilla nulled its function). Only a ceiling is
        // tracked this way; a stopped plat, as in vanilla, is not archived.
        if (th->stopped)
        {
            if (th->kind() == ThinkerKind::Ceiling)
            {
                *save.cursor++ = static_cast<byte>(SpecialClass::Ceiling);
                archiveSectorThinker<Ceiling>(th);
            }
            continue;
        }

        if (th->kind() == ThinkerKind::Ceiling)
        {
            *save.cursor++ = static_cast<byte>(SpecialClass::Ceiling);
            archiveSectorThinker<Ceiling>(th);
            continue;
        }

        if (th->kind() == ThinkerKind::Door)
        {
            *save.cursor++ = static_cast<byte>(SpecialClass::Door);
            archiveSectorThinker<Door>(th);
            continue;
        }

        if (th->kind() == ThinkerKind::Floor)
        {
            *save.cursor++ = static_cast<byte>(SpecialClass::Floor);
            archiveSectorThinker<FloorMove>(th);
            continue;
        }

        if (th->kind() == ThinkerKind::Plat)
        {
            *save.cursor++ = static_cast<byte>(SpecialClass::Plat);
            archiveSectorThinker<Plat>(th);
            continue;
        }

        if (th->kind() == ThinkerKind::LightFlash)
        {
            *save.cursor++ = static_cast<byte>(SpecialClass::Flash);
            archiveSectorThinker<LightFlash>(th);
            continue;
        }

        if (th->kind() == ThinkerKind::StrobeFlash)
        {
            *save.cursor++ = static_cast<byte>(SpecialClass::Strobe);
            archiveSectorThinker<Strobe>(th);
            continue;
        }

        if (th->kind() == ThinkerKind::Glow)
        {
            *save.cursor++ = static_cast<byte>(SpecialClass::Glow);
            archiveSectorThinker<Glow>(th);
            continue;
        }
    }

    // add a terminating marker
    *save.cursor++ = static_cast<byte>(SpecialClass::EndSpecials);
}

//
// unArchiveSpecials
//
void unArchiveSpecials()
{
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
        byte tclass = *save.cursor++;
        switch (static_cast<SpecialClass>(tclass))
        {
            case SpecialClass::EndSpecials:
                return; // end of list

            case SpecialClass::Ceiling:
                padSaveCursor(save.cursor);
                ceiling = unarchiveThinker<Ceiling>();
                ceiling->sector =
                    &level().sectors[reinterpret_cast<long long>(ceiling->sector)];
                ceiling->sector->specialdata = ceiling;

                addThinker(*ceiling);
                addActiveCeiling(*ceiling);
                break;

            case SpecialClass::Door:
                padSaveCursor(save.cursor);
                door = unarchiveThinker<Door>();
                door->sector =
                    &level().sectors[reinterpret_cast<long long>(door->sector)];
                door->sector->specialdata = door;
                addThinker(*door);
                break;

            case SpecialClass::Floor:
                padSaveCursor(save.cursor);
                floor = unarchiveThinker<FloorMove>();
                floor->sector =
                    &level().sectors[reinterpret_cast<long long>(floor->sector)];
                floor->sector->specialdata = floor;
                addThinker(*floor);
                break;

            case SpecialClass::Plat:
                padSaveCursor(save.cursor);
                plat = unarchiveThinker<Plat>();
                plat->sector =
                    &level().sectors[reinterpret_cast<long long>(plat->sector)];
                plat->sector->specialdata = plat;

                addThinker(*plat);
                addActivePlat(*plat);
                break;

            case SpecialClass::Flash:
                padSaveCursor(save.cursor);
                flash = unarchiveThinker<LightFlash>();
                flash->sector =
                    &level().sectors[reinterpret_cast<long long>(flash->sector)];
                addThinker(*flash);
                break;

            case SpecialClass::Strobe:
                padSaveCursor(save.cursor);
                strobe = unarchiveThinker<Strobe>();
                strobe->sector =
                    &level().sectors[reinterpret_cast<long long>(strobe->sector)];
                addThinker(*strobe);
                break;

            case SpecialClass::Glow:
                padSaveCursor(save.cursor);
                glow = unarchiveThinker<Glow>();
                glow->sector =
                    &level().sectors[reinterpret_cast<long long>(glow->sector)];
                addThinker(*glow);
                break;

            default:
            {
                //fatalError("Error: P_UnarchiveSpecials:Unknown tclass %i "
                //        "in savegame", tclass);

                fatalError("Error: P_UnarchiveSpecials:Unknown tclass ",
                           tclass,
                           " in savegame");
            }
        }
    }
}
} // namespace Doom
