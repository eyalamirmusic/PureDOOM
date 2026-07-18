// Rewritten out of vanilla p_plats into namespace Doom.
//
// Elevator platforms: the T_PlatRaise thinker and the EV_ handlers that spawn and
// stop them, over the global activeplats list. T_PlatRaise stays global (p_saveg
// identity; the spawners store its address). p_plats.cpp shims every name and owns
// the activeplats storage. Golden-neutral - the demos ride lifts.

#include "../doom_config.h"

#include "../doomdef.h"
#include "../doomstat.h"
#include "../i_system.h"
#include "../m_random.h"
#include "../p_local.h"
#include "../r_state.h"
#include "../s_sound.h"
#include "../sounds.h"

#include "Plats.h"
#include "Tick.h" // levelAlloc / levelFree / freeLevelAllocations
#include "Specials.h"

#include <new>

// The thinker functions stay global (p_saveg identity); declared so the spawners
// can store their address.
void T_PlatRaise(plat_t* plat);

namespace Doom
{
// Forward declarations so the file's own call order needs no rearranging.
void platRaise(plat_t& plat);
int doPlat(line_t* line, plattype_e type, int amount);
void activateInStasis(int tag);
void stopPlat(line_t* line);
void addActivePlat(plat_t* plat);
void removeActivePlat(plat_t* plat);

void platRaise(plat_t& plat)
{
    result_e res;

    switch (plat.status)
    {
        case up:
            res = T_MovePlane(
                plat.sector, plat.speed, plat.high, plat.crush, 0, 1);

            if (plat.type == raiseAndChange
                || plat.type == raiseToNearestAndChange)
            {
                if (!(leveltime & 7))
                    S_StartSound(reinterpret_cast<mobj_t*>(&plat.sector->soundorg),
                                 sfx_stnmov);
            }

            if (res == crushed && (!plat.crush))
            {
                plat.count = plat.wait;
                plat.status = down;
                S_StartSound(reinterpret_cast<mobj_t*>(&plat.sector->soundorg),
                             sfx_pstart);
            }
            else
            {
                if (res == pastdest)
                {
                    plat.count = plat.wait;
                    plat.status = waiting;
                    S_StartSound(reinterpret_cast<mobj_t*>(&plat.sector->soundorg),
                                 sfx_pstop);

                    switch (plat.type)
                    {
                        case blazeDWUS:
                        case downWaitUpStay:
                            removeActivePlat(&plat);
                            break;

                        case raiseAndChange:
                        case raiseToNearestAndChange:
                            removeActivePlat(&plat);
                            break;

                        default:
                            break;
                    }
                }
            }
            break;

        case down:
            res = T_MovePlane(plat.sector, plat.speed, plat.low, false, 0, -1);

            if (res == pastdest)
            {
                plat.count = plat.wait;
                plat.status = waiting;
                S_StartSound(reinterpret_cast<mobj_t*>(&plat.sector->soundorg),
                             sfx_pstop);
            }
            break;

        case waiting:
            if (!--plat.count)
            {
                if (plat.sector->floorheight == plat.low)
                    plat.status = up;
                else
                    plat.status = down;
                S_StartSound(reinterpret_cast<mobj_t*>(&plat.sector->soundorg),
                             sfx_pstart);
            }

        case in_stasis:
            break;
    }
}

//
// Do Platforms
//  "amount" is only used for SOME platforms.
//
int doPlat(line_t* line, plattype_e type, int amount)
{
    plat_t* plat;
    int secnum;
    int rtn;
    sector_t* sec;

    secnum = -1;
    rtn = 0;

    // Activate all <type> plats that are in_stasis
    switch (type)
    {
        case perpetualRaise:
            activateInStasis(line->tag);
            break;
        default:
            break;
    }

    while ((secnum = Doom::findSectorFromLineTag(line, secnum)) >= 0)
    {
        sec = &sectors[secnum];

        if (sec->specialdata)
            continue;

        // Find lowest & highest floors around sector
        rtn = 1;
        plat = new (levelAlloc(sizeof(*plat))) plat_t {};
        Doom::addThinker(plat);

        plat->type = type;
        plat->sector = sec;
        plat->sector->specialdata = plat;
        plat->crush = false;
        plat->tag = line->tag;

        switch (type)
        {
            case raiseToNearestAndChange:
                plat->speed = PLATSPEED / 2;
                sec->floorpic = sides[line->sidenum[0]].sector->floorpic;
                plat->high = Doom::findNextHighestFloor(sec, sec->floorheight);
                plat->wait = 0;
                plat->status = up;
                // NO MORE DAMAGE, IF APPLICABLE
                sec->special = 0;

                S_StartSound(reinterpret_cast<mobj_t*>(&sec->soundorg), sfx_stnmov);
                break;

            case raiseAndChange:
                plat->speed = PLATSPEED / 2;
                sec->floorpic = sides[line->sidenum[0]].sector->floorpic;
                plat->high = sec->floorheight + amount * FRACUNIT;
                plat->wait = 0;
                plat->status = up;

                S_StartSound(reinterpret_cast<mobj_t*>(&sec->soundorg), sfx_stnmov);
                break;

            case downWaitUpStay:
                plat->speed = PLATSPEED * 4;
                plat->low = Doom::findLowestFloorSurrounding(sec);

                if (plat->low > sec->floorheight)
                    plat->low = sec->floorheight;

                plat->high = sec->floorheight;
                plat->wait = 35 * PLATWAIT;
                plat->status = down;
                S_StartSound(reinterpret_cast<mobj_t*>(&sec->soundorg), sfx_pstart);
                break;

            case blazeDWUS:
                plat->speed = PLATSPEED * 8;
                plat->low = Doom::findLowestFloorSurrounding(sec);

                if (plat->low > sec->floorheight)
                    plat->low = sec->floorheight;

                plat->high = sec->floorheight;
                plat->wait = 35 * PLATWAIT;
                plat->status = down;
                S_StartSound(reinterpret_cast<mobj_t*>(&sec->soundorg), sfx_pstart);
                break;

            case perpetualRaise:
                plat->speed = PLATSPEED;
                plat->low = Doom::findLowestFloorSurrounding(sec);

                if (plat->low > sec->floorheight)
                    plat->low = sec->floorheight;

                plat->high = Doom::findHighestFloorSurrounding(sec);

                if (plat->high < sec->floorheight)
                    plat->high = sec->floorheight;

                plat->wait = 35 * PLATWAIT;
                plat->status = (plat_e) (P_Random() & 1);

                S_StartSound(reinterpret_cast<mobj_t*>(&sec->soundorg), sfx_pstart);
                break;
        }
        addActivePlat(plat);
    }

    return rtn;
}

void activateInStasis(int tag)
{
    for (int i = 0; i < MAXPLATS; i++)
        if (activeplats[i] && (activeplats[i])->tag == tag
            && (activeplats[i])->status == in_stasis)
        {
            (activeplats[i])->status = (activeplats[i])->oldstatus;
            (activeplats[i])->stopped = false;
        }
}

void stopPlat(line_t* line)
{
    for (int j = 0; j < MAXPLATS; j++)
        if (activeplats[j] && ((activeplats[j])->status != in_stasis)
            && ((activeplats[j])->tag == line->tag))
        {
            (activeplats[j])->oldstatus = (activeplats[j])->status;
            (activeplats[j])->status = in_stasis;
            (activeplats[j])->stopped = true;
        }
}

void addActivePlat(plat_t* plat)
{
    for (int i = 0; i < MAXPLATS; i++)
        if (activeplats[i] == nullptr)
        {
            activeplats[i] = plat;
            return;
        }
    I_Error("Error: addActivePlat: no more plats!");
}

void removeActivePlat(plat_t* plat)
{
    for (int i = 0; i < MAXPLATS; i++)
        if (plat == activeplats[i])
        {
            (activeplats[i])->sector->specialdata = nullptr;
            Doom::removeThinker(activeplats[i]);
            activeplats[i] = nullptr;

            return;
        }
    I_Error("Error: removeActivePlat: can't find plat!");
}
} // namespace Doom
