// Rewritten out of vanilla p_ceilng into namespace Doom.
//
// Moving ceilings and crushers: the T_MoveCeiling thinker and the EV_ handlers over
// the global activeceilings list. T_MoveCeiling stays global (p_saveg identity; the
// spawner stores its address). p_ceilng.cpp shims every name and owns the
// activeceilings storage. Golden-neutral - the demos trip crushers.

#include "../doom_config.h"

#include "../doomdef.h"
#include "../doomstat.h"
#include "../p_local.h"
#include "../r_state.h"
#include "../s_sound.h"
#include "../sounds.h"
#include "../z_zone.h"

#include "Ceilings.h"
#include "Tick.h" // levelAlloc / levelFree / freeLevelAllocations

// The thinker functions stay global (p_saveg identity); declared so the spawners
// can store their address.
void T_MoveCeiling(ceiling_t* ceiling);

namespace Doom
{
// Forward declarations so the file's own call order needs no rearranging.
void moveCeiling(ceiling_t* ceiling);
int doCeiling(line_t* line, ceiling_e type);
void addActiveCeiling(ceiling_t* c);
void removeActiveCeiling(ceiling_t* c);
void activateInStasisCeiling(line_t* line);
int ceilingCrushStop(line_t* line);

void moveCeiling(ceiling_t* ceiling)
{
    result_e res;

    switch (ceiling->direction)
    {
        case 0:
            // IN STASIS
            break;
        case 1:
            // UP
            res = T_MovePlane(ceiling->sector,
                              ceiling->speed,
                              ceiling->topheight,
                              false,
                              1,
                              ceiling->direction);

            if (!(leveltime & 7))
            {
                switch (ceiling->type)
                {
                    case silentCrushAndRaise:
                        break;
                    default:
                        S_StartSound((mobj_t*) &ceiling->sector->soundorg,
                                     sfx_stnmov);
                        // ?
                        break;
                }
            }

            if (res == pastdest)
            {
                switch (ceiling->type)
                {
                    case raiseToHighest:
                        removeActiveCeiling(ceiling);
                        break;

                    case silentCrushAndRaise:
                        S_StartSound((mobj_t*) &ceiling->sector->soundorg,
                                     sfx_pstop);
                    case fastCrushAndRaise:
                    case crushAndRaise:
                        ceiling->direction = -1;
                        break;

                    default:
                        break;
                }
            }
            break;

        case -1:
            // DOWN
            res = T_MovePlane(ceiling->sector,
                              ceiling->speed,
                              ceiling->bottomheight,
                              ceiling->crush,
                              1,
                              ceiling->direction);

            if (!(leveltime & 7))
            {
                switch (ceiling->type)
                {
                    case silentCrushAndRaise:
                        break;
                    default:
                        S_StartSound((mobj_t*) &ceiling->sector->soundorg,
                                     sfx_stnmov);
                }
            }

            if (res == pastdest)
            {
                switch (ceiling->type)
                {
                    case silentCrushAndRaise:
                        S_StartSound((mobj_t*) &ceiling->sector->soundorg,
                                     sfx_pstop);
                    case crushAndRaise:
                        ceiling->speed = CEILSPEED;
                    case fastCrushAndRaise:
                        ceiling->direction = 1;
                        break;

                    case lowerAndCrush:
                    case lowerToFloor:
                        removeActiveCeiling(ceiling);
                        break;

                    default:
                        break;
                }
            }
            else // ( res != pastdest )
            {
                if (res == crushed)
                {
                    switch (ceiling->type)
                    {
                        case silentCrushAndRaise:
                        case crushAndRaise:
                        case lowerAndCrush:
                            ceiling->speed = CEILSPEED / 8;
                            break;

                        default:
                            break;
                    }
                }
            }
            break;
    }
}

//
// doCeiling
// Move a ceiling up/down and all around!
//
int doCeiling(line_t* line, ceiling_e type)
{
    int secnum;
    int rtn;
    sector_t* sec;
    ceiling_t* ceiling;

    secnum = -1;
    rtn = 0;

    // Reactivate in-stasis ceilings...for certain types.
    switch (type)
    {
        case fastCrushAndRaise:
        case silentCrushAndRaise:
        case crushAndRaise:
            activateInStasisCeiling(line);
        default:
            break;
    }

    while ((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
    {
        sec = &sectors[secnum];
        if (sec->specialdata)
            continue;

        // new door thinker
        rtn = 1;
        ceiling = (ceiling_t*) (levelAlloc(sizeof(*ceiling)));
        P_AddThinker(&ceiling->thinker);
        sec->specialdata = ceiling;
        ceiling->thinker.function.acp1 = (actionf_p1) T_MoveCeiling;
        ceiling->sector = sec;
        ceiling->crush = false;

        switch (type)
        {
            case fastCrushAndRaise:
                ceiling->crush = true;
                ceiling->topheight = sec->ceilingheight;
                ceiling->bottomheight = sec->floorheight + (8 * FRACUNIT);
                ceiling->direction = -1;
                ceiling->speed = CEILSPEED * 2;
                break;

            case silentCrushAndRaise:
            case crushAndRaise:
                ceiling->crush = true;
                ceiling->topheight = sec->ceilingheight;
            case lowerAndCrush:
            case lowerToFloor:
                ceiling->bottomheight = sec->floorheight;
                if (type != lowerToFloor)
                    ceiling->bottomheight += 8 * FRACUNIT;
                ceiling->direction = -1;
                ceiling->speed = CEILSPEED;
                break;

            case raiseToHighest:
                ceiling->topheight = P_FindHighestCeilingSurrounding(sec);
                ceiling->direction = 1;
                ceiling->speed = CEILSPEED;
                break;
        }

        ceiling->tag = sec->tag;
        ceiling->type = type;
        addActiveCeiling(ceiling);
    }

    return rtn;
}

//
// Add an active ceiling
//
void addActiveCeiling(ceiling_t* c)
{
    int i;

    for (i = 0; i < MAXCEILINGS; i++)
    {
        if (activeceilings[i] == 0)
        {
            activeceilings[i] = c;
            return;
        }
    }
}

//
// Remove a ceiling's thinker
//
void removeActiveCeiling(ceiling_t* c)
{
    int i;

    for (i = 0; i < MAXCEILINGS; i++)
    {
        if (activeceilings[i] == c)
        {
            activeceilings[i]->sector->specialdata = 0;
            P_RemoveThinker(&activeceilings[i]->thinker);
            activeceilings[i] = 0;
            break;
        }
    }
}

//
// Restart a ceiling that's in-stasis
//
void activateInStasisCeiling(line_t* line)
{
    int i;

    for (i = 0; i < MAXCEILINGS; i++)
    {
        if (activeceilings[i] && (activeceilings[i]->tag == line->tag)
            && (activeceilings[i]->direction == 0))
        {
            activeceilings[i]->direction = activeceilings[i]->olddirection;
            activeceilings[i]->thinker.function.acp1 = (actionf_p1) T_MoveCeiling;
        }
    }
}

//
// ceilingCrushStop
// Stop a ceiling from crushing!
//
int ceilingCrushStop(line_t* line)
{
    int i;
    int rtn;

    rtn = 0;
    for (i = 0; i < MAXCEILINGS; i++)
    {
        if (activeceilings[i] && (activeceilings[i]->tag == line->tag)
            && (activeceilings[i]->direction != 0))
        {
            activeceilings[i]->olddirection = activeceilings[i]->direction;
            activeceilings[i]->thinker.function.acv = (actionf_v) 0;
            activeceilings[i]->direction = 0; // in-stasis
            rtn = 1;
        }
    }

    return rtn;
}
} // namespace Doom
