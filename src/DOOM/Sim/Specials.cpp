// Rewritten out of vanilla p_spec into namespace Doom.
//
// The specials coordinator: animated flats/textures, the surrounding-sector height
// and light queries the movers use, line-special cross/shoot dispatch, per-sector
// player damage/secret, the once-a-tic Doom::updateSpecials and level-spawn setup.
// getSide/getSector/twoSided/getNextSector stay at global scope (p_spec.h API);
// every P_/EV_ entry is shimmed by p_spec.cpp; the animation state is file-local and
// levelTimer stays global. Golden-neutral - the demos scroll skies, damage in slime
// and open exits.

#include "../Host/Platform.h"
#include "Level.h"
#include "../Render/GraphicsData.h"

#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h"
#include "../Game/Args.h"
#include "Random.h"
#include "SimDefs.h"
#include "../Game/SoundData.h"
#include "../Wad/WadFile.h"

#include "../Game/GameSession.h"
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
#include "../Containers.h"

#include <new>

#include "../Game/Game.h"
#include "../Game/Sound.h"
#include "../Host/System.h"
#include "Doors.h"
#include "Floors.h"
#include "Interaction.h"
#include "Switches.h"
#include "Random.h"

// (The vanilla file-scope `Doom::SurfaceAnim` typedef that sat here was dead - unused at global scope, a
// leftover of the namespace wrap - and was removed; Doom::SurfaceAnim now lives in Sim/AnimatedSurfaces.h.)

Doom::Side* getSide(int currentSector, int line, int side)
{
    return &Doom::level().sides[(Doom::level().sectors[currentSector].lines[line])
                                    ->sidenum[side]];
}

//
// getSector()
// Will return a Doom::Sector*
//  given the number of the current sector,
//  the line number and the side (0/1) that you want.
//
Doom::Sector* getSector(int currentSector, int line, int side)
{
    return Doom::level()
        .sides[(Doom::level().sectors[currentSector].lines[line])->sidenum[side]]
        .sector;
}

//
// twoSided()
// Given the sector number and the line number,
//  it will tell you whether the line is two-sided or not.
//
int twoSided(int sector, int line)
{
    return (Doom::level().sectors[sector].lines[line])->flags & Doom::ML_TWOSIDED;
}

//
// getNextSector()
// Return Doom::Sector * of sector next to current.
// 0 if not two-sided line
//
Doom::Sector* getNextSector(Doom::Line& line, Doom::Sector& sec)
{
    if (!(line.flags & Doom::ML_TWOSIDED))
        return nullptr;

    if (line.frontsector == &sec)
        return line.backsector;

    return line.frontsector;
}

//
// Doom::findLowestFloorSurrounding()
// FIND LOWEST FLOOR HEIGHT IN SURROUNDING SECTORS
//

namespace Doom
{
constexpr int MAX_ADJOINING_SECTORS = 20;

// SurfaceAnim moved to Sim/AnimatedSurfaces.h with the anims/lastanim it types.

struct AnimDef
{
    // Not a boolean, despite the name and despite what vanilla called it: the
    // table below ends with {-1}, and initPicAnims walks until it finds that.
    // Compiled as C this was an int-sized enum and -1 fitted. As a C++ bool the
    // terminator would quietly become `true`, never be recognised, and the loop
    // would run off the end of the array.
    int istexture; // 0 a flat, 1 a texture, -1 ends the table
    std::string_view endname;
    std::string_view startname;
    int speed;
};

Array<AnimDef, 23> animdefs = {{false, "NUKAGE3", "NUKAGE1", 8},
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
// (Sim/AnimatedSurfaces.h, moved by the file-scope-statics sweep - REFACTOR.md, Step 5).
// initPicAnims, updateSpecials and spawnSpecials each hoist animatedSurfaces() once and reach its
// members through it, rather than through file-scope reference aliases (REFACTOR.md, Step 9
// strand (a)).

// Forward declarations so call order needs no rearranging. The height/light queries,
// the line-special dispatchers and doDonut are now Sector/Line methods (MapTypes.h);
// only the level-wide coordinators, which have no single owner, stay free functions.
void initPicAnims();
void updateSpecials();
void spawnSpecials();

void initPicAnims()
{
    auto& surf = animatedSurfaces();

    // Init animation
    surf.anims.clear();
    for (int i = 0; animdefs[i].istexture != -1; i++)
    {
        auto anim = SurfaceAnim {};

        if (animdefs[i].istexture)
        {
            // different episode ?
            if (checkTextureNumForName(animdefs[i].startname) == -1)
                continue;

            anim.picnum = textureNumForName(animdefs[i].endname);
            anim.basepic = textureNumForName(animdefs[i].startname);
        }
        else
        {
            if (wad().find(animdefs[i].startname) == -1)
                continue;

            anim.picnum = flatNumForName(animdefs[i].endname);
            anim.basepic = flatNumForName(animdefs[i].startname);
        }

        anim.istexture = animdefs[i].istexture;
        anim.numpics = anim.picnum - anim.basepic + 1;

        if (anim.numpics < 2)
        {
            //fatalError("Error: initPicAnims: bad cycle from %s to %s",
            //        animdefs[i].startname,
            //        animdefs[i].endname);

            fatalError("Error: initPicAnims: bad cycle from ",
                       animdefs[i].startname,
                       " to ",
                       animdefs[i].endname);
        }

        anim.speed = animdefs[i].speed;
        surf.anims.add(anim);
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
Fixed Sector::findLowestFloorSurrounding()
{
    Fixed floor = floorheight;

    for (int i = 0; i < linecount; i++)
    {
        Line* check = lines[i];
        Sector* other = getNextSector(*check, *this);

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
Fixed Sector::findHighestFloorSurrounding()
{
    Fixed floor = -500 * FRACUNIT;

    for (int i = 0; i < linecount; i++)
    {
        Line* check = lines[i];
        Sector* other = getNextSector(*check, *this);

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
Fixed Sector::findNextHighestFloor(Fixed currentheight)
{
    int i;
    int h;
    Fixed height = currentheight;

    Array<Fixed, MAX_ADJOINING_SECTORS> heightlist;

    for (i = 0, h = 0; i < linecount; i++)
    {
        Line* check = lines[i];
        Sector* other = getNextSector(*check, *this);

        if (!other)
            continue;

        if (other->floorheight > height)
            heightlist[h++] = other->floorheight;

        // Check for overflow. Exit.
        if (h >= MAX_ADJOINING_SECTORS)
        {
            print("Sector with more than 20 adjoining sectors\n");
            break;
        }
    }

    // Find lowest height in list
    if (!h)
        return currentheight;

    Fixed min = heightlist[0];

    // Range checking?
    for (i = 1; i < h; i++)
        if (heightlist[i] < min)
            min = heightlist[i];

    return min;
}

//
// FIND LOWEST CEILING IN THE SURROUNDING SECTORS
//
Fixed Sector::findLowestCeilingSurrounding()
{
    auto height = Fixed {DOOM_MAXINT};

    for (int i = 0; i < linecount; i++)
    {
        Line* check = lines[i];
        Sector* other = getNextSector(*check, *this);

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
Fixed Sector::findHighestCeilingSurrounding()
{
    Fixed height {};

    for (int i = 0; i < linecount; i++)
    {
        Line* check = lines[i];
        Sector* other = getNextSector(*check, *this);

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
int Line::findSectorFromLineTag(int start)
{
    for (int i = start + 1; i < level().sectors.size(); i++)
        if (level().sectors[i].tag == tag)
            return i;

    return -1;
}

//
// Find minimum light from an adjacent sector
//
int Sector::findMinSurroundingLight(int max)
{
    int min = max;
    for (int i = 0; i < linecount; i++)
    {
        Line* line = lines[i];
        Sector* check = getNextSector(*line, *this);

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
void Line::crossSpecialLine(int side, Mobj& thing)
{
    //        Triggers that other things can activate
    if (!thing.player)
    {
        // Things that should NOT trigger specials...
        switch (thing.type)
        {
            case MobjType::Rocket:
            case MobjType::Plasma:
            case MobjType::Bfg:
            case MobjType::Troopshot:
            case MobjType::Headshot:
            case MobjType::Bruisershot:
                return;
                break;

            default:
                break;
        }

        int ok = 0;
        switch (special)
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
    switch (special)
    {
        // TRIGGERS.
        // All from here to RETRIGGERS.
        case 2:
            // Open Door
            doDoor(DoorType::Open);
            special = 0;
            break;

        case 3:
            // Close Door
            doDoor(DoorType::Close);
            special = 0;
            break;

        case 4:
            // Raise Door
            doDoor(DoorType::Normal);
            special = 0;
            break;

        case 5:
            // Raise Floor
            doFloor(FloorType::RaiseFloor);
            special = 0;
            break;

        case 6:
            // Fast Ceiling Crush & Raise
            doCeiling(CeilingType::FastCrushAndRaise);
            special = 0;
            break;

        case 8:
            // Build Stairs
            buildStairs(StairType::Build8);
            special = 0;
            break;

        case 10:
            // PlatDownWaitUp
            doPlat(PlatType::DownWaitUpStay, 0);
            special = 0;
            break;

        case 12:
            // Light Turn On - brightest near
            lightTurnOn(0);
            special = 0;
            break;

        case 13:
            // Light Turn On 255
            lightTurnOn(255);
            special = 0;
            break;

        case 16:
            // Close Door 30
            doDoor(DoorType::Close30ThenOpen);
            special = 0;
            break;

        case 17:
            // Start Light Strobing
            startLightStrobing();
            special = 0;
            break;

        case 19:
            // Lower Floor
            doFloor(FloorType::LowerFloor);
            special = 0;
            break;

        case 22:
            // Raise floor to nearest height and change texture
            doPlat(PlatType::RaiseToNearestAndChange, 0);
            special = 0;
            break;

        case 25:
            // Ceiling Crush and Raise
            doCeiling(CeilingType::CrushAndRaise);
            special = 0;
            break;

        case 30:
            // Raise floor to shortest texture height
            //  on either side of lines.
            doFloor(FloorType::RaiseToTexture);
            special = 0;
            break;

        case 35:
            // Lights Very Dark
            lightTurnOn(35);
            special = 0;
            break;

        case 36:
            // Lower Floor (TURBO)
            doFloor(FloorType::TurboLower);
            special = 0;
            break;

        case 37:
            // LowerAndChange
            doFloor(FloorType::LowerAndChange);
            special = 0;
            break;

        case 38:
            // Lower Floor To Lowest
            doFloor(FloorType::LowerFloorToLowest);
            special = 0;
            break;

        case 39:
            // TELEPORT!
            teleport(side, thing);
            special = 0;
            break;

        case 40:
            // RaiseCeilingLowerFloor
            doCeiling(CeilingType::RaiseToHighest);
            doFloor(FloorType::LowerFloorToLowest);
            special = 0;
            break;

        case 44:
            // Ceiling Crush
            doCeiling(CeilingType::LowerAndCrush);
            special = 0;
            break;

        case 52:
            // EXIT!
            exitLevel();
            break;

        case 53:
            // Perpetual Platform Raise
            doPlat(PlatType::PerpetualRaise, 0);
            special = 0;
            break;

        case 54:
            // Platform Stop
            stopPlat();
            special = 0;
            break;

        case 56:
            // Raise Floor Crush
            doFloor(FloorType::RaiseFloorCrush);
            special = 0;
            break;

        case 57:
            // Ceiling Crush Stop
            ceilingCrushStop();
            special = 0;
            break;

        case 58:
            // Raise Floor 24
            doFloor(FloorType::RaiseFloor24);
            special = 0;
            break;

        case 59:
            // Raise Floor 24 And Change
            doFloor(FloorType::RaiseFloor24AndChange);
            special = 0;
            break;

        case 104:
            // Turn lights off in sector(tag)
            turnTagLightsOff();
            special = 0;
            break;

        case 108:
            // Blazing Door Raise (faster than TURBO!)
            doDoor(DoorType::BlazeRaise);
            special = 0;
            break;

        case 109:
            // Blazing Door Open (faster than TURBO!)
            doDoor(DoorType::BlazeOpen);
            special = 0;
            break;

        case 100:
            // Build Stairs Turbo 16
            buildStairs(StairType::Turbo16);
            special = 0;
            break;

        case 110:
            // Blazing Door Close (faster than TURBO!)
            doDoor(DoorType::BlazeClose);
            special = 0;
            break;

        case 119:
            // Raise floor to nearest surr. floor
            doFloor(FloorType::RaiseFloorToNearest);
            special = 0;
            break;

        case 121:
            // Blazing PlatDownWaitUpStay
            doPlat(PlatType::BlazeDWUS, 0);
            special = 0;
            break;

        case 124:
            // Secret EXIT
            secretExitLevel();
            break;

        case 125:
            // TELEPORT MonsterONLY
            if (!thing.player)
            {
                teleport(side, thing);
                special = 0;
            }
            break;

        case 130:
            // Raise Floor Turbo
            doFloor(FloorType::RaiseFloorTurbo);
            special = 0;
            break;

        case 141:
            // Silent Ceiling Crush & Raise
            doCeiling(CeilingType::SilentCrushAndRaise);
            special = 0;
            break;

            // RETRIGGERS.  All from here till end.
        case 72:
            // Ceiling Crush
            doCeiling(CeilingType::LowerAndCrush);
            break;

        case 73:
            // Ceiling Crush and Raise
            doCeiling(CeilingType::CrushAndRaise);
            break;

        case 74:
            // Ceiling Crush Stop
            ceilingCrushStop();
            break;

        case 75:
            // Close Door
            doDoor(DoorType::Close);
            break;

        case 76:
            // Close Door 30
            doDoor(DoorType::Close30ThenOpen);
            break;

        case 77:
            // Fast Ceiling Crush & Raise
            doCeiling(CeilingType::FastCrushAndRaise);
            break;

        case 79:
            // Lights Very Dark
            lightTurnOn(35);
            break;

        case 80:
            // Light Turn On - brightest near
            lightTurnOn(0);
            break;

        case 81:
            // Light Turn On 255
            lightTurnOn(255);
            break;

        case 82:
            // Lower Floor To Lowest
            doFloor(FloorType::LowerFloorToLowest);
            break;

        case 83:
            // Lower Floor
            doFloor(FloorType::LowerFloor);
            break;

        case 84:
            // LowerAndChange
            doFloor(FloorType::LowerAndChange);
            break;

        case 86:
            // Open Door
            doDoor(DoorType::Open);
            break;

        case 87:
            // Perpetual Platform Raise
            doPlat(PlatType::PerpetualRaise, 0);
            break;

        case 88:
            // PlatDownWaitUp
            doPlat(PlatType::DownWaitUpStay, 0);
            break;

        case 89:
            // Platform Stop
            stopPlat();
            break;

        case 90:
            // Raise Door
            doDoor(DoorType::Normal);
            break;

        case 91:
            // Raise Floor
            doFloor(FloorType::RaiseFloor);
            break;

        case 92:
            // Raise Floor 24
            doFloor(FloorType::RaiseFloor24);
            break;

        case 93:
            // Raise Floor 24 And Change
            doFloor(FloorType::RaiseFloor24AndChange);
            break;

        case 94:
            // Raise Floor Crush
            doFloor(FloorType::RaiseFloorCrush);
            break;

        case 95:
            // Raise floor to nearest height
            // and change texture.
            doPlat(PlatType::RaiseToNearestAndChange, 0);
            break;

        case 96:
            // Raise floor to shortest texture height
            // on either side of lines.
            doFloor(FloorType::RaiseToTexture);
            break;

        case 97:
            // TELEPORT!
            teleport(side, thing);
            break;

        case 98:
            // Lower Floor (TURBO)
            doFloor(FloorType::TurboLower);
            break;

        case 105:
            // Blazing Door Raise (faster than TURBO!)
            doDoor(DoorType::BlazeRaise);
            break;

        case 106:
            // Blazing Door Open (faster than TURBO!)
            doDoor(DoorType::BlazeOpen);
            break;

        case 107:
            // Blazing Door Close (faster than TURBO!)
            doDoor(DoorType::BlazeClose);
            break;

        case 120:
            // Blazing PlatDownWaitUpStay.
            doPlat(PlatType::BlazeDWUS, 0);
            break;

        case 126:
            // TELEPORT MonsterONLY.
            if (!thing.player)
                teleport(side, thing);
            break;

        case 128:
            // Raise To Nearest Floor
            doFloor(FloorType::RaiseFloorToNearest);
            break;

        case 129:
            // Raise Floor Turbo
            doFloor(FloorType::RaiseFloorTurbo);
            break;
    }
}

//
// shootSpecialLine - IMPACT SPECIALS
// Called when a thing shoots a special line.
//
void Line::shootSpecialLine(Mobj& thing)
{
    // Impacts that other things can activate.
    if (!thing.player)
    {
        int ok = 0;
        switch (special)
        {
            case 46:
                // OPEN DOOR IMPACT
                ok = 1;
                break;
        }
        if (!ok)
            return;
    }

    switch (special)
    {
        case 24:
            // RAISE FLOOR
            doFloor(FloorType::RaiseFloor);
            changeSwitchTexture(0);
            break;

        case 46:
            // OPEN DOOR
            doDoor(DoorType::Open);
            changeSwitchTexture(1);
            break;

        case 47:
            // RAISE FLOOR NEAR AND CHANGE
            doPlat(PlatType::RaiseToNearestAndChange, 0);
            changeSwitchTexture(0);
            break;
    }
}

//
// playerInSpecialSector
// Called every tic frame
//  that the player origin is in a special sector
//
void Player::inSpecialSector()
{
    auto& stats = levelStats();

    Sector* sector = mo->subsector->sector;

    // Falling, not all the way down yet?
    if (mo->z != sector->floorheight)
        return;

    // Has hitten ground.
    switch (sector->special)
    {
        case 5:
            // HELLSLIME DAMAGE
            if (!powers[toIndex(PowerType::IronFeet)])
                if (!(stats.leveltime & 0x1f))
                    mo->damage(0, 0, 10);
            break;

        case 7:
            // NUKAGE DAMAGE
            if (!powers[toIndex(PowerType::IronFeet)])
                if (!(stats.leveltime & 0x1f))
                    mo->damage(0, 0, 5);
            break;

        case 16:
            // SUPER HELLSLIME DAMAGE
        case 4:
            // STROBE HURT
            if (!powers[toIndex(PowerType::IronFeet)]
                || (randomness().forPlay() < 5))
            {
                if (!(stats.leveltime & 0x1f))
                    mo->damage(0, 0, 20);
            }
            break;

        case 9:
            // SECRET SECTOR
            secretcount++;
            message = "A secret is revealed!";
            startSound(nullptr, SfxEnum::Getpow);
            sector->special = 0;
            break;

        case 11:
            // EXIT SUPER DAMAGE! (for E1M8 finale)
            cheats = withoutFlags(cheats, CheatFlag::GodMode);

            if (!(stats.leveltime & 0x1f))
                mo->damage(0, 0, 20);

            if (health <= 10)
                exitLevel();
            break;

        default:
        {
            //fatalError("Error: playerInSpecialSector: "
            //        "unknown special %i",
            //        sector->special);

            fatalError("Error: playerInSpecialSector: unknown special ",
                       sector->special);
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
    auto& timer = endLevelTimer();
    auto& specials = activeSpecials();
    auto& surf = animatedSurfaces();

    // LEVEL TIMER
    if (timer.levelTimer == true)
    {
        timer.levelTimeCount--;
        if (!timer.levelTimeCount)
            exitLevel();
    }

    // ANIMATE FLATS AND TEXTURES GLOBALLY
    for (auto& anim: surf.anims)
    {
        for (int i = anim.basepic; i < anim.basepic + anim.numpics; i++)
        {
            int pic = anim.basepic
                      + ((levelStats().leveltime / anim.speed + i) % anim.numpics);
            if (anim.istexture)
                graphicsData().texturetranslation[i] = pic;
            else
                graphicsData().flattranslation[i] = pic;
        }
    }

    // ANIMATE LINE SPECIALS
    for (auto* line: surf.linespeciallist)
    {
        switch (line->special)
        {
            case 48:
                // EFFECT FIRSTCOL SCROLL +
                level().sides[line->sidenum[0]].textureoffset += FRACUNIT;
                break;
        }
    }

    // DO BUTTONS
    for (auto& button: specials.buttonlist)
        if (button.btimer)
        {
            button.btimer--;
            if (!button.btimer)
            {
                switch (button.where)
                {
                    case ButtonWhere::Top:
                        level().sides[button.line->sidenum[0]].toptexture =
                            button.btexture;
                        break;

                    case ButtonWhere::Middle:
                        level().sides[button.line->sidenum[0]].midtexture =
                            button.btexture;
                        break;

                    case ButtonWhere::Bottom:
                        level().sides[button.line->sidenum[0]].bottomtexture =
                            button.btexture;
                        break;
                }
                // vanilla's degenmobj pun: the sound source is the address of the
                // soundorg member, not the pointer it holds.
                startSound(reinterpret_cast<Mobj*>(&button.soundorg),
                           SfxEnum::Swtchn);
                button = {};
            }
        }
}

//
// Special Stuff that can not be categorized
//
int Line::doDonut()
{
    int secnum = -1;
    int rtn = 0;
    while ((secnum = findSectorFromLineTag(secnum)) >= 0)
    {
        Sector* s1 = &level().sectors[secnum];

        // ALREADY MOVING?  IF SO, KEEP GOING...
        if (s1->specialdata)
            continue;

        rtn = 1;
        Sector* s2 = getNextSector(*s1->lines[0], *s1);
        for (int i = 0; i < s2->linecount; i++)
        {
            if ((!(s2->lines[i]->flags & ML_TWOSIDED))
                || (s2->lines[i]->backsector == s1))
                continue;
            Sector* s3 = s2->lines[i]->backsector;

            //        Spawn rising slime
            FloorMove* floor = new (levelAlloc(sizeof(*floor))) FloorMove {};
            addThinker(*floor);
            s2->specialdata = floor;
            floor->type = FloorType::DonutRaise;
            floor->crush = false;
            floor->direction = 1;
            floor->sector = s2;
            floor->speed = FLOORSPEED / 2;
            floor->texture = s3->floorpic;
            floor->newspecial = 0;
            floor->floordestheight = s3->floorheight;

            //        Spawn lowering donut-hole
            floor = new (levelAlloc(sizeof(*floor))) FloorMove {};
            addThinker(*floor);
            s1->specialdata = floor;
            floor->type = FloorType::LowerFloor;
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
    auto& timer = endLevelTimer();
    auto& specials = activeSpecials();
    auto& surf = animatedSurfaces();
    const auto& session = gameSession();

    // See if -TIMER needs to be used.
    timer.levelTimer = false;

    int i = checkParm("-avg");
    if (i && session.deathmatch)
    {
        timer.levelTimer = true;
        timer.levelTimeCount = 20 * 60 * 35;
    }

    i = checkParm("-timer");
    if (i && session.deathmatch)
    {
        int time = parseInt(myargv()[i + 1]) * 60 * 35;
        timer.levelTimer = true;
        timer.levelTimeCount = time;
    }

    //        Init special SECTORs.
    Sector* sector = level().sectors.data();
    for (i = 0; i < level().sectors.size(); i++, sector++)
    {
        if (!sector->special)
            continue;

        switch (sector->special)
        {
            case 1:
                // FLICKERING LIGHTS
                sector->spawnLightFlash();
                break;

            case 2:
                // STROBE FAST
                sector->spawnStrobeFlash(FASTDARK, 0);
                break;

            case 3:
                // STROBE SLOW
                sector->spawnStrobeFlash(SLOWDARK, 0);
                break;

            case 4:
                // STROBE FAST/DEATH SLIME
                sector->spawnStrobeFlash(FASTDARK, 0);
                sector->special = 4;
                break;

            case 8:
                // GLOWING LIGHT
                sector->spawnGlowingLight();
                break;
            case 9:
                // SECRET SECTOR
                levelStats().totalsecret++;
                break;

            case 10:
                // DOOR CLOSE IN 30 SECONDS
                sector->spawnDoorCloseIn30();
                break;

            case 12:
                // SYNC STROBE SLOW
                sector->spawnStrobeFlash(SLOWDARK, 1);
                break;

            case 13:
                // SYNC STROBE FAST
                sector->spawnStrobeFlash(FASTDARK, 1);
                break;

            case 14:
                // DOOR RAISE IN 5 MINUTES
                sector->spawnDoorRaiseIn5Mins(i);
                break;

            case 17:
                sector->spawnFireFlicker();
                break;
        }
    }

    // Init line EFFECTs
    surf.linespeciallist.clear();
    for (i = 0; i < level().lines.size(); i++)
    {
        switch (level().lines[i].special)
        {
            case 48:
                // EFFECT FIRSTCOL SCROLL+
                surf.linespeciallist.add(&level().lines[i]);
                break;
        }
    }

    // Init other misc stuff
    specials.activeceilings.fill(nullptr);
    specials.activeplats.fill(nullptr);
    specials.buttonlist.fill({});
}
} // namespace Doom
