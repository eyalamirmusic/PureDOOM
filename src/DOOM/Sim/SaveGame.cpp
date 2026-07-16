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

// save_p is defined in the shim; g_game and this file share it.
extern byte* save_p;

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
typedef enum
{
    tc_end,
    tc_mobj
} thinkerclass_t;

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
        if (th->function.acp1 == reinterpret_cast<actionf_p1>(P_MobjThinker))
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

        if (currentthinker->function.acp1
            == reinterpret_cast<actionf_p1>(P_MobjThinker))
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
                mobj = static_cast<mobj_t*>(levelAlloc(sizeof(*mobj)));
                doom_memcpy(mobj, save_p, sizeof(*mobj));
                save_p += sizeof(*mobj);
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
                mobj->thinker.function.acp1 =
                    reinterpret_cast<actionf_p1>(P_MobjThinker);
                P_AddThinker(&mobj->thinker);
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
    int i;

    // save off the current thinkers
    for (th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if (th->function.acv == (actionf_v) 0)
        {
            for (i = 0; i < MAXCEILINGS; i++)
                if (activeceilings[i] == reinterpret_cast<ceiling_t*>(th))
                    break;

            if (i < MAXCEILINGS)
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

        if (th->function.acp1 == reinterpret_cast<actionf_p1>(T_MoveCeiling))
        {
            *save_p++ = tc_ceiling;
            PADSAVEP();
            ceiling = reinterpret_cast<ceiling_t*>(save_p);
            doom_memcpy(ceiling, th, sizeof(*ceiling));
            save_p += sizeof(*ceiling);
            ceiling->sector = reinterpret_cast<sector_t*>(ceiling->sector - sectors);
            continue;
        }

        if (th->function.acp1 == reinterpret_cast<actionf_p1>(T_VerticalDoor))
        {
            *save_p++ = tc_door;
            PADSAVEP();
            door = reinterpret_cast<vldoor_t*>(save_p);
            doom_memcpy(door, th, sizeof(*door));
            save_p += sizeof(*door);
            door->sector = reinterpret_cast<sector_t*>(door->sector - sectors);
            continue;
        }

        if (th->function.acp1 == reinterpret_cast<actionf_p1>(T_MoveFloor))
        {
            *save_p++ = tc_floor;
            PADSAVEP();
            floor = reinterpret_cast<floormove_t*>(save_p);
            doom_memcpy(floor, th, sizeof(*floor));
            save_p += sizeof(*floor);
            floor->sector = reinterpret_cast<sector_t*>(floor->sector - sectors);
            continue;
        }

        if (th->function.acp1 == reinterpret_cast<actionf_p1>(T_PlatRaise))
        {
            *save_p++ = tc_plat;
            PADSAVEP();
            plat = reinterpret_cast<plat_t*>(save_p);
            doom_memcpy(plat, th, sizeof(*plat));
            save_p += sizeof(*plat);
            plat->sector = reinterpret_cast<sector_t*>(plat->sector - sectors);
            continue;
        }

        if (th->function.acp1 == reinterpret_cast<actionf_p1>(T_LightFlash))
        {
            *save_p++ = tc_flash;
            PADSAVEP();
            flash = reinterpret_cast<lightflash_t*>(save_p);
            doom_memcpy(flash, th, sizeof(*flash));
            save_p += sizeof(*flash);
            flash->sector = reinterpret_cast<sector_t*>(flash->sector - sectors);
            continue;
        }

        if (th->function.acp1 == reinterpret_cast<actionf_p1>(T_StrobeFlash))
        {
            *save_p++ = tc_strobe;
            PADSAVEP();
            strobe = reinterpret_cast<strobe_t*>(save_p);
            doom_memcpy(strobe, th, sizeof(*strobe));
            save_p += sizeof(*strobe);
            strobe->sector = reinterpret_cast<sector_t*>(strobe->sector - sectors);
            continue;
        }

        if (th->function.acp1 == reinterpret_cast<actionf_p1>(T_Glow))
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
                ceiling = static_cast<ceiling_t*>(levelAlloc(sizeof(*ceiling)));
                doom_memcpy(ceiling, save_p, sizeof(*ceiling));
                save_p += sizeof(*ceiling);
                ceiling->sector =
                    &sectors[reinterpret_cast<long long>(ceiling->sector)];
                ceiling->sector->specialdata = ceiling;

                if (ceiling->thinker.function.acp1)
                    ceiling->thinker.function.acp1 =
                        reinterpret_cast<actionf_p1>(T_MoveCeiling);

                P_AddThinker(&ceiling->thinker);
                P_AddActiveCeiling(ceiling);
                break;

            case tc_door:
                PADSAVEP();
                door = static_cast<vldoor_t*>(levelAlloc(sizeof(*door)));
                doom_memcpy(door, save_p, sizeof(*door));
                save_p += sizeof(*door);
                door->sector = &sectors[reinterpret_cast<long long>(door->sector)];
                door->sector->specialdata = door;
                door->thinker.function.acp1 =
                    reinterpret_cast<actionf_p1>(T_VerticalDoor);
                P_AddThinker(&door->thinker);
                break;

            case tc_floor:
                PADSAVEP();
                floor = static_cast<floormove_t*>(levelAlloc(sizeof(*floor)));
                doom_memcpy(floor, save_p, sizeof(*floor));
                save_p += sizeof(*floor);
                floor->sector = &sectors[reinterpret_cast<long long>(floor->sector)];
                floor->sector->specialdata = floor;
                floor->thinker.function.acp1 =
                    reinterpret_cast<actionf_p1>(T_MoveFloor);
                P_AddThinker(&floor->thinker);
                break;

            case tc_plat:
                PADSAVEP();
                plat = static_cast<plat_t*>(levelAlloc(sizeof(*plat)));
                doom_memcpy(plat, save_p, sizeof(*plat));
                save_p += sizeof(*plat);
                plat->sector = &sectors[reinterpret_cast<long long>(plat->sector)];
                plat->sector->specialdata = plat;

                if (plat->thinker.function.acp1)
                    plat->thinker.function.acp1 =
                        reinterpret_cast<actionf_p1>(T_PlatRaise);

                P_AddThinker(&plat->thinker);
                P_AddActivePlat(plat);
                break;

            case tc_flash:
                PADSAVEP();
                flash = static_cast<lightflash_t*>(levelAlloc(sizeof(*flash)));
                doom_memcpy(flash, save_p, sizeof(*flash));
                save_p += sizeof(*flash);
                flash->sector = &sectors[reinterpret_cast<long long>(flash->sector)];
                flash->thinker.function.acp1 =
                    reinterpret_cast<actionf_p1>(T_LightFlash);
                P_AddThinker(&flash->thinker);
                break;

            case tc_strobe:
                PADSAVEP();
                strobe = static_cast<strobe_t*>(levelAlloc(sizeof(*strobe)));
                doom_memcpy(strobe, save_p, sizeof(*strobe));
                save_p += sizeof(*strobe);
                strobe->sector =
                    &sectors[reinterpret_cast<long long>(strobe->sector)];
                strobe->thinker.function.acp1 =
                    reinterpret_cast<actionf_p1>(T_StrobeFlash);
                P_AddThinker(&strobe->thinker);
                break;

            case tc_glow:
                PADSAVEP();
                glow = static_cast<glow_t*>(levelAlloc(sizeof(*glow)));
                doom_memcpy(glow, save_p, sizeof(*glow));
                save_p += sizeof(*glow);
                glow->sector = &sectors[reinterpret_cast<long long>(glow->sector)];
                glow->thinker.function.acp1 = reinterpret_cast<actionf_p1>(T_Glow);
                P_AddThinker(&glow->thinker);
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
