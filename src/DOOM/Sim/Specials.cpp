// Rewritten out of vanilla p_spec into namespace Doom.
//
// The specials coordinator: animated flats/textures, the surrounding-sector height
// and light queries the movers use, line-special cross/shoot dispatch, per-sector
// player damage/secret, the once-a-tic Doom::updateSpecials and level-spawn setup.
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
#include "../Wad/WadFile.h"

#include "../Game/LevelStats.h"
#include "ActiveSpecials.h"
#include "AnimatedSurfaces.h"
#include "EndLevelTimer.h"
#include "Specials.h"
#include "Tick.h" // levelAlloc / levelFree / freeLevelAllocations
#include "../Game/Args.h"

#include "../Render/Data.h"
#include "Ceilings.h"
#include "Lights.h"
#include "Plats.h"
#include "Teleport.h"
#include <ea_data_structures/Structures/Array.h>

#include <new>

#include "../Game/Game.h"
#include "../Game/Sound.h"
#include "../Host/System.h"
#include "Doors.h"
#include "Floors.h"
#include "Interaction.h"
#include "Switches.h"
#include "Random.h"
#define MAXANIMS 32
#define MAXLINEANIMS 64
#define MAX_ADJOINING_SECTORS 20

// (The vanilla file-scope `Doom::SurfaceAnim` typedef that sat here was dead - unused at global scope, a
// leftover of the namespace wrap - and was removed; Doom::SurfaceAnim now lives in Sim/AnimatedSurfaces.h.)

Doom::Side* getSide(int currentSector, int line, int side)
{
    return &sides[(sectors[currentSector].lines[line])->sidenum[side]];
}

//
// getSector()
// Will return a Doom::Sector*
//  given the number of the current sector,
//  the line number and the side (0/1) that you want.
//
Doom::Sector* getSector(int currentSector, int line, int side)
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
// Return Doom::Sector * of sector next to current.
// 0 if not two-sided line
//
Doom::Sector* getNextSector(Doom::Line* line, Doom::Sector* sec)
{
    if (!(line->flags & ML_TWOSIDED))
        return nullptr;

    if (line->frontsector == sec)
        return line->backsector;

    return line->frontsector;
}

//
// Doom::findLowestFloorSurrounding()
// FIND LOWEST FLOOR HEIGHT IN SURROUNDING SECTORS
//

namespace Doom
{
// SurfaceAnim moved to Sim/AnimatedSurfaces.h with the anims/lastanim it types.

struct AnimDef
{
    // Not a boolean, despite the name and despite what vanilla called it: the
    // table below ends with {-1}, and Doom::initPicAnims walks until it finds that.
    // Compiled as C this was an int-sized enum and -1 fitted. As a C++ bool the
    // terminator would quietly become `true`, never be recognised, and the loop
    // would run off the end of the array.
    int istexture; // 0 a flat, 1 a texture, -1 ends the table
    char endname[9];
    char startname[9];
    int speed;
};

EA::Array<AnimDef, 23> animdefs = {{false, "NUKAGE3", "NUKAGE1", 8},
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
static SurfaceAnim (&anims)[MAXANIMS] = animatedSurfaces().anims;
static SurfaceAnim*& lastanim = animatedSurfaces().lastanim;

static short& numlinespecials = animatedSurfaces().numlinespecials;
static Line* (&linespeciallist)[MAXLINEANIMS] = animatedSurfaces().linespeciallist;

// Forward declarations so call order needs no rearranging.
void initPicAnims();
fixed_t findLowestFloorSurrounding(Sector* sec);
fixed_t findHighestFloorSurrounding(Sector* sec);
fixed_t findNextHighestFloor(Sector* sec, int currentheight);
fixed_t findLowestCeilingSurrounding(Sector* sec);
fixed_t findHighestCeilingSurrounding(Sector* sec);
int findSectorFromLineTag(Line* line, int start);
int findMinSurroundingLight(Sector* sector, int max);
void crossSpecialLine(int linenum, int side, Mobj* thing);
void shootSpecialLine(Mobj* thing, Line* line);
void playerInSpecialSector(Player* player);
void updateSpecials();
int doDonut(Line* line);
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
            if (Doom::checkTextureNumForName(animdefs[i].startname) == -1)
                continue;

            lastanim->picnum = Doom::textureNumForName(animdefs[i].endname);
            lastanim->basepic = Doom::textureNumForName(animdefs[i].startname);
        }
        else
        {
            if (Doom::wad().find(animdefs[i].startname) == -1)
                continue;

            lastanim->picnum = Doom::flatNumForName(animdefs[i].endname);
            lastanim->basepic = Doom::flatNumForName(animdefs[i].startname);
        }

        lastanim->istexture = animdefs[i].istexture;
        lastanim->numpics = lastanim->picnum - lastanim->basepic + 1;

        if (lastanim->numpics < 2)
        {
            //fatalError("Error: initPicAnims: bad cycle from %s to %s",
            //        animdefs[i].startname,
            //        animdefs[i].endname);

            doom_strcpy(error_buf, "Error: initPicAnims: bad cycle from ");
            doom_concat(error_buf, animdefs[i].startname);
            doom_concat(error_buf, " to ");
            doom_concat(error_buf, animdefs[i].endname);
            fatalError(error_buf);
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
// Will return a Side*
//  given the number of the current sector,
//  the line number, and the side (0/1) that you want.
//
fixed_t findLowestFloorSurrounding(Sector* sec)
{
    Line* check;
    Sector* other;
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
fixed_t findHighestFloorSurrounding(Sector* sec)
{
    Line* check;
    Sector* other;
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
fixed_t findNextHighestFloor(Sector* sec, int currentheight)
{
    int i;
    int h;
    int min;
    Line* check;
    Sector* other;
    fixed_t height = currentheight;

    EA::Array<fixed_t, MAX_ADJOINING_SECTORS> heightlist;

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
fixed_t findLowestCeilingSurrounding(Sector* sec)
{
    Line* check;
    Sector* other;
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
fixed_t findHighestCeilingSurrounding(Sector* sec)
{
    Line* check;
    Sector* other;
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
int findSectorFromLineTag(Line* line, int start)
{
    for (int i = start + 1; i < numsectors; i++)
        if (sectors[i].tag == line->tag)
            return i;

    return -1;
}

//
// Find minimum light from an adjacent sector
//
int findMinSurroundingLight(Sector* sector, int max)
{
    int min;
    Line* line;
    Sector* check;

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
void crossSpecialLine(int linenum, int side, Mobj* thing)
{
    Line* line;
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
            Doom::doDoor(line, door_open);
            line->special = 0;
            break;

        case 3:
            // Close Door
            Doom::doDoor(line, door_close);
            line->special = 0;
            break;

        case 4:
            // Raise Door
            Doom::doDoor(line, door_normal);
            line->special = 0;
            break;

        case 5:
            // Raise Floor
            Doom::doFloor(line, raiseFloor);
            line->special = 0;
            break;

        case 6:
            // Fast Ceiling Crush & Raise
            Doom::doCeiling(line, fastCrushAndRaise);
            line->special = 0;
            break;

        case 8:
            // Build Stairs
            Doom::buildStairs(line, build8);
            line->special = 0;
            break;

        case 10:
            // PlatDownWaitUp
            Doom::doPlat(line, downWaitUpStay, 0);
            line->special = 0;
            break;

        case 12:
            // Light Turn On - brightest near
            Doom::lightTurnOn(line, 0);
            line->special = 0;
            break;

        case 13:
            // Light Turn On 255
            Doom::lightTurnOn(line, 255);
            line->special = 0;
            break;

        case 16:
            // Close Door 30
            Doom::doDoor(line, close30ThenOpen);
            line->special = 0;
            break;

        case 17:
            // Start Light Strobing
            Doom::startLightStrobing(line);
            line->special = 0;
            break;

        case 19:
            // Lower Floor
            Doom::doFloor(line, lowerFloor);
            line->special = 0;
            break;

        case 22:
            // Raise floor to nearest height and change texture
            Doom::doPlat(line, raiseToNearestAndChange, 0);
            line->special = 0;
            break;

        case 25:
            // Ceiling Crush and Raise
            Doom::doCeiling(line, crushAndRaise);
            line->special = 0;
            break;

        case 30:
            // Raise floor to shortest texture height
            //  on either side of lines.
            Doom::doFloor(line, raiseToTexture);
            line->special = 0;
            break;

        case 35:
            // Lights Very Dark
            Doom::lightTurnOn(line, 35);
            line->special = 0;
            break;

        case 36:
            // Lower Floor (TURBO)
            Doom::doFloor(line, turboLower);
            line->special = 0;
            break;

        case 37:
            // LowerAndChange
            Doom::doFloor(line, lowerAndChange);
            line->special = 0;
            break;

        case 38:
            // Lower Floor To Lowest
            Doom::doFloor(line, lowerFloorToLowest);
            line->special = 0;
            break;

        case 39:
            // TELEPORT!
            Doom::teleport(line, side, thing);
            line->special = 0;
            break;

        case 40:
            // RaiseCeilingLowerFloor
            Doom::doCeiling(line, raiseToHighest);
            Doom::doFloor(line, lowerFloorToLowest);
            line->special = 0;
            break;

        case 44:
            // Ceiling Crush
            Doom::doCeiling(line, lowerAndCrush);
            line->special = 0;
            break;

        case 52:
            // EXIT!
            Doom::exitLevel();
            break;

        case 53:
            // Perpetual Platform Raise
            Doom::doPlat(line, perpetualRaise, 0);
            line->special = 0;
            break;

        case 54:
            // Platform Stop
            Doom::stopPlat(line);
            line->special = 0;
            break;

        case 56:
            // Raise Floor Crush
            Doom::doFloor(line, raiseFloorCrush);
            line->special = 0;
            break;

        case 57:
            // Ceiling Crush Stop
            Doom::ceilingCrushStop(line);
            line->special = 0;
            break;

        case 58:
            // Raise Floor 24
            Doom::doFloor(line, raiseFloor24);
            line->special = 0;
            break;

        case 59:
            // Raise Floor 24 And Change
            Doom::doFloor(line, raiseFloor24AndChange);
            line->special = 0;
            break;

        case 104:
            // Turn lights off in sector(tag)
            Doom::turnTagLightsOff(line);
            line->special = 0;
            break;

        case 108:
            // Blazing Door Raise (faster than TURBO!)
            Doom::doDoor(line, blazeRaise);
            line->special = 0;
            break;

        case 109:
            // Blazing Door Open (faster than TURBO!)
            Doom::doDoor(line, blazeOpen);
            line->special = 0;
            break;

        case 100:
            // Build Stairs Turbo 16
            Doom::buildStairs(line, turbo16);
            line->special = 0;
            break;

        case 110:
            // Blazing Door Close (faster than TURBO!)
            Doom::doDoor(line, blazeClose);
            line->special = 0;
            break;

        case 119:
            // Raise floor to nearest surr. floor
            Doom::doFloor(line, raiseFloorToNearest);
            line->special = 0;
            break;

        case 121:
            // Blazing PlatDownWaitUpStay
            Doom::doPlat(line, blazeDWUS, 0);
            line->special = 0;
            break;

        case 124:
            // Secret EXIT
            Doom::secretExitLevel();
            break;

        case 125:
            // TELEPORT MonsterONLY
            if (!thing->player)
            {
                Doom::teleport(line, side, thing);
                line->special = 0;
            }
            break;

        case 130:
            // Raise Floor Turbo
            Doom::doFloor(line, raiseFloorTurbo);
            line->special = 0;
            break;

        case 141:
            // Silent Ceiling Crush & Raise
            Doom::doCeiling(line, silentCrushAndRaise);
            line->special = 0;
            break;

            // RETRIGGERS.  All from here till end.
        case 72:
            // Ceiling Crush
            Doom::doCeiling(line, lowerAndCrush);
            break;

        case 73:
            // Ceiling Crush and Raise
            Doom::doCeiling(line, crushAndRaise);
            break;

        case 74:
            // Ceiling Crush Stop
            Doom::ceilingCrushStop(line);
            break;

        case 75:
            // Close Door
            Doom::doDoor(line, door_close);
            break;

        case 76:
            // Close Door 30
            Doom::doDoor(line, close30ThenOpen);
            break;

        case 77:
            // Fast Ceiling Crush & Raise
            Doom::doCeiling(line, fastCrushAndRaise);
            break;

        case 79:
            // Lights Very Dark
            Doom::lightTurnOn(line, 35);
            break;

        case 80:
            // Light Turn On - brightest near
            Doom::lightTurnOn(line, 0);
            break;

        case 81:
            // Light Turn On 255
            Doom::lightTurnOn(line, 255);
            break;

        case 82:
            // Lower Floor To Lowest
            Doom::doFloor(line, lowerFloorToLowest);
            break;

        case 83:
            // Lower Floor
            Doom::doFloor(line, lowerFloor);
            break;

        case 84:
            // LowerAndChange
            Doom::doFloor(line, lowerAndChange);
            break;

        case 86:
            // Open Door
            Doom::doDoor(line, door_open);
            break;

        case 87:
            // Perpetual Platform Raise
            Doom::doPlat(line, perpetualRaise, 0);
            break;

        case 88:
            // PlatDownWaitUp
            Doom::doPlat(line, downWaitUpStay, 0);
            break;

        case 89:
            // Platform Stop
            Doom::stopPlat(line);
            break;

        case 90:
            // Raise Door
            Doom::doDoor(line, door_normal);
            break;

        case 91:
            // Raise Floor
            Doom::doFloor(line, raiseFloor);
            break;

        case 92:
            // Raise Floor 24
            Doom::doFloor(line, raiseFloor24);
            break;

        case 93:
            // Raise Floor 24 And Change
            Doom::doFloor(line, raiseFloor24AndChange);
            break;

        case 94:
            // Raise Floor Crush
            Doom::doFloor(line, raiseFloorCrush);
            break;

        case 95:
            // Raise floor to nearest height
            // and change texture.
            Doom::doPlat(line, raiseToNearestAndChange, 0);
            break;

        case 96:
            // Raise floor to shortest texture height
            // on either side of lines.
            Doom::doFloor(line, raiseToTexture);
            break;

        case 97:
            // TELEPORT!
            Doom::teleport(line, side, thing);
            break;

        case 98:
            // Lower Floor (TURBO)
            Doom::doFloor(line, turboLower);
            break;

        case 105:
            // Blazing Door Raise (faster than TURBO!)
            Doom::doDoor(line, blazeRaise);
            break;

        case 106:
            // Blazing Door Open (faster than TURBO!)
            Doom::doDoor(line, blazeOpen);
            break;

        case 107:
            // Blazing Door Close (faster than TURBO!)
            Doom::doDoor(line, blazeClose);
            break;

        case 120:
            // Blazing PlatDownWaitUpStay.
            Doom::doPlat(line, blazeDWUS, 0);
            break;

        case 126:
            // TELEPORT MonsterONLY.
            if (!thing->player)
                Doom::teleport(line, side, thing);
            break;

        case 128:
            // Raise To Nearest Floor
            Doom::doFloor(line, raiseFloorToNearest);
            break;

        case 129:
            // Raise Floor Turbo
            Doom::doFloor(line, raiseFloorTurbo);
            break;
    }
}

//
// shootSpecialLine - IMPACT SPECIALS
// Called when a thing shoots a special line.
//
void shootSpecialLine(Mobj* thing, Line* line)
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
            Doom::doFloor(line, raiseFloor);
            Doom::changeSwitchTexture(line, 0);
            break;

        case 46:
            // OPEN DOOR
            Doom::doDoor(line, door_open);
            Doom::changeSwitchTexture(line, 1);
            break;

        case 47:
            // RAISE FLOOR NEAR AND CHANGE
            Doom::doPlat(line, raiseToNearestAndChange, 0);
            Doom::changeSwitchTexture(line, 0);
            break;
    }
}

//
// playerInSpecialSector
// Called every tic frame
//  that the player origin is in a special sector
//
void playerInSpecialSector(Player* player)
{
    Sector* sector;

    auto& stats = levelStats();

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
                if (!(stats.leveltime & 0x1f))
                    Doom::damageMobj(player->mo, 0, 0, 10);
            break;

        case 7:
            // NUKAGE DAMAGE
            if (!player->powers[pw_ironfeet])
                if (!(stats.leveltime & 0x1f))
                    Doom::damageMobj(player->mo, 0, 0, 5);
            break;

        case 16:
            // SUPER HELLSLIME DAMAGE
        case 4:
            // STROBE HURT
            if (!player->powers[pw_ironfeet] || (Doom::randomness().forPlay() < 5))
            {
                if (!(stats.leveltime & 0x1f))
                    Doom::damageMobj(player->mo, 0, 0, 20);
            }
            break;

        case 9:
            // SECRET SECTOR
            player->secretcount++;
            player->message = "A secret is revealed!";
            Doom::startSound(nullptr, sfx_getpow);
            sector->special = 0;
            break;

        case 11:
            // EXIT SUPER DAMAGE! (for E1M8 finale)
            player->cheats &= ~CF_GODMODE;

            if (!(stats.leveltime & 0x1f))
                Doom::damageMobj(player->mo, 0, 0, 20);

            if (player->health <= 10)
                Doom::exitLevel();
            break;

        default:
        {
            //fatalError("Error: playerInSpecialSector: "
            //        "unknown special %i",
            //        sector->special);

            doom_strcpy(error_buf, "Error: playerInSpecialSector: unknown special ");
            doom_concat(error_buf, doom_itoa(sector->special, 10));
            fatalError(error_buf);
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
    SurfaceAnim* anim;
    int pic;
    Line* line;

    auto& timer = endLevelTimer();
    auto& specials = activeSpecials();

    // LEVEL TIMER
    if (timer.levelTimer == true)
    {
        timer.levelTimeCount--;
        if (!timer.levelTimeCount)
            Doom::exitLevel();
    }

    // ANIMATE FLATS AND TEXTURES GLOBALLY
    for (anim = anims; anim < lastanim; anim++)
    {
        for (int i = anim->basepic; i < anim->basepic + anim->numpics; i++)
        {
            pic = anim->basepic
                  + ((levelStats().leveltime / anim->speed + i) % anim->numpics);
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
        if (specials.buttonlist[i].btimer)
        {
            specials.buttonlist[i].btimer--;
            if (!specials.buttonlist[i].btimer)
            {
                switch (specials.buttonlist[i].where)
                {
                    case top:
                        sides[specials.buttonlist[i].line->sidenum[0]].toptexture =
                            specials.buttonlist[i].btexture;
                        break;

                    case middle:
                        sides[specials.buttonlist[i].line->sidenum[0]].midtexture =
                            specials.buttonlist[i].btexture;
                        break;

                    case bottom:
                        sides[specials.buttonlist[i].line->sidenum[0]].bottomtexture =
                            specials.buttonlist[i].btexture;
                        break;
                }
                Doom::startSound(
                    reinterpret_cast<Mobj*>(&specials.buttonlist[i].soundorg),
                    sfx_swtchn);
                doom_memset(&specials.buttonlist[i], 0, sizeof(Button));
            }
        }
}

//
// Special Stuff that can not be categorized
//
int doDonut(Line* line)
{
    Sector* s1;
    Sector* s2;
    Sector* s3;
    int secnum;
    int rtn;
    FloorMove* floor;

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
            floor = new (levelAlloc(sizeof(*floor))) FloorMove {};
            Doom::addThinker(floor);
            s2->specialdata = floor;
            floor->type = donutRaise;
            floor->crush = false;
            floor->direction = 1;
            floor->sector = s2;
            floor->speed = FLOORSPEED / 2;
            floor->texture = s3->floorpic;
            floor->newspecial = 0;
            floor->floordestheight = s3->floorheight;

            //        Spawn lowering donut-hole
            floor = new (levelAlloc(sizeof(*floor))) FloorMove {};
            Doom::addThinker(floor);
            s1->specialdata = floor;
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
    Sector* sector;
    int i;

    auto& timer = endLevelTimer();
    auto& specials = activeSpecials();

    // See if -TIMER needs to be used.
    timer.levelTimer = false;

    i = Doom::checkParm("-avg");
    if (i && deathmatch)
    {
        timer.levelTimer = true;
        timer.levelTimeCount = 20 * 60 * 35;
    }

    i = Doom::checkParm("-timer");
    if (i && deathmatch)
    {
        int time = doom_atoi(myargv[i + 1]) * 60 * 35;
        timer.levelTimer = true;
        timer.levelTimeCount = time;
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
                Doom::spawnLightFlash(sector);
                break;

            case 2:
                // STROBE FAST
                Doom::spawnStrobeFlash(sector, FASTDARK, 0);
                break;

            case 3:
                // STROBE SLOW
                Doom::spawnStrobeFlash(sector, SLOWDARK, 0);
                break;

            case 4:
                // STROBE FAST/DEATH SLIME
                Doom::spawnStrobeFlash(sector, FASTDARK, 0);
                sector->special = 4;
                break;

            case 8:
                // GLOWING LIGHT
                Doom::spawnGlowingLight(sector);
                break;
            case 9:
                // SECRET SECTOR
                totalsecret++;
                break;

            case 10:
                // DOOR CLOSE IN 30 SECONDS
                Doom::spawnDoorCloseIn30(sector);
                break;

            case 12:
                // SYNC STROBE SLOW
                Doom::spawnStrobeFlash(sector, SLOWDARK, 1);
                break;

            case 13:
                // SYNC STROBE FAST
                Doom::spawnStrobeFlash(sector, FASTDARK, 1);
                break;

            case 14:
                // DOOR RAISE IN 5 MINUTES
                Doom::spawnDoorRaiseIn5Mins(sector, i);
                break;

            case 17:
                Doom::spawnFireFlicker(sector);
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
        specials.activeceilings[i] = nullptr;

    for (i = 0; i < MAXPLATS; i++)
        specials.activeplats[i] = nullptr;

    for (i = 0; i < MAXBUTTONS; i++)
        doom_memset(&specials.buttonlist[i], 0, sizeof(Button));
}
} // namespace Doom
