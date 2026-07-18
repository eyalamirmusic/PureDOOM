// Rewritten out of vanilla p_saveg into namespace Doom.
//
// Savegame serialisation: players, the world (sectors/lines), the thinkers (mobjs),
// and the active specials. Thinkers are identified on write and restored on read by
// their function pointer, which is why the T_* thinker functions and Doom::mobjThinker
// stay global shims - this code compares against and stores those exact addresses.
// p_saveg.cpp shims the eight vanilla names and owns the save_p cursor. Not covered
// by the demos (no save in a demo); migrated copy-for-copy so the byte layout is
// unchanged.

#include "../doom_config.h"

#include "../doomstat.h"
#include "../i_system.h"
#include "../p_local.h"
#include "../r_state.h"

#include "SaveGame.h"
#include "Tick.h" // levelAlloc / levelFree / freeLevelAllocations
#include "Ceilings.h"

#include "Plats.h"
#include <new> // placement new
#include "../Host/System.h"

// save_p is a reference onto Doom::SaveGameState's cursor (an Engine member), bound in
#include "Mobj.h"
// the p_saveg.cpp shim; g_game, the probe and this file share it. This bare extern must
// stay a reference in lockstep with p_saveg.h's - a plain `extern byte* save_p` here
// would write the low half of the reference's pointer and corrupt it.
extern byte*& save_p;

#define PADSAVEP() save_p += (4 - ((long long) save_p & 3)) & 3

// The thinker functions stay global (p_saveg identity); declared so the spawners
// can store their address.

namespace Doom
{
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

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (!playeringame[i])
            continue;

        PADSAVEP();

        dest = reinterpret_cast<Player*>(save_p);
        doom_memcpy(dest, &players[i], sizeof(Player));
        save_p += sizeof(Player);
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
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (!playeringame[i])
            continue;

        PADSAVEP();

        doom_memcpy(&players[i], save_p, sizeof(Player));
        save_p += sizeof(Player);

        // will be set when unarc thinker
        players[i].mo = nullptr;
        players[i].message = nullptr;
        players[i].attacker = nullptr;

        for (int j = 0; j < NUMPSPRITES; j++)
        {
            if (players[i].psprites[j].state)
            {
                players[i].psprites[j].state = &states[reinterpret_cast<long long>(
                    players[i].psprites[j].state)];
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

    put = reinterpret_cast<short*>(save_p);

    // do sectors
    for (i = 0, sec = sectors; i < numsectors; i++, sec++)
    {
        *put++ = sec->floorheight >> FRACBITS;
        *put++ = sec->ceilingheight >> FRACBITS;
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

            *put++ = si->textureoffset >> FRACBITS;
            *put++ = si->rowoffset >> FRACBITS;
            *put++ = si->toptexture;
            *put++ = si->bottomtexture;
            *put++ = si->midtexture;
        }
    }

    save_p = reinterpret_cast<byte*>(put);
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

    get = reinterpret_cast<short*>(save_p);

    // do sectors
    for (i = 0, sec = sectors; i < numsectors; i++, sec++)
    {
        sec->floorheight = *get++ << FRACBITS;
        sec->ceilingheight = *get++ << FRACBITS;
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
            si->textureoffset = *get++ << FRACBITS;
            si->rowoffset = *get++ << FRACBITS;
            si->toptexture = *get++;
            si->bottomtexture = *get++;
            si->midtexture = *get++;
        }
    }

    save_p = reinterpret_cast<byte*>(get);
}

//
// Thinkers
//
enum thinkerclass_t
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
    T* obj = new (levelAlloc(sizeof(T))) T {};
    void* vtable = *reinterpret_cast<void**>(obj);
    doom_memcpy(obj, save_p, sizeof(T));
    *reinterpret_cast<void**>(obj) = vtable;
    save_p += sizeof(T);
    return obj;
}

//
// archiveThinkers
//
void archiveThinkers()
{
    Doom::Thinker* th;
    Mobj* mobj;

    // save off the current thinkers
    for (th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        // A removed-but-not-yet-freed mobj is skipped, as vanilla did (its function
        // was the -1 sentinel, matching no archived type).
        if (th->kind() == Doom::ThinkerKind::Mobj && !th->removed)
        {
            *save_p++ = tc_mobj;
            PADSAVEP();
            mobj = reinterpret_cast<Mobj*>(save_p);
            doom_memcpy(mobj, th, sizeof(*mobj));
            save_p += sizeof(*mobj);
            mobj->state = reinterpret_cast<State*>(mobj->state - states);

            if (mobj->player)
                mobj->player =
                    reinterpret_cast<Player*>((mobj->player - players) + 1);
            continue;
        }

        // fatalError ("archiveThinkers: Unknown thinker function");
    }

    // add a terminating marker
    *save_p++ = tc_end;
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

    // remove all the current thinkers
    currentthinker = thinkercap.next;
    while (currentthinker != &thinkercap)
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
        tclass = *save_p++;
        switch (tclass)
        {
            case tc_end:
                return; // end of list

            case tc_mobj:
                PADSAVEP();
                mobj = unarchiveThinker<Mobj>();
                mobj->state = &states[reinterpret_cast<long long>(mobj->state)];
                mobj->target = nullptr;
                if (mobj->player)
                {
                    mobj->player =
                        &players[reinterpret_cast<long long>(mobj->player) - 1];
                    mobj->player->mo = mobj;
                }
                P_SetThingPosition(mobj);
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

    // save off the current thinkers
    for (th = thinkercap.next; th != &thinkercap; th = th->next)
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
                *save_p++ = tc_ceiling;
                PADSAVEP();
                ceiling = reinterpret_cast<Ceiling*>(save_p);
                doom_memcpy(ceiling, th, sizeof(*ceiling));
                save_p += sizeof(*ceiling);
                ceiling->sector =
                    reinterpret_cast<Sector*>(ceiling->sector - sectors);
            }
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::Ceiling)
        {
            *save_p++ = tc_ceiling;
            PADSAVEP();
            ceiling = reinterpret_cast<Ceiling*>(save_p);
            doom_memcpy(ceiling, th, sizeof(*ceiling));
            save_p += sizeof(*ceiling);
            ceiling->sector = reinterpret_cast<Sector*>(ceiling->sector - sectors);
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::Door)
        {
            *save_p++ = tc_door;
            PADSAVEP();
            door = reinterpret_cast<Door*>(save_p);
            doom_memcpy(door, th, sizeof(*door));
            save_p += sizeof(*door);
            door->sector = reinterpret_cast<Sector*>(door->sector - sectors);
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::Floor)
        {
            *save_p++ = tc_floor;
            PADSAVEP();
            floor = reinterpret_cast<FloorMove*>(save_p);
            doom_memcpy(floor, th, sizeof(*floor));
            save_p += sizeof(*floor);
            floor->sector = reinterpret_cast<Sector*>(floor->sector - sectors);
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::Plat)
        {
            *save_p++ = tc_plat;
            PADSAVEP();
            plat = reinterpret_cast<Plat*>(save_p);
            doom_memcpy(plat, th, sizeof(*plat));
            save_p += sizeof(*plat);
            plat->sector = reinterpret_cast<Sector*>(plat->sector - sectors);
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::LightFlash)
        {
            *save_p++ = tc_flash;
            PADSAVEP();
            flash = reinterpret_cast<LightFlash*>(save_p);
            doom_memcpy(flash, th, sizeof(*flash));
            save_p += sizeof(*flash);
            flash->sector = reinterpret_cast<Sector*>(flash->sector - sectors);
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::StrobeFlash)
        {
            *save_p++ = tc_strobe;
            PADSAVEP();
            strobe = reinterpret_cast<Strobe*>(save_p);
            doom_memcpy(strobe, th, sizeof(*strobe));
            save_p += sizeof(*strobe);
            strobe->sector = reinterpret_cast<Sector*>(strobe->sector - sectors);
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::Glow)
        {
            *save_p++ = tc_glow;
            PADSAVEP();
            glow = reinterpret_cast<Glow*>(save_p);
            doom_memcpy(glow, th, sizeof(*glow));
            save_p += sizeof(*glow);
            glow->sector = reinterpret_cast<Sector*>(glow->sector - sectors);
            continue;
        }
    }

    // add a terminating marker
    *save_p++ = tc_endspecials;
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

    // read in saved thinkers
    while (1)
    {
        tclass = *save_p++;
        switch (tclass)
        {
            case tc_endspecials:
                return; // end of list

            case tc_ceiling:
                PADSAVEP();
                ceiling = unarchiveThinker<Ceiling>();
                ceiling->sector =
                    &sectors[reinterpret_cast<long long>(ceiling->sector)];
                ceiling->sector->specialdata = ceiling;

                Doom::addThinker(ceiling);
                Doom::addActiveCeiling(ceiling);
                break;

            case tc_door:
                PADSAVEP();
                door = unarchiveThinker<Door>();
                door->sector = &sectors[reinterpret_cast<long long>(door->sector)];
                door->sector->specialdata = door;
                Doom::addThinker(door);
                break;

            case tc_floor:
                PADSAVEP();
                floor = unarchiveThinker<FloorMove>();
                floor->sector = &sectors[reinterpret_cast<long long>(floor->sector)];
                floor->sector->specialdata = floor;
                Doom::addThinker(floor);
                break;

            case tc_plat:
                PADSAVEP();
                plat = unarchiveThinker<Plat>();
                plat->sector = &sectors[reinterpret_cast<long long>(plat->sector)];
                plat->sector->specialdata = plat;

                Doom::addThinker(plat);
                Doom::addActivePlat(plat);
                break;

            case tc_flash:
                PADSAVEP();
                flash = unarchiveThinker<LightFlash>();
                flash->sector = &sectors[reinterpret_cast<long long>(flash->sector)];
                Doom::addThinker(flash);
                break;

            case tc_strobe:
                PADSAVEP();
                strobe = unarchiveThinker<Strobe>();
                strobe->sector =
                    &sectors[reinterpret_cast<long long>(strobe->sector)];
                Doom::addThinker(strobe);
                break;

            case tc_glow:
                PADSAVEP();
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
