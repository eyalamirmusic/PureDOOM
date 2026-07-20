// Rewritten out of vanilla p_switch into namespace Doom.
//
// Wall switches and the buttons they arm: build the switch texture-pair list,
// start a button's timed return, flip a switch's texture, and use a special line.
// Doom::useSpecialLine is called by the map-action use code and Doom::changeSwitchTexture
// by the door/floor/lift specials, so p_switch.cpp shims every name; the switch data
// is file-local and buttonlist stays global (p_spec ticks it). Golden-neutral - the
// demos flip switches.

#include "../Host/Platform.h"
#include "Level.h"

#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h"
#include "SimDefs.h"
#include "../Game/SoundData.h"

#include "ActiveSpecials.h"
#include "SwitchList.h"
#include "Switches.h"

// The thinker functions stay global (p_saveg identity); declared so the spawners
#include "../Render/Data.h"
// can store their address.
#include "Ceilings.h"

#include "Lights.h"
#include "Plats.h"
#include "Specials.h"
#include "../Game/Game.h"
#include "../Game/GameVersion.h"
#include "../Game/Sound.h"
#include "../Host/System.h"
#include "Doors.h"
#include "Floors.h"
namespace Doom
{
SwitchListEntry alphSwitchList[] = {
    // Doom shareware episode 1 switches
    {"SW1BRCOM", "SW2BRCOM", 1},
    {"SW1BRN1", "SW2BRN1", 1},
    {"SW1BRN2", "SW2BRN2", 1},
    {"SW1BRNGN", "SW2BRNGN", 1},
    {"SW1BROWN", "SW2BROWN", 1},
    {"SW1COMM", "SW2COMM", 1},
    {"SW1COMP", "SW2COMP", 1},
    {"SW1DIRT", "SW2DIRT", 1},
    {"SW1EXIT", "SW2EXIT", 1},
    {"SW1GRAY", "SW2GRAY", 1},
    {"SW1GRAY1", "SW2GRAY1", 1},
    {"SW1METAL", "SW2METAL", 1},
    {"SW1PIPE", "SW2PIPE", 1},
    {"SW1SLAD", "SW2SLAD", 1},
    {"SW1STARG", "SW2STARG", 1},
    {"SW1STON1", "SW2STON1", 1},
    {"SW1STON2", "SW2STON2", 1},
    {"SW1STONE", "SW2STONE", 1},
    {"SW1STRTN", "SW2STRTN", 1},

    // Doom registered episodes 2&3 switches
    {"SW1BLUE", "SW2BLUE", 2},
    {"SW1CMT", "SW2CMT", 2},
    {"SW1GARG", "SW2GARG", 2},
    {"SW1GSTON", "SW2GSTON", 2},
    {"SW1HOT", "SW2HOT", 2},
    {"SW1LION", "SW2LION", 2},
    {"SW1SATYR", "SW2SATYR", 2},
    {"SW1SKIN", "SW2SKIN", 2},
    {"SW1VINE", "SW2VINE", 2},
    {"SW1WOOD", "SW2WOOD", 2},

    // Doom II switches
    {"SW1PANEL", "SW2PANEL", 3},
    {"SW1ROCK", "SW2ROCK", 3},
    {"SW1MET2", "SW2MET2", 3},
    {"SW1WDMET", "SW2WDMET", 3},
    {"SW1BRIK", "SW2BRIK", 3},
    {"SW1MOD1", "SW2MOD1", 3},
    {"SW1ZIM", "SW2ZIM", 3},
    {"SW1STON6", "SW2STON6", 3},
    {"SW1TEK", "SW2TEK", 3},
    {"SW1MARB", "SW2MARB", 3},
    {"SW1SKULL", "SW2SKULL", 3},

    {"\0", "\0", 0}};

// switchlist/numswitches now live on the Engine (Sim/SwitchList.h, moved by the file-scope-statics
// sweep - REFACTOR.md, Step 5). initSwitchList and changeSwitchTexture each hoist switchList() once
// and reach its members through it, rather than through file-scope reference aliases (REFACTOR.md,
// Step 9 strand (a)).

// Forward declarations so the file's own call order needs no rearranging.
void initSwitchList();
void startButton(Line* line, ButtonWhere w, int texture, int time);
void changeSwitchTexture(Line* line, int useAgain);
bool useSpecialLine(Mobj* thing, Line* line, int side);

void initSwitchList()
{
    int episode = 1;

    const auto& version = gameVersion();
    auto& list = switchList();

    if (version.gamemode == registered)
        episode = 2;
    else if (version.gamemode == commercial)
        episode = 3;

    list.switchlist.clear();

    for (const auto& entry: alphSwitchList)
    {
        // The table ends on a zero-episode row rather than on its own extent.
        if (!entry.episode)
            break;

        if (entry.episode <= episode)
        {
            list.switchlist.add(textureNumForName(entry.name1));
            list.switchlist.add(textureNumForName(entry.name2));
        }
    }
}

//
// Start a button counting down till it turns off.
//
void startButton(Line* line, ButtonWhere w, int texture, int time)
{
    auto& specials = activeSpecials();

    // See if button is already pressed
    for (int i = 0; i < MAXBUTTONS; i++)
    {
        if (specials.buttonlist[i].btimer && specials.buttonlist[i].line == line)
        {
            return;
        }
    }

    for (int i = 0; i < MAXBUTTONS; i++)
    {
        if (!specials.buttonlist[i].btimer)
        {
            specials.buttonlist[i].line = line;
            specials.buttonlist[i].where = w;
            specials.buttonlist[i].btexture = texture;
            specials.buttonlist[i].btimer = time;
            specials.buttonlist[i].soundorg =
                reinterpret_cast<Mobj*>(&line->frontsector->soundorg);
            return;
        }
    }

    fatalError("Error: startButton: no button slots left!");
}

//
// Function that changes wall texture.
// Tell it if switch is ok to use again (1=yes, it's a button).
//
void changeSwitchTexture(Line* line, int useAgain)
{
    auto& specials = activeSpecials();
    auto& list = switchList();

    if (!useAgain)
        line->special = 0;

    int texTop = sides[line->sidenum[0]].toptexture;
    int texMid = sides[line->sidenum[0]].midtexture;
    int texBot = sides[line->sidenum[0]].bottomtexture;

    int sound = sfx_swtchn;

    // EXIT SWITCH?
    if (line->special == 11)
        sound = sfx_swtchx;

    // Not a ranged-for: the index is load-bearing, switchlist[i ^ 1] being the
    // texture this one flips to.
    for (int i = 0; i < list.switchlist.size(); i++)
    {
        if (list.switchlist[i] == texTop)
        {
            startSound(specials.buttonlist.data()->soundorg, sound);
            sides[line->sidenum[0]].toptexture = list.switchlist[i ^ 1];

            if (useAgain)
                startButton(line, top, list.switchlist[i], BUTTONTIME);

            return;
        }
        else
        {
            if (list.switchlist[i] == texMid)
            {
                startSound(specials.buttonlist.data()->soundorg, sound);
                sides[line->sidenum[0]].midtexture = list.switchlist[i ^ 1];

                if (useAgain)
                    startButton(line, middle, list.switchlist[i], BUTTONTIME);

                return;
            }
            else
            {
                if (list.switchlist[i] == texBot)
                {
                    startSound(specials.buttonlist.data()->soundorg, sound);
                    sides[line->sidenum[0]].bottomtexture = list.switchlist[i ^ 1];

                    if (useAgain)
                        startButton(line, bottom, list.switchlist[i], BUTTONTIME);

                    return;
                }
            }
        }
    }
}

//
// useSpecialLine
// Called when a thing uses a special line.
// Only the front sides of lines are usable.
//
bool useSpecialLine(Mobj* thing, Line* line, int side)
{
    // Err...
    // Use the back sides of VERY SPECIAL lines...
    if (side)
    {
        switch (line->special)
        {
            case 124:
                // Sliding door open&close
                // UNUSED?
                break;

            default:
                return false;
                break;
        }
    }

    // Switches that other things can activate.
    if (!thing->player)
    {
        // never open secret doors
        if (line->flags & ML_SECRET)
            return false;

        switch (line->special)
        {
            case 1: // MANUAL DOOR RAISE
            case 32: // MANUAL BLUE
            case 33: // MANUAL RED
            case 34: // MANUAL YELLOW
                break;

            default:
                return false;
                break;
        }
    }

    // do something
    switch (line->special)
    {
        // MANUALS
        case 1: // Vertical Door
        case 26: // Blue Door/Locked
        case 27: // Yellow Door /Locked
        case 28: // Red Door /Locked

        case 31: // Manual door open
        case 32: // Blue locked door open
        case 33: // Red locked door open
        case 34: // Yellow locked door open

        case 117: // Blazing door raise
        case 118: // Blazing door open
            verticalDoor(line, thing);
            break;

            //UNUSED - Door Slide Open&Close
            // case 124:
            // EV_SlidingDoor (line, thing);
            // break;

            // SWITCHES
        case 7:
            // Build Stairs
            if (buildStairs(line, build8))
                changeSwitchTexture(line, 0);
            break;

        case 9:
            // Change Donut
            if (doDonut(line))
                changeSwitchTexture(line, 0);
            break;

        case 11:
            // Exit level
            changeSwitchTexture(line, 0);
            exitLevel();
            break;

        case 14:
            // Raise Floor 32 and change texture
            if (doPlat(line, raiseAndChange, 32))
                changeSwitchTexture(line, 0);
            break;

        case 15:
            // Raise Floor 24 and change texture
            if (doPlat(line, raiseAndChange, 24))
                changeSwitchTexture(line, 0);
            break;

        case 18:
            // Raise Floor to next highest floor
            if (doFloor(line, raiseFloorToNearest))
                changeSwitchTexture(line, 0);
            break;

        case 20:
            // Raise Plat next highest floor and change texture
            if (doPlat(line, raiseToNearestAndChange, 0))
                changeSwitchTexture(line, 0);
            break;

        case 21:
            // PlatDownWaitUpStay
            if (doPlat(line, downWaitUpStay, 0))
                changeSwitchTexture(line, 0);
            break;

        case 23:
            // Lower Floor to Lowest
            if (doFloor(line, lowerFloorToLowest))
                changeSwitchTexture(line, 0);
            break;

        case 29:
            // Raise Door
            if (doDoor(line, door_normal))
                changeSwitchTexture(line, 0);
            break;

        case 41:
            // Lower Ceiling to Floor
            if (doCeiling(line, lowerToFloor))
                changeSwitchTexture(line, 0);
            break;

        case 71:
            // Turbo Lower Floor
            if (doFloor(line, turboLower))
                changeSwitchTexture(line, 0);
            break;

        case 49:
            // Ceiling Crush And Raise
            if (doCeiling(line, crushAndRaise))
                changeSwitchTexture(line, 0);
            break;

        case 50:
            // Close Door
            if (doDoor(line, door_close))
                changeSwitchTexture(line, 0);
            break;

        case 51:
            // Secret EXIT
            changeSwitchTexture(line, 0);
            secretExitLevel();
            break;

        case 55:
            // Raise Floor Crush
            if (doFloor(line, raiseFloorCrush))
                changeSwitchTexture(line, 0);
            break;

        case 101:
            // Raise Floor
            if (doFloor(line, raiseFloor))
                changeSwitchTexture(line, 0);
            break;

        case 102:
            // Lower Floor to Surrounding floor height
            if (doFloor(line, lowerFloor))
                changeSwitchTexture(line, 0);
            break;

        case 103:
            // Open Door
            if (doDoor(line, door_open))
                changeSwitchTexture(line, 0);
            break;

        case 111:
            // Blazing Door Raise (faster than TURBO!)
            if (doDoor(line, blazeRaise))
                changeSwitchTexture(line, 0);
            break;

        case 112:
            // Blazing Door Open (faster than TURBO!)
            if (doDoor(line, blazeOpen))
                changeSwitchTexture(line, 0);
            break;

        case 113:
            // Blazing Door Close (faster than TURBO!)
            if (doDoor(line, blazeClose))
                changeSwitchTexture(line, 0);
            break;

        case 122:
            // Blazing PlatDownWaitUpStay
            if (doPlat(line, blazeDWUS, 0))
                changeSwitchTexture(line, 0);
            break;

        case 127:
            // Build Stairs Turbo 16
            if (buildStairs(line, turbo16))
                changeSwitchTexture(line, 0);
            break;

        case 131:
            // Raise Floor Turbo
            if (doFloor(line, raiseFloorTurbo))
                changeSwitchTexture(line, 0);
            break;

        case 133:
            // BlzOpenDoor BLUE
        case 135:
            // BlzOpenDoor RED
        case 137:
            // BlzOpenDoor YELLOW
            if (doLockedDoor(line, blazeOpen, thing))
                changeSwitchTexture(line, 0);
            break;

        case 140:
            // Raise Floor 512
            if (doFloor(line, raiseFloor512))
                changeSwitchTexture(line, 0);
            break;

            // BUTTONS
        case 42:
            // Close Door
            if (doDoor(line, door_close))
                changeSwitchTexture(line, 1);
            break;

        case 43:
            // Lower Ceiling to Floor
            if (doCeiling(line, lowerToFloor))
                changeSwitchTexture(line, 1);
            break;

        case 45:
            // Lower Floor to Surrounding floor height
            if (doFloor(line, lowerFloor))
                changeSwitchTexture(line, 1);
            break;

        case 60:
            // Lower Floor to Lowest
            if (doFloor(line, lowerFloorToLowest))
                changeSwitchTexture(line, 1);
            break;

        case 61:
            // Open Door
            if (doDoor(line, door_open))
                changeSwitchTexture(line, 1);
            break;

        case 62:
            // PlatDownWaitUpStay
            if (doPlat(line, downWaitUpStay, 1))
                changeSwitchTexture(line, 1);
            break;

        case 63:
            // Raise Door
            if (doDoor(line, door_normal))
                changeSwitchTexture(line, 1);
            break;

        case 64:
            // Raise Floor to ceiling
            if (doFloor(line, raiseFloor))
                changeSwitchTexture(line, 1);
            break;

        case 66:
            // Raise Floor 24 and change texture
            if (doPlat(line, raiseAndChange, 24))
                changeSwitchTexture(line, 1);
            break;

        case 67:
            // Raise Floor 32 and change texture
            if (doPlat(line, raiseAndChange, 32))
                changeSwitchTexture(line, 1);
            break;

        case 65:
            // Raise Floor Crush
            if (doFloor(line, raiseFloorCrush))
                changeSwitchTexture(line, 1);
            break;

        case 68:
            // Raise Plat to next highest floor and change texture
            if (doPlat(line, raiseToNearestAndChange, 0))
                changeSwitchTexture(line, 1);
            break;

        case 69:
            // Raise Floor to next highest floor
            if (doFloor(line, raiseFloorToNearest))
                changeSwitchTexture(line, 1);
            break;

        case 70:
            // Turbo Lower Floor
            if (doFloor(line, turboLower))
                changeSwitchTexture(line, 1);
            break;

        case 114:
            // Blazing Door Raise (faster than TURBO!)
            if (doDoor(line, blazeRaise))
                changeSwitchTexture(line, 1);
            break;

        case 115:
            // Blazing Door Open (faster than TURBO!)
            if (doDoor(line, blazeOpen))
                changeSwitchTexture(line, 1);
            break;

        case 116:
            // Blazing Door Close (faster than TURBO!)
            if (doDoor(line, blazeClose))
                changeSwitchTexture(line, 1);
            break;

        case 123:
            // Blazing PlatDownWaitUpStay
            if (doPlat(line, blazeDWUS, 0))
                changeSwitchTexture(line, 1);
            break;

        case 132:
            // Raise Floor Turbo
            if (doFloor(line, raiseFloorTurbo))
                changeSwitchTexture(line, 1);
            break;

        case 99:
            // BlzOpenDoor BLUE
        case 134:
            // BlzOpenDoor RED
        case 136:
            // BlzOpenDoor YELLOW
            if (doLockedDoor(line, blazeOpen, thing))
                changeSwitchTexture(line, 1);
            break;

        case 138:
            // Light Turn On
            lightTurnOn(line, 255);
            changeSwitchTexture(line, 1);
            break;

        case 139:
            // Light Turn Off
            lightTurnOn(line, 35);
            changeSwitchTexture(line, 1);
            break;
    }

    return true;
}
} // namespace Doom
