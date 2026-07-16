// Rewritten out of vanilla p_spec into namespace Doom.
//
// The specials coordinator: animated flats/textures, the surrounding-sector height
// and light queries the movers use, line-special cross/shoot dispatch, per-sector
// player damage/secret, the once-a-tic P_UpdateSpecials and level-spawn setup.
// getSide/getSector/twoSided/getNextSector stay at global scope (p_spec.h API);
// every P_/EV_ entry is shimmed by p_spec.cpp; the animation state is file-local and
// levelTimer stays global. Golden-neutral - the demos scroll skies, damage in slime
// and open exits.

#include "../doom_config.h"

#include "../doomdef.h"
#include "../doomstat.h"
#include "../g_game.h"
#include "../i_system.h"
#include "../m_argv.h"
#include "../m_random.h"
#include "../p_local.h"
#include "../r_local.h"
#include "../r_state.h"
#include "../s_sound.h"
#include "../sounds.h"
#include "../w_wad.h"

#include "AnimatedSurfaces.h"
#include "Specials.h"
#include "Tick.h" // levelAlloc / levelFree / freeLevelAllocations

#define MAXANIMS 32
#define MAXLINEANIMS 64
#define MAX_ADJOINING_SECTORS 20

// (The vanilla file-scope `anim_t` typedef that sat here was dead - unused at global scope, a
// leftover of the namespace wrap - and was removed; anim_t now lives in Sim/AnimatedSurfaces.h.)

side_t* getSide(int currentSector, int line, int side)
{
    return &sides[(sectors[currentSector].lines[line])->sidenum[side]];
}

//
// getSector()
// Will return a sector_t*
//  given the number of the current sector,
//  the line number and the side (0/1) that you want.
//
sector_t* getSector(int currentSector, int line, int side)
{
    return sides[(sectors[currentSector].lines[line])->sidenum[side]].sector;
}

//
// twoSided()
// Given the sector number and the line number,
//  it will tell you whether the line is two-sided or not.
//
int twoSided(int sector, int line)
{
    return (sectors[sector].lines[line])->flags & ML_TWOSIDED;
}

//
// getNextSector()
// Return sector_t * of sector next to current.
// 0 if not two-sided line
//
sector_t* getNextSector(line_t* line, sector_t* sec)
{
    if (!(line->flags & ML_TWOSIDED))
        return nullptr;

    if (line->frontsector == sec)
        return line->backsector;

    return line->frontsector;
}

//
// P_FindLowestFloorSurrounding()
// FIND LOWEST FLOOR HEIGHT IN SURROUNDING SECTORS
//

namespace Doom
{
// anim_t moved to Sim/AnimatedSurfaces.h with the anims/lastanim it types.

struct animdef_t
{
    // Not a boolean, despite the name and despite what vanilla called it: the
    // table below ends with {-1}, and P_InitPicAnims walks until it finds that.
    // Compiled as C this was an int-sized enum and -1 fitted. As a C++ bool the
    // terminator would quietly become `true`, never be recognised, and the loop
    // would run off the end of the array.
    int istexture; // 0 a flat, 1 a texture, -1 ends the table
    char endname[9];
    char startname[9];
    int speed;
};

animdef_t animdefs[] = {{false, "NUKAGE3", "NUKAGE1", 8},
                        {false, "FWATER4", "FWATER1", 8},
                        {false, "SWATER4", "SWATER1", 8},
                        {false, "LAVA4", "LAVA1", 8},
                        {false, "BLOOD3", "BLOOD1", 8},

                        // DOOM II flat animations.
                        {false, "RROCK08", "RROCK05", 8},
                        {false, "SLIME04", "SLIME01", 8},
                        {false, "SLIME08", "SLIME05", 8},
                        {false, "SLIME12", "SLIME09", 8},

                        {true, "BLODGR4", "BLODGR1", 8},
                        {true, "SLADRIP3", "SLADRIP1", 8},

                        {true, "BLODRIP4", "BLODRIP1", 8},
                        {true, "FIREWALL", "FIREWALA", 8},
                        {true, "GSTFONT3", "GSTFONT1", 8},
                        {true, "FIRELAVA", "FIRELAV3", 8},
                        {true, "FIREMAG3", "FIREMAG1", 8},
                        {true, "FIREBLU2", "FIREBLU1", 8},
                        {true, "ROCKRED3", "ROCKRED1", 8},

                        {true, "BFALL4", "BFALL1", 8},
                        {true, "SFALL4", "SFALL1", 8},
                        {true, "WFALL4", "WFALL1", 8},
                        {true, "DBRAIN4", "DBRAIN1", 8},

                        {-1, "", "", 0}};

// The animated flats/textures and the scrolling-line list now live on the Engine
// (Sim/AnimatedSurfaces.h, moved by the file-scope-statics sweep - REFACTOR.md, Step 5). The vanilla
// names are references onto that member; read by no other file.
static anim_t (&anims)[MAXANIMS] = animatedSurfaces().anims;
static anim_t*& lastanim = animatedSurfaces().lastanim;

static short& numlinespecials = animatedSurfaces().numlinespecials;
static line_t* (&linespeciallist)[MAXLINEANIMS] = animatedSurfaces().linespeciallist;

// Forward declarations so call order needs no rearranging.
void initPicAnims();
fixed_t findLowestFloorSurrounding(sector_t* sec);
fixed_t findHighestFloorSurrounding(sector_t* sec);
fixed_t findNextHighestFloor(sector_t* sec, int currentheight);
fixed_t findLowestCeilingSurrounding(sector_t* sec);
fixed_t findHighestCeilingSurrounding(sector_t* sec);
int findSectorFromLineTag(line_t* line, int start);
int findMinSurroundingLight(sector_t* sector, int max);
void crossSpecialLine(int linenum, int side, mobj_t* thing);
void shootSpecialLine(mobj_t* thing, line_t* line);
void playerInSpecialSector(player_t* player);
void updateSpecials();
int doDonut(line_t* line);
void spawnSpecials();

void initPicAnims()
{
    // Init animation
    lastanim = anims;
    for (int i = 0; animdefs[i].istexture != -1; i++)
    {
        if (animdefs[i].istexture)
        {
            // different episode ?
            if (R_CheckTextureNumForName(animdefs[i].startname) == -1)
                continue;

            lastanim->picnum = R_TextureNumForName(animdefs[i].endname);
            lastanim->basepic = R_TextureNumForName(animdefs[i].startname);
        }
        else
        {
            if (W_CheckNumForName(animdefs[i].startname) == -1)
                continue;

            lastanim->picnum = R_FlatNumForName(animdefs[i].endname);
            lastanim->basepic = R_FlatNumForName(animdefs[i].startname);
        }

        lastanim->istexture = animdefs[i].istexture;
        lastanim->numpics = lastanim->picnum - lastanim->basepic + 1;

        if (lastanim->numpics < 2)
        {
            //I_Error("Error: initPicAnims: bad cycle from %s to %s",
            //        animdefs[i].startname,
            //        animdefs[i].endname);

            doom_strcpy(error_buf, "Error: initPicAnims: bad cycle from ");
            doom_concat(error_buf, animdefs[i].startname);
            doom_concat(error_buf, " to ");
            doom_concat(error_buf, animdefs[i].endname);
            I_Error(error_buf);
        }

        lastanim->speed = animdefs[i].speed;
        lastanim++;
    }
}

//
// UTILITIES
//

//
// getSide()
// Will return a side_t*
//  given the number of the current sector,
//  the line number, and the side (0/1) that you want.
//
fixed_t findLowestFloorSurrounding(sector_t* sec)
{
    line_t* check;
    sector_t* other;
    fixed_t floor = sec->floorheight;

    for (int i = 0; i < sec->linecount; i++)
    {
        check = sec->lines[i];
        other = getNextSector(check, sec);

        if (!other)
            continue;

        if (other->floorheight < floor)
            floor = other->floorheight;
    }

    return floor;
}

//
// findHighestFloorSurrounding()
// FIND HIGHEST FLOOR HEIGHT IN SURROUNDING SECTORS
//
fixed_t findHighestFloorSurrounding(sector_t* sec)
{
    line_t* check;
    sector_t* other;
    fixed_t floor = -500 * FRACUNIT;

    for (int i = 0; i < sec->linecount; i++)
    {
        check = sec->lines[i];
        other = getNextSector(check, sec);

        if (!other)
            continue;

        if (other->floorheight > floor)
            floor = other->floorheight;
    }

    return floor;
}

//
// findNextHighestFloor
// FIND NEXT HIGHEST FLOOR IN SURROUNDING SECTORS
// Note: this should be doable w/o a fixed array.
fixed_t findNextHighestFloor(sector_t* sec, int currentheight)
{
    int i;
    int h;
    int min;
    line_t* check;
    sector_t* other;
    fixed_t height = currentheight;

    fixed_t heightlist[MAX_ADJOINING_SECTORS];

    for (i = 0, h = 0; i < sec->linecount; i++)
    {
        check = sec->lines[i];
        other = getNextSector(check, sec);

        if (!other)
            continue;

        if (other->floorheight > height)
            heightlist[h++] = other->floorheight;

        // Check for overflow. Exit.
        if (h >= MAX_ADJOINING_SECTORS)
        {
            doom_print("Sector with more than 20 adjoining sectors\n");
            break;
        }
    }

    // Find lowest height in list
    if (!h)
        return currentheight;

    min = heightlist[0];

    // Range checking?
    for (i = 1; i < h; i++)
        if (heightlist[i] < min)
            min = heightlist[i];

    return min;
}

//
// FIND LOWEST CEILING IN THE SURROUNDING SECTORS
//
fixed_t findLowestCeilingSurrounding(sector_t* sec)
{
    line_t* check;
    sector_t* other;
    fixed_t height = DOOM_MAXINT;

    for (int i = 0; i < sec->linecount; i++)
    {
        check = sec->lines[i];
        other = getNextSector(check, sec);

        if (!other)
            continue;

        if (other->ceilingheight < height)
            height = other->ceilingheight;
    }

    return height;
}

//
// FIND HIGHEST CEILING IN THE SURROUNDING SECTORS
//
fixed_t findHighestCeilingSurrounding(sector_t* sec)
{
    line_t* check;
    sector_t* other;
    fixed_t height = 0;

    for (int i = 0; i < sec->linecount; i++)
    {
        check = sec->lines[i];
        other = getNextSector(check, sec);

        if (!other)
            continue;

        if (other->ceilingheight > height)
            height = other->ceilingheight;
    }

    return height;
}

//
// RETURN NEXT SECTOR # THAT LINE TAG REFERS TO
//
int findSectorFromLineTag(line_t* line, int start)
{
    for (int i = start + 1; i < numsectors; i++)
        if (sectors[i].tag == line->tag)
            return i;

    return -1;
}

//
// Find minimum light from an adjacent sector
//
int findMinSurroundingLight(sector_t* sector, int max)
{
    int min;
    line_t* line;
    sector_t* check;

    min = max;
    for (int i = 0; i < sector->linecount; i++)
    {
        line = sector->lines[i];
        check = getNextSector(line, sector);

        if (!check)
            continue;

        if (check->lightlevel < min)
            min = check->lightlevel;
    }

    return min;
}

//
// EVENTS
// Events are operations triggered by using, crossing,
// or shooting special lines, or by timed thinkers.
//

//
// crossSpecialLine - TRIGGER
// Called every time a thing origin is about
//  to cross a line with a non 0 special.
//
void crossSpecialLine(int linenum, int side, mobj_t* thing)
{
    line_t* line;
    int ok;

    line = &lines[linenum];

    //        Triggers that other things can activate
    if (!thing->player)
    {
        // Things that should NOT trigger specials...
        switch (thing->type)
        {
            case MT_ROCKET:
            case MT_PLASMA:
            case MT_BFG:
            case MT_TROOPSHOT:
            case MT_HEADSHOT:
            case MT_BRUISERSHOT:
                return;
                break;

            default:
                break;
        }

        ok = 0;
        switch (line->special)
        {
            case 39: // TELEPORT TRIGGER
            case 97: // TELEPORT RETRIGGER
            case 125: // TELEPORT MONSTERONLY TRIGGER
            case 126: // TELEPORT MONSTERONLY RETRIGGER
            case 4: // RAISE DOOR
            case 10: // PLAT DOWN-WAIT-UP-STAY TRIGGER
            case 88: // PLAT DOWN-WAIT-UP-STAY RETRIGGER
                ok = 1;
                break;
        }
        if (!ok)
            return;
    }

    // Note: could use some const's here.
    switch (line->special)
    {
        // TRIGGERS.
        // All from here to RETRIGGERS.
        case 2:
            // Open Door
            EV_DoDoor(line, door_open);
            line->special = 0;
            break;

        case 3:
            // Close Door
            EV_DoDoor(line, door_close);
            line->special = 0;
            break;

        case 4:
            // Raise Door
            EV_DoDoor(line, door_normal);
            line->special = 0;
            break;

        case 5:
            // Raise Floor
            EV_DoFloor(line, raiseFloor);
            line->special = 0;
            break;

        case 6:
            // Fast Ceiling Crush & Raise
            EV_DoCeiling(line, fastCrushAndRaise);
            line->special = 0;
            break;

        case 8:
            // Build Stairs
            EV_BuildStairs(line, build8);
            line->special = 0;
            break;

        case 10:
            // PlatDownWaitUp
            EV_DoPlat(line, downWaitUpStay, 0);
            line->special = 0;
            break;

        case 12:
            // Light Turn On - brightest near
            EV_LightTurnOn(line, 0);
            line->special = 0;
            break;

        case 13:
            // Light Turn On 255
            EV_LightTurnOn(line, 255);
            line->special = 0;
            break;

        case 16:
            // Close Door 30
            EV_DoDoor(line, close30ThenOpen);
            line->special = 0;
            break;

        case 17:
            // Start Light Strobing
            EV_StartLightStrobing(line);
            line->special = 0;
            break;

        case 19:
            // Lower Floor
            EV_DoFloor(line, lowerFloor);
            line->special = 0;
            break;

        case 22:
            // Raise floor to nearest height and change texture
            EV_DoPlat(line, raiseToNearestAndChange, 0);
            line->special = 0;
            break;

        case 25:
            // Ceiling Crush and Raise
            EV_DoCeiling(line, crushAndRaise);
            line->special = 0;
            break;

        case 30:
            // Raise floor to shortest texture height
            //  on either side of lines.
            EV_DoFloor(line, raiseToTexture);
            line->special = 0;
            break;

        case 35:
            // Lights Very Dark
            EV_LightTurnOn(line, 35);
            line->special = 0;
            break;

        case 36:
            // Lower Floor (TURBO)
            EV_DoFloor(line, turboLower);
            line->special = 0;
            break;

        case 37:
            // LowerAndChange
            EV_DoFloor(line, lowerAndChange);
            line->special = 0;
            break;

        case 38:
            // Lower Floor To Lowest
            EV_DoFloor(line, lowerFloorToLowest);
            line->special = 0;
            break;

        case 39:
            // TELEPORT!
            EV_Teleport(line, side, thing);
            line->special = 0;
            break;

        case 40:
            // RaiseCeilingLowerFloor
            EV_DoCeiling(line, raiseToHighest);
            EV_DoFloor(line, lowerFloorToLowest);
            line->special = 0;
            break;

        case 44:
            // Ceiling Crush
            EV_DoCeiling(line, lowerAndCrush);
            line->special = 0;
            break;

        case 52:
            // EXIT!
            G_ExitLevel();
            break;

        case 53:
            // Perpetual Platform Raise
            EV_DoPlat(line, perpetualRaise, 0);
            line->special = 0;
            break;

        case 54:
            // Platform Stop
            EV_StopPlat(line);
            line->special = 0;
            break;

        case 56:
            // Raise Floor Crush
            EV_DoFloor(line, raiseFloorCrush);
            line->special = 0;
            break;

        case 57:
            // Ceiling Crush Stop
            EV_CeilingCrushStop(line);
            line->special = 0;
            break;

        case 58:
            // Raise Floor 24
            EV_DoFloor(line, raiseFloor24);
            line->special = 0;
            break;

        case 59:
            // Raise Floor 24 And Change
            EV_DoFloor(line, raiseFloor24AndChange);
            line->special = 0;
            break;

        case 104:
            // Turn lights off in sector(tag)
            EV_TurnTagLightsOff(line);
            line->special = 0;
            break;

        case 108:
            // Blazing Door Raise (faster than TURBO!)
            EV_DoDoor(line, blazeRaise);
            line->special = 0;
            break;

        case 109:
            // Blazing Door Open (faster than TURBO!)
            EV_DoDoor(line, blazeOpen);
            line->special = 0;
            break;

        case 100:
            // Build Stairs Turbo 16
            EV_BuildStairs(line, turbo16);
            line->special = 0;
            break;

        case 110:
            // Blazing Door Close (faster than TURBO!)
            EV_DoDoor(line, blazeClose);
            line->special = 0;
            break;

        case 119:
            // Raise floor to nearest surr. floor
            EV_DoFloor(line, raiseFloorToNearest);
            line->special = 0;
            break;

        case 121:
            // Blazing PlatDownWaitUpStay
            EV_DoPlat(line, blazeDWUS, 0);
            line->special = 0;
            break;

        case 124:
            // Secret EXIT
            G_SecretExitLevel();
            break;

        case 125:
            // TELEPORT MonsterONLY
            if (!thing->player)
            {
                EV_Teleport(line, side, thing);
                line->special = 0;
            }
            break;

        case 130:
            // Raise Floor Turbo
            EV_DoFloor(line, raiseFloorTurbo);
            line->special = 0;
            break;

        case 141:
            // Silent Ceiling Crush & Raise
            EV_DoCeiling(line, silentCrushAndRaise);
            line->special = 0;
            break;

            // RETRIGGERS.  All from here till end.
        case 72:
            // Ceiling Crush
            EV_DoCeiling(line, lowerAndCrush);
            break;

        case 73:
            // Ceiling Crush and Raise
            EV_DoCeiling(line, crushAndRaise);
            break;

        case 74:
            // Ceiling Crush Stop
            EV_CeilingCrushStop(line);
            break;

        case 75:
            // Close Door
            EV_DoDoor(line, door_close);
            break;

        case 76:
            // Close Door 30
            EV_DoDoor(line, close30ThenOpen);
            break;

        case 77:
            // Fast Ceiling Crush & Raise
            EV_DoCeiling(line, fastCrushAndRaise);
            break;

        case 79:
            // Lights Very Dark
            EV_LightTurnOn(line, 35);
            break;

        case 80:
            // Light Turn On - brightest near
            EV_LightTurnOn(line, 0);
            break;

        case 81:
            // Light Turn On 255
            EV_LightTurnOn(line, 255);
            break;

        case 82:
            // Lower Floor To Lowest
            EV_DoFloor(line, lowerFloorToLowest);
            break;

        case 83:
            // Lower Floor
            EV_DoFloor(line, lowerFloor);
            break;

        case 84:
            // LowerAndChange
            EV_DoFloor(line, lowerAndChange);
            break;

        case 86:
            // Open Door
            EV_DoDoor(line, door_open);
            break;

        case 87:
            // Perpetual Platform Raise
            EV_DoPlat(line, perpetualRaise, 0);
            break;

        case 88:
            // PlatDownWaitUp
            EV_DoPlat(line, downWaitUpStay, 0);
            break;

        case 89:
            // Platform Stop
            EV_StopPlat(line);
            break;

        case 90:
            // Raise Door
            EV_DoDoor(line, door_normal);
            break;

        case 91:
            // Raise Floor
            EV_DoFloor(line, raiseFloor);
            break;

        case 92:
            // Raise Floor 24
            EV_DoFloor(line, raiseFloor24);
            break;

        case 93:
            // Raise Floor 24 And Change
            EV_DoFloor(line, raiseFloor24AndChange);
            break;

        case 94:
            // Raise Floor Crush
            EV_DoFloor(line, raiseFloorCrush);
            break;

        case 95:
            // Raise floor to nearest height
            // and change texture.
            EV_DoPlat(line, raiseToNearestAndChange, 0);
            break;

        case 96:
            // Raise floor to shortest texture height
            // on either side of lines.
            EV_DoFloor(line, raiseToTexture);
            break;

        case 97:
            // TELEPORT!
            EV_Teleport(line, side, thing);
            break;

        case 98:
            // Lower Floor (TURBO)
            EV_DoFloor(line, turboLower);
            break;

        case 105:
            // Blazing Door Raise (faster than TURBO!)
            EV_DoDoor(line, blazeRaise);
            break;

        case 106:
            // Blazing Door Open (faster than TURBO!)
            EV_DoDoor(line, blazeOpen);
            break;

        case 107:
            // Blazing Door Close (faster than TURBO!)
            EV_DoDoor(line, blazeClose);
            break;

        case 120:
            // Blazing PlatDownWaitUpStay.
            EV_DoPlat(line, blazeDWUS, 0);
            break;

        case 126:
            // TELEPORT MonsterONLY.
            if (!thing->player)
                EV_Teleport(line, side, thing);
            break;

        case 128:
            // Raise To Nearest Floor
            EV_DoFloor(line, raiseFloorToNearest);
            break;

        case 129:
            // Raise Floor Turbo
            EV_DoFloor(line, raiseFloorTurbo);
            break;
    }
}

//
// shootSpecialLine - IMPACT SPECIALS
// Called when a thing shoots a special line.
//
void shootSpecialLine(mobj_t* thing, line_t* line)
{
    int ok;

    // Impacts that other things can activate.
    if (!thing->player)
    {
        ok = 0;
        switch (line->special)
        {
            case 46:
                // OPEN DOOR IMPACT
                ok = 1;
                break;
        }
        if (!ok)
            return;
    }

    switch (line->special)
    {
        case 24:
            // RAISE FLOOR
            EV_DoFloor(line, raiseFloor);
            P_ChangeSwitchTexture(line, 0);
            break;

        case 46:
            // OPEN DOOR
            EV_DoDoor(line, door_open);
            P_ChangeSwitchTexture(line, 1);
            break;

        case 47:
            // RAISE FLOOR NEAR AND CHANGE
            EV_DoPlat(line, raiseToNearestAndChange, 0);
            P_ChangeSwitchTexture(line, 0);
            break;
    }
}

//
// playerInSpecialSector
// Called every tic frame
//  that the player origin is in a special sector
//
void playerInSpecialSector(player_t* player)
{
    sector_t* sector;

    sector = player->mo->subsector->sector;

    // Falling, not all the way down yet?
    if (player->mo->z != sector->floorheight)
        return;

    // Has hitten ground.
    switch (sector->special)
    {
        case 5:
            // HELLSLIME DAMAGE
            if (!player->powers[pw_ironfeet])
                if (!(leveltime & 0x1f))
                    P_DamageMobj(player->mo, 0, 0, 10);
            break;

        case 7:
            // NUKAGE DAMAGE
            if (!player->powers[pw_ironfeet])
                if (!(leveltime & 0x1f))
                    P_DamageMobj(player->mo, 0, 0, 5);
            break;

        case 16:
            // SUPER HELLSLIME DAMAGE
        case 4:
            // STROBE HURT
            if (!player->powers[pw_ironfeet] || (P_Random() < 5))
            {
                if (!(leveltime & 0x1f))
                    P_DamageMobj(player->mo, 0, 0, 20);
            }
            break;

        case 9:
            // SECRET SECTOR
            player->secretcount++;
            player->message = "A secret is revealed!";
            S_StartSound(nullptr, sfx_getpow);
            sector->special = 0;
            break;

        case 11:
            // EXIT SUPER DAMAGE! (for E1M8 finale)
            player->cheats &= ~CF_GODMODE;

            if (!(leveltime & 0x1f))
                P_DamageMobj(player->mo, 0, 0, 20);

            if (player->health <= 10)
                G_ExitLevel();
            break;

        default:
        {
            //I_Error("Error: playerInSpecialSector: "
            //        "unknown special %i",
            //        sector->special);

            doom_strcpy(error_buf, "Error: playerInSpecialSector: unknown special ");
            doom_concat(error_buf, doom_itoa(sector->special, 10));
            I_Error(error_buf);
            break;
        }
    };
}

//
// updateSpecials
// Animate planes, scroll walls, etc.
//
void updateSpecials()
{
    anim_t* anim;
    int pic;
    line_t* line;

    // LEVEL TIMER
    if (levelTimer == true)
    {
        levelTimeCount--;
        if (!levelTimeCount)
            G_ExitLevel();
    }

    // ANIMATE FLATS AND TEXTURES GLOBALLY
    for (anim = anims; anim < lastanim; anim++)
    {
        for (int i = anim->basepic; i < anim->basepic + anim->numpics; i++)
        {
            pic = anim->basepic + ((leveltime / anim->speed + i) % anim->numpics);
            if (anim->istexture)
                texturetranslation[i] = pic;
            else
                flattranslation[i] = pic;
        }
    }

    // ANIMATE LINE SPECIALS
    for (int i = 0; i < numlinespecials; i++)
    {
        line = linespeciallist[i];
        switch (line->special)
        {
            case 48:
                // EFFECT FIRSTCOL SCROLL +
                sides[line->sidenum[0]].textureoffset += FRACUNIT;
                break;
        }
    }

    // DO BUTTONS
    for (int i = 0; i < MAXBUTTONS; i++)
        if (buttonlist[i].btimer)
        {
            buttonlist[i].btimer--;
            if (!buttonlist[i].btimer)
            {
                switch (buttonlist[i].where)
                {
                    case top:
                        sides[buttonlist[i].line->sidenum[0]].toptexture =
                            buttonlist[i].btexture;
                        break;

                    case middle:
                        sides[buttonlist[i].line->sidenum[0]].midtexture =
                            buttonlist[i].btexture;
                        break;

                    case bottom:
                        sides[buttonlist[i].line->sidenum[0]].bottomtexture =
                            buttonlist[i].btexture;
                        break;
                }
                S_StartSound(reinterpret_cast<mobj_t*>(&buttonlist[i].soundorg),
                             sfx_swtchn);
                doom_memset(&buttonlist[i], 0, sizeof(button_t));
            }
        }
}

//
// Special Stuff that can not be categorized
//
int doDonut(line_t* line)
{
    sector_t* s1;
    sector_t* s2;
    sector_t* s3;
    int secnum;
    int rtn;
    floormove_t* floor;

    secnum = -1;
    rtn = 0;
    while ((secnum = findSectorFromLineTag(line, secnum)) >= 0)
    {
        s1 = &sectors[secnum];

        // ALREADY MOVING?  IF SO, KEEP GOING...
        if (s1->specialdata)
            continue;

        rtn = 1;
        s2 = getNextSector(s1->lines[0], s1);
        for (int i = 0; i < s2->linecount; i++)
        {
            if ((!(s2->lines[i]->flags & ML_TWOSIDED))
                || (s2->lines[i]->backsector == s1))
                continue;
            s3 = s2->lines[i]->backsector;

            //        Spawn rising slime
            floor = static_cast<floormove_t*>(levelAlloc(sizeof(*floor)));
            P_AddThinker(&floor->thinker);
            s2->specialdata = floor;
            floor->thinker.function.acp1 = reinterpret_cast<actionf_p1>(T_MoveFloor);
            floor->type = donutRaise;
            floor->crush = false;
            floor->direction = 1;
            floor->sector = s2;
            floor->speed = FLOORSPEED / 2;
            floor->texture = s3->floorpic;
            floor->newspecial = 0;
            floor->floordestheight = s3->floorheight;

            //        Spawn lowering donut-hole
            floor = static_cast<floormove_t*>(levelAlloc(sizeof(*floor)));
            P_AddThinker(&floor->thinker);
            s1->specialdata = floor;
            floor->thinker.function.acp1 = reinterpret_cast<actionf_p1>(T_MoveFloor);
            floor->type = lowerFloor;
            floor->crush = false;
            floor->direction = -1;
            floor->sector = s1;
            floor->speed = FLOORSPEED / 2;
            floor->floordestheight = s3->floorheight;
            break;
        }
    }

    return rtn;
}

//
// SPECIAL SPAWNING
//

//
// spawnSpecials
// After the map has been loaded, scan for specials
//  that spawn thinkers
//

// Parses command line parameters.
void spawnSpecials()
{
    sector_t* sector;
    int i;

    // See if -TIMER needs to be used.
    levelTimer = false;

    i = M_CheckParm("-avg");
    if (i && deathmatch)
    {
        levelTimer = true;
        levelTimeCount = 20 * 60 * 35;
    }

    i = M_CheckParm("-timer");
    if (i && deathmatch)
    {
        int time = doom_atoi(myargv[i + 1]) * 60 * 35;
        levelTimer = true;
        levelTimeCount = time;
    }

    //        Init special SECTORs.
    sector = sectors;
    for (i = 0; i < numsectors; i++, sector++)
    {
        if (!sector->special)
            continue;

        switch (sector->special)
        {
            case 1:
                // FLICKERING LIGHTS
                P_SpawnLightFlash(sector);
                break;

            case 2:
                // STROBE FAST
                P_SpawnStrobeFlash(sector, FASTDARK, 0);
                break;

            case 3:
                // STROBE SLOW
                P_SpawnStrobeFlash(sector, SLOWDARK, 0);
                break;

            case 4:
                // STROBE FAST/DEATH SLIME
                P_SpawnStrobeFlash(sector, FASTDARK, 0);
                sector->special = 4;
                break;

            case 8:
                // GLOWING LIGHT
                P_SpawnGlowingLight(sector);
                break;
            case 9:
                // SECRET SECTOR
                totalsecret++;
                break;

            case 10:
                // DOOR CLOSE IN 30 SECONDS
                P_SpawnDoorCloseIn30(sector);
                break;

            case 12:
                // SYNC STROBE SLOW
                P_SpawnStrobeFlash(sector, SLOWDARK, 1);
                break;

            case 13:
                // SYNC STROBE FAST
                P_SpawnStrobeFlash(sector, FASTDARK, 1);
                break;

            case 14:
                // DOOR RAISE IN 5 MINUTES
                P_SpawnDoorRaiseIn5Mins(sector, i);
                break;

            case 17:
                P_SpawnFireFlicker(sector);
                break;
        }
    }

    // Init line EFFECTs
    numlinespecials = 0;
    for (i = 0; i < numlines; i++)
    {
        switch (lines[i].special)
        {
            case 48:
                // EFFECT FIRSTCOL SCROLL+
                linespeciallist[numlinespecials] = &lines[i];
                numlinespecials++;
                break;
        }
    }

    // Init other misc stuff
    for (i = 0; i < MAXCEILINGS; i++)
        activeceilings[i] = nullptr;

    for (i = 0; i < MAXPLATS; i++)
        activeplats[i] = nullptr;

    for (i = 0; i < MAXBUTTONS; i++)
        doom_memset(&buttonlist[i], 0, sizeof(button_t));
}
} // namespace Doom
