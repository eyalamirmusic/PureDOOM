// Rewritten out of vanilla p_saveg into namespace Doom.
//
// Savegame serialisation: players, the world (sectors/lines), the thinkers (mobjs),
// and the active specials. Thinkers are identified on write and restored on read by
// their function pointer, which is why the T_* thinker functions and P_MobjThinker
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

#include <new> // placement new

// save_p is a reference onto Doom::SaveGameState's cursor (an Engine member), bound in
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
    player_t* dest;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (!playeringame[i])
            continue;

        PADSAVEP();

        dest = reinterpret_cast<player_t*>(save_p);
        doom_memcpy(dest, &players[i], sizeof(player_t));
        save_p += sizeof(player_t);
        for (int j = 0; j < NUMPSPRITES; j++)
        {
            if (dest->psprites[j].state)
            {
                dest->psprites[j].state =
                    reinterpret_cast<state_t*>(dest->psprites[j].state - states);
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

        doom_memcpy(&players[i], save_p, sizeof(player_t));
        save_p += sizeof(player_t);

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
    sector_t* sec;
    line_t* li;
    side_t* si;
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
    sector_t* sec;
    line_t* li;
    side_t* si;
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
// polymorphic mobj_t / special without corrupting its dispatch.
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
    thinker_t* th;
    mobj_t* mobj;

    // save off the current thinkers
    for (th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        // A removed-but-not-yet-freed mobj is skipped, as vanilla did (its function
        // was the -1 sentinel, matching no archived type).
        if (th->kind() == Doom::ThinkerKind::Mobj && !th->removed)
        {
            *save_p++ = tc_mobj;
            PADSAVEP();
            mobj = reinterpret_cast<mobj_t*>(save_p);
            doom_memcpy(mobj, th, sizeof(*mobj));
            save_p += sizeof(*mobj);
            mobj->state = reinterpret_cast<state_t*>(mobj->state - states);

            if (mobj->player)
                mobj->player =
                    reinterpret_cast<player_t*>((mobj->player - players) + 1);
            continue;
        }

        // I_Error ("archiveThinkers: Unknown thinker function");
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
    thinker_t* currentthinker;
    thinker_t* next;
    mobj_t* mobj;

    // remove all the current thinkers
    currentthinker = thinkercap.next;
    while (currentthinker != &thinkercap)
    {
        next = currentthinker->next;

        if (currentthinker->kind() == Doom::ThinkerKind::Mobj)
            P_RemoveMobj(reinterpret_cast<mobj_t*>(currentthinker));
        else
            levelFree(currentthinker);

        currentthinker = next;
    }
    P_InitThinkers();

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
                mobj = unarchiveThinker<mobj_t>();
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
                P_AddThinker(mobj);
                break;

            default:
            {
                //I_Error("Error: Unknown tclass %i in savegame", tclass);

                doom_strcpy(error_buf, "Error: Unknown tclass ");
                doom_concat(error_buf, doom_itoa(tclass, 10));
                doom_concat(error_buf, " in savegame");
                I_Error(error_buf);
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
// T_MoveCeiling, (ceiling_t: sector_t * swizzle), - active list
// T_VerticalDoor, (vldoor_t: sector_t * swizzle),
// T_MoveFloor, (floormove_t: sector_t * swizzle),
// T_LightFlash, (lightflash_t: sector_t * swizzle),
// T_StrobeFlash, (strobe_t: sector_t *),
// T_Glow, (glow_t: sector_t *),
// T_PlatRaise, (plat_t: sector_t *), - active list
//
void archiveSpecials()
{
    thinker_t* th;
    ceiling_t* ceiling;
    vldoor_t* door;
    floormove_t* floor;
    plat_t* plat;
    lightflash_t* flash;
    strobe_t* strobe;
    glow_t* glow;

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
                ceiling = reinterpret_cast<ceiling_t*>(save_p);
                doom_memcpy(ceiling, th, sizeof(*ceiling));
                save_p += sizeof(*ceiling);
                ceiling->sector =
                    reinterpret_cast<sector_t*>(ceiling->sector - sectors);
            }
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::Ceiling)
        {
            *save_p++ = tc_ceiling;
            PADSAVEP();
            ceiling = reinterpret_cast<ceiling_t*>(save_p);
            doom_memcpy(ceiling, th, sizeof(*ceiling));
            save_p += sizeof(*ceiling);
            ceiling->sector = reinterpret_cast<sector_t*>(ceiling->sector - sectors);
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::Door)
        {
            *save_p++ = tc_door;
            PADSAVEP();
            door = reinterpret_cast<vldoor_t*>(save_p);
            doom_memcpy(door, th, sizeof(*door));
            save_p += sizeof(*door);
            door->sector = reinterpret_cast<sector_t*>(door->sector - sectors);
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::Floor)
        {
            *save_p++ = tc_floor;
            PADSAVEP();
            floor = reinterpret_cast<floormove_t*>(save_p);
            doom_memcpy(floor, th, sizeof(*floor));
            save_p += sizeof(*floor);
            floor->sector = reinterpret_cast<sector_t*>(floor->sector - sectors);
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::Plat)
        {
            *save_p++ = tc_plat;
            PADSAVEP();
            plat = reinterpret_cast<plat_t*>(save_p);
            doom_memcpy(plat, th, sizeof(*plat));
            save_p += sizeof(*plat);
            plat->sector = reinterpret_cast<sector_t*>(plat->sector - sectors);
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::LightFlash)
        {
            *save_p++ = tc_flash;
            PADSAVEP();
            flash = reinterpret_cast<lightflash_t*>(save_p);
            doom_memcpy(flash, th, sizeof(*flash));
            save_p += sizeof(*flash);
            flash->sector = reinterpret_cast<sector_t*>(flash->sector - sectors);
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::StrobeFlash)
        {
            *save_p++ = tc_strobe;
            PADSAVEP();
            strobe = reinterpret_cast<strobe_t*>(save_p);
            doom_memcpy(strobe, th, sizeof(*strobe));
            save_p += sizeof(*strobe);
            strobe->sector = reinterpret_cast<sector_t*>(strobe->sector - sectors);
            continue;
        }

        if (th->kind() == Doom::ThinkerKind::Glow)
        {
            *save_p++ = tc_glow;
            PADSAVEP();
            glow = reinterpret_cast<glow_t*>(save_p);
            doom_memcpy(glow, th, sizeof(*glow));
            save_p += sizeof(*glow);
            glow->sector = reinterpret_cast<sector_t*>(glow->sector - sectors);
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
    ceiling_t* ceiling;
    vldoor_t* door;
    floormove_t* floor;
    plat_t* plat;
    lightflash_t* flash;
    strobe_t* strobe;
    glow_t* glow;

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
                ceiling = unarchiveThinker<ceiling_t>();
                ceiling->sector =
                    &sectors[reinterpret_cast<long long>(ceiling->sector)];
                ceiling->sector->specialdata = ceiling;

                P_AddThinker(ceiling);
                P_AddActiveCeiling(ceiling);
                break;

            case tc_door:
                PADSAVEP();
                door = unarchiveThinker<vldoor_t>();
                door->sector = &sectors[reinterpret_cast<long long>(door->sector)];
                door->sector->specialdata = door;
                P_AddThinker(door);
                break;

            case tc_floor:
                PADSAVEP();
                floor = unarchiveThinker<floormove_t>();
                floor->sector = &sectors[reinterpret_cast<long long>(floor->sector)];
                floor->sector->specialdata = floor;
                P_AddThinker(floor);
                break;

            case tc_plat:
                PADSAVEP();
                plat = unarchiveThinker<plat_t>();
                plat->sector = &sectors[reinterpret_cast<long long>(plat->sector)];
                plat->sector->specialdata = plat;

                P_AddThinker(plat);
                P_AddActivePlat(plat);
                break;

            case tc_flash:
                PADSAVEP();
                flash = unarchiveThinker<lightflash_t>();
                flash->sector = &sectors[reinterpret_cast<long long>(flash->sector)];
                P_AddThinker(flash);
                break;

            case tc_strobe:
                PADSAVEP();
                strobe = unarchiveThinker<strobe_t>();
                strobe->sector =
                    &sectors[reinterpret_cast<long long>(strobe->sector)];
                P_AddThinker(strobe);
                break;

            case tc_glow:
                PADSAVEP();
                glow = unarchiveThinker<glow_t>();
                glow->sector = &sectors[reinterpret_cast<long long>(glow->sector)];
                P_AddThinker(glow);
                break;

            default:
            {
                //I_Error("Error: P_UnarchiveSpecials:Unknown tclass %i "
                //        "in savegame", tclass);

                doom_strcpy(error_buf, "Error: P_UnarchiveSpecials:Unknown tclass ");
                doom_concat(error_buf, doom_itoa(tclass, 10));
                doom_concat(error_buf, " in savegame");
                I_Error(error_buf);
            }
        }
    }
}
} // namespace Doom
