// Rewritten out of vanilla p_saveg into namespace Doom.
//
// Savegame serialisation: players, the world (sectors/lines), the thinkers (mobjs),
// and the active specials. Thinkers are identified on write and restored on read by
// their function pointer, which is why the T_* thinker functions and mobjThinker
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
#include "Tick.h" // addThinker / removeThinker
#include "Ceilings.h"

#include "Plats.h"
#include <cstdint>
#include "../Host/System.h"

// save.cursor is a reference onto SaveGameState's cursor (an Engine member), bound in
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

    for (auto i = 0; i < MAXPLAYERS; i++)
    {
        if (!players_.playeringame[i])
            continue;

        padSaveCursor(save.cursor);

        auto* dest = reinterpret_cast<Player*>(save.cursor);
        doom_memcpy(dest, &players_.players[i], sizeof(Player));
        save.cursor += sizeof(Player);
        for (auto& psp: dest->psprites)
        {
            if (psp.state)
                psp.state = reinterpret_cast<State*>(psp.state - states());
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

    for (auto i = 0; i < MAXPLAYERS; i++)
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

        for (auto& psp: players_.players[i].psprites)
        {
            if (psp.state)
                psp.state = &states()[reinterpret_cast<long long>(psp.state)];
        }
    }
}

//
// archiveWorld
//
void archiveWorld()
{
    auto& save = saveGameState();

    auto* put = reinterpret_cast<short*>(save.cursor);

    // do sectors
    for (const auto& sec: level().sectors)
    {
        // The on-disk format stores heights in WHOLE map units, as vanilla's
        // `>> fracBits` into a short did - so toInt(), not raw.
        *put++ = sec.floorheight.toInt();
        *put++ = sec.ceilingheight.toInt();
        *put++ = sec.floorpic;
        *put++ = sec.ceilingpic;
        *put++ = sec.lightlevel;
        *put++ = sec.special; // needed?
        *put++ = sec.tag; // needed?
    }

    // do lines
    for (const auto& li: level().lines)
    {
        *put++ = li.flags;
        *put++ = li.special;
        *put++ = li.tag;
        for (short sidenum: li.sidenum)
        {
            if (sidenum == -1)
                continue;

            auto* si = &level().sides[sidenum];

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
    auto& save = saveGameState();

    auto* get = reinterpret_cast<short*>(save.cursor);

    // do sectors
    for (auto& sec: level().sectors)
    {
        sec.floorheight = Fixed::fromInt(*get++);
        sec.ceilingheight = Fixed::fromInt(*get++);
        sec.floorpic = *get++;
        sec.ceilingpic = *get++;
        sec.lightlevel = *get++;
        sec.special = *get++; // needed?
        sec.tag = *get++; // needed?
        sec.specialdata = nullptr;
        sec.soundtarget = nullptr;
    }

    // do lines
    for (auto& li: level().lines)
    {
        li.flags = *get++;
        li.special = *get++;
        li.tag = *get++;
        for (short sidenum: li.sidenum)
        {
            if (sidenum == -1)
                continue;
            auto* si = &level().sides[sidenum];
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

// Reconstruct a saved thinker at the back of the thinker list. Construction sets the
// vtable (and the base's removed/stopped), then the saved bytes are copied back over
// the object exactly as vanilla's whole-struct memcpy did - but the vtable pointer is
// preserved across the copy, since the bytes on disk carry a stale one. Every derived
// field lands byte-identical; only the vtable is not taken from the save. This is what
// lets p_saveg keep memcpy'ing a now polymorphic Mobj / special without corrupting its
// dispatch.
//
// The thinker is registered here rather than by an addThinker call at the end of each
// case below, which is where the list linkage used to be restored. The order is
// unchanged - the cases run in file order and each appends exactly once - and nothing
// between here and a case's last fixup walks the list.
template <typename T>
static T* unarchiveThinker()
{
    auto& save = saveGameState();
    auto* obj = &addThinker<T>();
    auto* vtable = *reinterpret_cast<void**>(obj);
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
//
// T is deduced from the special handed in rather than named at the call site, which
// is what lets archiveSpecials state each type exactly once - see there.
template <typename T>
static void archiveSectorThinker(const T& special)
{
    auto& save = saveGameState();
    padSaveCursor(save.cursor);

    T record {};
    doom_memcpy(&record, &special, sizeof(record));
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

    // save off the current thinkers
    for (auto& th: thinkerList())
    {
        // A removed-but-not-yet-freed mobj is skipped, as vanilla did (its function
        // was the -1 sentinel, matching no archived type).
        if (th->asMobj() && !th->removed)
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

    // Remove all the current thinkers. Mobj::remove is called for its side effects -
    // it unlinks from the blockmap and sector lists, stops any sound the mobj owns,
    // and may push a respawn queue entry - and its own deallocation is lazy, so the
    // clear below is what actually frees them. That used to leave every mobj orphaned
    // on the level pool until the next level load, the freeing being split across two
    // lists; one owning list has no such gap.
    for (auto& thinker: thinkers)
        if (auto* mobj = thinker->asMobj())
            mobj->remove();

    thinkers.clear();

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
                mobj->setPosition();
                mobj->info = &mobjinfo()[toIndex(mobj->type)];
                mobj->floorz = mobj->subsector->sector->floorheight;
                mobj->ceilingz = mobj->subsector->sector->ceilingheight;
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
// Write one special: its class byte, then its whole record. This is the only place
// in the engine that needs to know *which* special a Thinker is, and the only one
// that cannot ask a virtual for it - archiveSectorThinker needs the static type, to
// size the record it memcpy's to disk. So it dynamic_casts, which is affordable
// (this runs once per save, over a few dozen specials) and is what deleted the type
// tag: `kind() == ThinkerKind::Ceiling` next to `archiveSectorThinker<Ceiling>` was
// two independent statements of one type that no compiler checked against each
// other. Here the type is written once and the cast produces it.
//
// What remains paired is the type and its *wire* tag, and that pairing is the file
// format itself - it cannot be derived from anything.
template <typename T>
static bool archiveSpecialIfType(const Thinker& th, SpecialClass cls)
{
    const auto* special = dynamic_cast<const T*>(&th);

    if (!special)
        return false;

    auto& save = saveGameState();
    *save.cursor++ = static_cast<byte>(cls);
    archiveSectorThinker(*special);

    return true;
}

void archiveSpecials()
{
    auto& save = saveGameState();

    // save off the current thinkers
    for (auto& th: thinkerList())
    {
        // Skip a removed-but-not-yet-freed thinker (vanilla's -1 function matched
        // no type).
        if (th->removed)
            continue;

        // A crusher in stasis (vanilla nulled its function). Only a ceiling is
        // tracked this way; a stopped plat, as in vanilla, is not archived.
        if (th->stopped)
        {
            archiveSpecialIfType<Ceiling>(*th, SpecialClass::Ceiling);
            continue;
        }

        archiveSpecialIfType<Ceiling>(*th, SpecialClass::Ceiling)
            || archiveSpecialIfType<Door>(*th, SpecialClass::Door)
            || archiveSpecialIfType<FloorMove>(*th, SpecialClass::Floor)
            || archiveSpecialIfType<Plat>(*th, SpecialClass::Plat)
            || archiveSpecialIfType<LightFlash>(*th, SpecialClass::Flash)
            || archiveSpecialIfType<Strobe>(*th, SpecialClass::Strobe)
            || archiveSpecialIfType<Glow>(*th, SpecialClass::Glow);
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

                addActiveCeiling(*ceiling);
                break;

            case SpecialClass::Door:
                padSaveCursor(save.cursor);
                door = unarchiveThinker<Door>();
                door->sector =
                    &level().sectors[reinterpret_cast<long long>(door->sector)];
                door->sector->specialdata = door;
                break;

            case SpecialClass::Floor:
                padSaveCursor(save.cursor);
                floor = unarchiveThinker<FloorMove>();
                floor->sector =
                    &level().sectors[reinterpret_cast<long long>(floor->sector)];
                floor->sector->specialdata = floor;
                break;

            case SpecialClass::Plat:
                padSaveCursor(save.cursor);
                plat = unarchiveThinker<Plat>();
                plat->sector =
                    &level().sectors[reinterpret_cast<long long>(plat->sector)];
                plat->sector->specialdata = plat;

                addActivePlat(*plat);
                break;

            case SpecialClass::Flash:
                padSaveCursor(save.cursor);
                flash = unarchiveThinker<LightFlash>();
                flash->sector =
                    &level().sectors[reinterpret_cast<long long>(flash->sector)];
                break;

            case SpecialClass::Strobe:
                padSaveCursor(save.cursor);
                strobe = unarchiveThinker<Strobe>();
                strobe->sector =
                    &level().sectors[reinterpret_cast<long long>(strobe->sector)];
                break;

            case SpecialClass::Glow:
                padSaveCursor(save.cursor);
                glow = unarchiveThinker<Glow>();
                glow->sector =
                    &level().sectors[reinterpret_cast<long long>(glow->sector)];
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
