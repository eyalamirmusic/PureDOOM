// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:  none
//
//-----------------------------------------------------------------------------

// Rewritten out of vanilla g_game into namespace Doom.
//
// The game controller: ticcmd building, the top-level ticker and responder,
// level load/completion, save/load, and demo record/playback. g_game.cpp shims
// the G_ names. g_game owns the bulk of the core game state (the doomstat.h
// globals plus the key/mouse bindings and demo buffers); rather than scatter ~90
// shared globals into the shim, they stay defined at file scope here (the state
// this file has always owned), above the namespace. The demos drive this file
// end to end, so the simulation AND frame goldens pin it exactly.

#include "../doom_config.h"

#include "../am_map.h"
#include "../d_main.h"
#include "../doomdef.h"
#include "../doomstat.h"
#include "../dstrings.h" // Data.
#include "../f_finale.h"
#include "../hu_stuff.h"
#include "../i_system.h"
#include "../m_argv.h"
#include "../m_menu.h"
#include "../m_misc.h"
#include "../m_random.h"
#include "../p_local.h"
#include "../p_saveg.h"
#include "../p_setup.h"
#include "../p_tick.h"
#include "../r_data.h" // SKY handling - still the wrong place.
#include "../r_sky.h"
#include "../s_sound.h"
#include "../sounds.h"
#include "../st_stuff.h"
#include "../v_video.h" // Needs access to LFB.
#include "../Wad/WadFile.h"
#include "../wi_stuff.h"
#include "../g_game.h"

#include "CorpseQueue.h"
#include "DeferredNewGame.h"
#include "DemoState.h"
#include "EngineParams.h"
#include "Game.h"
#include "GameClock.h"
#include "GameFlow.h"
#include "GameSession.h"
#include "GameVersion.h"
#include "InputConfig.h"
#include "LaunchOptions.h"
#include "IntermissionInfo.h"
#include "LevelStats.h"
#include "MovementSpeeds.h"
#include "NetState.h"
#include "ParTimes.h"
#include "PendingCommands.h"
#include "AmmoLimits.h"
#include "MapSpawns.h"
#include "OverlayState.h"
#include "PlayerState.h"
#include "RefreshFlags.h"
#include "SaveGameState.h"
#include "TiccmdInput.h"
#include "SkyState.h"
#include "TimeDemo.h"

#include "../Render/Data.h"
#include "../Render/ViewWindow.h"
#include "../Render/Draw.h"
#include "../Sim/SaveGame.h"
#include "../Sim/Setup.h"
#include "../Sim/Tick.h"
#include "../UI/Automap.h"
#include "../UI/Finale.h"
#include "../UI/Hud.h"
#include "../UI/Intermission.h"
#include "../UI/StatusBar.h"
#include "Args.h"
#include "DoomMain.h"
#include "../UI/Menu.h"
#include "../UI/MenuSettings.h"
#include <ea_data_structures/Structures/Array.h>

#include "Config.h"
#include "../Host/System.h"
#include "../Render/Main.h"
#include "../Sim/Mobj.h"
#include "../Sim/Movement.h"
#include "Sound.h"
#include "../Sim/Random.h"
#define SAVEGAMESIZE 0x2c000
#define SAVESTRINGSIZE 24
#define MAXPLMOVE (Doom::movementSpeeds().forwardmove[1])
#define TURBOTHRESHOLD 0x32
#define SLOWTURNTICS 6
#define NUMKEYS 256
#define BODYQUESIZE 32
#define VERSIONSIZE 16
#define DEMOMARKER 0x80

// Prototypes for other subsystems' functions.
void Doom::spawnPlayer(Doom::MapThing* mthing);
void Doom::executeSetViewSize();

// The reference aliases that survive here are the ones a header still externs, so other
// translation units read them by their vanilla names; this file itself goes through the owning
// cluster (gameFlow(), gameSession(), ...) and no longer reads any of them. The aliases nothing
// outside this file needed are gone (REFACTOR.md, Step 5).
//
// gameaction and gamestate are a Doom::GameFlow owned by the Engine now; these are references
// onto it (d_event.h / doomstat.h extern them).

// The current game's rules are a Doom::GameSession owned by the Engine now; these (and
// netgame/deathmatch below) are references onto it (REFACTOR.md, Step 5).

// paused (with viewactive/nodrawers/noblit below) is a Doom::RefreshFlags owned by the
// Engine now; these are references onto it (REFACTOR.md, Step 5).

// usergame (with the demo flags below) is a Doom::DemoState owned by the Engine now; these
// are references onto it (REFACTOR.md, Step 5).



// The player roster and view selection is a Doom::PlayerState owned by the Engine now;
// these are references onto it (the arrays as references-to-array) (REFACTOR.md, Step 5).


// The level's progress (levelstarttic + the intermission totals, and leveltime over in
// p_tick) is a Doom::LevelStats owned by the Engine now; these are references onto it.

// The demo flags are a Doom::DemoState owned by the Engine now; these vanilla names are
// references onto it (REFACTOR.md, Step 5). The buffer state (demoname/demobuffer/demo_p/
// demoend/netdemo) needed no alias - nothing outside this file reads it.

// precache is a Doom::EngineParams owned by the Engine now; this is a reference onto it (default
// true - load all graphics at start; REFACTOR.md, Step 5).

// wminfo is a Doom::IntermissionInfo owned by the Engine now; this is a reference onto it
// (REFACTOR.md, Step 5).

// mousemove is the one control binding another translation unit still reads by its vanilla name
// (UI/Menu.cpp externs it); the rest of the Doom::InputConfig bindings are reached through
// inputConfig() here. Config.cpp binds its defaults[] entries to the members at runtime rather
// than capturing their addresses at static-init, which is what unblocked the migration.

// The interior views onto Doom::TiccmdInput's button arrays, offset by one so vanilla's [-1]
// index (an unbound button) stays in bounds.
doom_boolean* mousebuttons = &Doom::ticcmdInput().mousearray[1];
doom_boolean* joybuttons = &Doom::ticcmdInput().joyarray[1];

int savegameslot;
EA::Array<char, 32> savedescription;

// bodyqueslot is a Doom::CorpseQueue owned by the Engine now; this is a reference onto it
// (doomstat.h externs it, bodyque[] alongside it does not need one) (REFACTOR.md, Step 5).

void* statcopy; // for statistics driver

doom_boolean secretexit;

const char* defdemoname;


// Other subsystems' globals this file reads (declared at global scope so the
// namespace code below resolves them to ::, not Doom::).
extern char* player_names[4]; // hu_stuff

namespace Doom
{

// Forward declarations so call order needs no rearranging.
doom_boolean checkDemoStatus();
void readDemoTiccmd(Ticcmd* cmd);
void writeDemoTiccmd(Ticcmd* cmd);
void playerReborn(int player);
void initNewGame(Skill skill, int episode, int map);
void doReborn(int playernum);
void doLoadLevel();
void doNewGame();
void doLoadGame();
void doPlayDemo();
void doCompleted();
void doWorldDone();
void doSaveGame();

int cmdChecksum(Ticcmd* cmd)
{
    int sum = 0;

    for (int i = 0; i < static_cast<int>((sizeof(*cmd) / 4 - 1)); i++)
        sum += (reinterpret_cast<int*>(cmd))[i];

    return sum;
}

//
// buildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer.
// If recording a demo, write it out
//
void buildTiccmd(Ticcmd* cmd)
{
    auto& config = inputConfig();
    auto& input = ticcmdInput();
    auto& speeds = movementSpeeds();
    auto& net = netState();
    auto& pending = pendingCommands();

    doom_boolean strafe;
    doom_boolean bstrafe;
    int speed;
    int tspeed;
    int forward;
    int side;

    Ticcmd* base;

    base = baseTiccmd(); // empty, or external driver
    doom_memcpy(cmd, base, sizeof(*cmd));

    cmd->consistancy =
        net.consistancy[playerState().consoleplayer][net.maketic % BACKUPTICS];

    strafe = input.gamekeydown[config.key_strafe]
             || mousebuttons[config.mousebstrafe] || joybuttons[config.joybstrafe];

    doom_boolean running =
        config.always_run ? (input.gamekeydown[config.key_speed] ? false : true)
                          : (input.gamekeydown[config.key_speed] ? true : false);
    speed = running || joybuttons[config.joybspeed];

    forward = side = 0;

    // use two stage accelerative turning
    // on the keyboard and joystick
    if (input.joyxmove < 0 || input.joyxmove > 0
        || input.gamekeydown[config.key_right] || input.gamekeydown[config.key_left])
        input.turnheld += net.ticdup;
    else
        input.turnheld = 0;

    if (input.turnheld < SLOWTURNTICS)
        tspeed = 2; // slow turn
    else
        tspeed = speed;

    // let movement keys cancel each other out
    if (strafe)
    {
        if (input.gamekeydown[config.key_right])
        {
            side += speeds.sidemove[speed];
        }
        if (input.gamekeydown[config.key_left])
        {
            side -= speeds.sidemove[speed];
        }
        if (input.joyxmove > 0)
            side += speeds.sidemove[speed];
        if (input.joyxmove < 0)
            side -= speeds.sidemove[speed];
    }
    else
    {
        if (input.gamekeydown[config.key_right])
            cmd->angleturn -= speeds.angleturn[tspeed];
        if (input.gamekeydown[config.key_left])
            cmd->angleturn += speeds.angleturn[tspeed];
        if (input.joyxmove > 0)
            cmd->angleturn -= speeds.angleturn[tspeed];
        if (input.joyxmove < 0)
            cmd->angleturn += speeds.angleturn[tspeed];
    }

    if (input.gamekeydown[config.key_up])
    {
        forward += speeds.forwardmove[speed];
    }
    if (input.gamekeydown[config.key_down])
    {
        forward -= speeds.forwardmove[speed];
    }
    if (input.joyymove < 0)
        forward += speeds.forwardmove[speed];
    if (input.joyymove > 0)
        forward -= speeds.forwardmove[speed];
    if (input.gamekeydown[config.key_straferight])
        side += speeds.sidemove[speed];
    if (input.gamekeydown[config.key_strafeleft])
        side -= speeds.sidemove[speed];

    // buttons
    cmd->chatchar = Doom::dequeueChatChar();

    if (input.gamekeydown[config.key_fire] || mousebuttons[config.mousebfire]
        || joybuttons[config.joybfire])
        cmd->buttons |= BT_ATTACK;

    if (input.gamekeydown[config.key_use] || joybuttons[config.joybuse])
    {
        cmd->buttons |= BT_USE;
        // clear double clicks if hit use button
        input.dclicks = 0;
    }

    // chainsaw overrides
    for (int i = 0; i < NUMWEAPONS - 1; i++)
        if (input.gamekeydown['1' + i])
        {
            cmd->buttons |= BT_CHANGE;
            cmd->buttons |= i << BT_WEAPONSHIFT;
            break;
        }

    // mouse
    if (mousebuttons[config.mousebforward])
        forward += speeds.forwardmove[speed];

    // forward double click
    if (mousebuttons[config.mousebforward] != input.dclickstate
        && input.dclicktime > 1)
    {
        input.dclickstate = mousebuttons[config.mousebforward];
        if (input.dclickstate)
            input.dclicks++;
        if (input.dclicks == 2)
        {
            cmd->buttons |= BT_USE;
            input.dclicks = 0;
        }
        else
            input.dclicktime = 0;
    }
    else
    {
        input.dclicktime += net.ticdup;
        if (input.dclicktime > 20)
        {
            input.dclicks = 0;
            input.dclickstate = 0;
        }
    }

    // strafe double click
    bstrafe = mousebuttons[config.mousebstrafe] || joybuttons[config.joybstrafe];
    if (bstrafe != input.dclickstate2 && input.dclicktime2 > 1)
    {
        input.dclickstate2 = bstrafe;
        if (input.dclickstate2)
            input.dclicks2++;
        if (input.dclicks2 == 2)
        {
            cmd->buttons |= BT_USE;
            input.dclicks2 = 0;
        }
        else
            input.dclicktime2 = 0;
    }
    else
    {
        input.dclicktime2 += net.ticdup;
        if (input.dclicktime2 > 20)
        {
            input.dclicks2 = 0;
            input.dclickstate2 = 0;
        }
    }

    if (config.mousemove)
        forward += input.mousey;
    if (strafe)
        side += input.mousex * 2;
    else
        cmd->angleturn -= input.mousex * 0x8;

    input.mousex = input.mousey = 0;

    if (forward > MAXPLMOVE)
        forward = MAXPLMOVE;
    else if (forward < -MAXPLMOVE)
        forward = -MAXPLMOVE;
    if (side > MAXPLMOVE)
        side = MAXPLMOVE;
    else if (side < -MAXPLMOVE)
        side = -MAXPLMOVE;

    cmd->forwardmove += forward;
    cmd->sidemove += side;

    // special buttons
    if (pending.sendpause)
    {
        pending.sendpause = false;
        cmd->buttons = BT_SPECIAL | BTS_PAUSE;
    }

    if (pending.sendsave)
    {
        pending.sendsave = false;
        cmd->buttons = BT_SPECIAL | BTS_SAVEGAME | (savegameslot << BTS_SAVESHIFT);
    }
}

//
// doLoadLevel
//
void doLoadLevel()
{
    auto& sky = skyState();
    auto& session = gameSession();
    auto& flow = gameFlow();
    auto& players_ = playerState();
    auto& input = ticcmdInput();

    // Set the sky map.
    // First thing, we have a dummy sky texture name,
    //  a flat. The data is in the WAD only because
    //  we look for an actual index, instead of simply
    //  setting one.
    sky.skyflatnum = Doom::flatNumForName(SKYFLATNAME);

    // DOOM determines the sky texture to be used
    // depending on the current episode, and the game version.
    const auto mode = gameVersion().gamemode;
    if ((mode == commercial)
        || (static_cast<int>(mode) == static_cast<int>(pack_tnt))
        || (static_cast<int>(mode) == static_cast<int>(pack_plut)))
    {
        sky.skytexture = Doom::textureNumForName("SKY3");
        if (session.gamemap < 12)
            sky.skytexture = Doom::textureNumForName("SKY1");
        else if (session.gamemap < 21)
            sky.skytexture = Doom::textureNumForName("SKY2");
    }

    levelStats().levelstarttic = gameClock().gametic; // for time calculation

    if (flow.wipegamestate == GS_LEVEL)
        flow.wipegamestate = static_cast<GameState>((-1)); // force a wipe

    flow.gamestate = GS_LEVEL;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (players_.playeringame[i] && players_.players[i].playerstate == PST_DEAD)
            players_.players[i].playerstate = PST_REBORN;
        doom_memset(players_.players[i].frags, 0, sizeof(players_.players[i].frags));
    }

    Doom::setupLevel(session.gameepisode, session.gamemap, 0, session.gameskill);
    players_.displayplayer = players_.consoleplayer; // view the guy you are playing
    timeDemo().starttime = currentTic();
    flow.gameaction = ga_nothing;

    // clear cmd building stuff
    doom_memset(input.gamekeydown, 0, sizeof(input.gamekeydown));
    input.joyxmove = input.joyymove = 0;
    input.mousex = input.mousey = 0;
    pendingCommands().sendpause = pendingCommands().sendsave =
        refreshFlags().paused = false;
    doom_memset(mousebuttons, 0, sizeof(*mousebuttons) * 3);
    doom_memset(joybuttons, 0, sizeof(*joybuttons) * 4);
}

//
// gameResponder
// Get info needed to make ticcmd_ts for the players.
//
doom_boolean gameResponder(Event* ev)
{
    auto& flow = gameFlow();
    auto& demo = demoState();
    auto& players_ = playerState();
    auto& input = ticcmdInput();

    // allow spy mode changes even during the demo
    if (flow.gamestate == GS_LEVEL && ev->type == ev_keydown && ev->data1 == KEY_F12
        && (demo.singledemo || !gameSession().deathmatch))
    {
        // spy mode
        do
        {
            players_.displayplayer++;
            if (players_.displayplayer == MAXPLAYERS)
                players_.displayplayer = 0;
        } while (!players_.playeringame[players_.displayplayer]
                 && players_.displayplayer != players_.consoleplayer);
        return true;
    }

    // any other key pops up menu if in demos
    if (flow.gameaction == ga_nothing && !demo.singledemo
        && (demo.demoplayback || flow.gamestate == GS_DEMOSCREEN))
    {
        if (ev->type == ev_keydown || (ev->type == ev_mouse && ev->data1)
            || (ev->type == ev_joystick && ev->data1))
        {
            startControlPanel();
            return true;
        }
        return false;
    }

    if (flow.gamestate == GS_LEVEL)
    {
#if 0 
        if (devparm && ev->type == ev_keydown && ev->data1 == ';')
        {
            deathMatchSpawnPlayer(0);
            return true;
        }
#endif
        if (Doom::hudResponder(ev))
            return true; // chat ate the event
        if (Doom::statusBarResponder(ev))
            return true; // status window ate it
        if (Doom::automapResponder(ev))
            return true; // automap ate it
    }

    if (flow.gamestate == GS_FINALE)
    {
        if (Doom::finaleResponder(ev))
            return true; // finale ate the event
    }

    switch (ev->type)
    {
        case ev_keydown:
            if (ev->data1 == KEY_PAUSE)
            {
                pendingCommands().sendpause = true;
                return true;
            }
            if (ev->data1 < NUMKEYS)
                input.gamekeydown[ev->data1] = true;
            return true; // eat key down events

        case ev_keyup:
            if (ev->data1 < NUMKEYS)
                input.gamekeydown[ev->data1] = false;
            return false; // always let key up events filter down

        case ev_mouse:
        {
            const auto sensitivity = menuSettings().mouseSensitivity;
            mousebuttons[0] = ev->data1 & 1;
            mousebuttons[1] = ev->data1 & 2;
            mousebuttons[2] = ev->data1 & 4;
            input.mousex = ev->data2 * (sensitivity + 5) / 10;
            input.mousey = ev->data3 * (sensitivity + 5) / 10;
            return true; // eat events
        }

        case ev_joystick:
            joybuttons[0] = ev->data1 & 1;
            joybuttons[1] = ev->data1 & 2;
            joybuttons[2] = ev->data1 & 4;
            joybuttons[3] = ev->data1 & 8;
            input.joyxmove = ev->data2;
            input.joyymove = ev->data3;
            return true; // eat events

        default:
            break;
    }

    return false;
}

//
// gameTicker
// Make ticcmd_ts for the players.
//
void gameTicker()
{
    auto& flow = gameFlow();
    auto& clock = gameClock();
    auto& net = netState();
    auto& demo = demoState();
    auto& players_ = playerState();

    int buf;
    Ticcmd* cmd;

    // do player reborns if needed
    for (int i = 0; i < MAXPLAYERS; i++)
        if (players_.playeringame[i]
            && players_.players[i].playerstate == PST_REBORN)
            doReborn(i);

    // do things to change the game state
    while (flow.gameaction != ga_nothing)
    {
        switch (flow.gameaction)
        {
            case ga_loadlevel:
                doLoadLevel();
                break;
            case ga_newgame:
                doNewGame();
                break;
            case ga_loadgame:
                doLoadGame();
                break;
            case ga_savegame:
                doSaveGame();
                break;
            case ga_playdemo:
                doPlayDemo();
                break;
            case ga_completed:
                doCompleted();
                break;
            case ga_victory:
                Doom::startFinale();
                break;
            case ga_worlddone:
                doWorldDone();
                break;
            case ga_screenshot:
                Doom::writeScreenshot();
                flow.gameaction = ga_nothing;
                break;
            case ga_nothing:
                break;
        }
    }

    // get commands, check consistancy,
    // and build new consistancy check
    buf = (clock.gametic / net.ticdup) % BACKUPTICS;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (players_.playeringame[i])
        {
            cmd = &players_.players[i].cmd;

            doom_memcpy(cmd, &net.netcmds[i][buf], sizeof(Ticcmd));

            if (demo.demoplayback)
                readDemoTiccmd(cmd);
            if (demo.demorecording)
                writeDemoTiccmd(cmd);

            // check for turbo cheats
            if (cmd->forwardmove > TURBOTHRESHOLD && !(clock.gametic & 31)
                && ((clock.gametic >> 5) & 3) == i)
            {
                static EA::Array<char, 80> turbomessage;
                //doom_sprintf(turbomessage, "%s is turbo!", player_names[i]);
                doom_strcpy(turbomessage.data(), player_names[i]);
                doom_concat(turbomessage.data(), " is turbo!");
                players_.players[players_.consoleplayer].message =
                    turbomessage.data();
            }

            if (gameSession().netgame && !demo.netdemo
                && !(clock.gametic % net.ticdup))
            {
                if (clock.gametic > BACKUPTICS
                    && net.consistancy[i][buf] != cmd->consistancy)
                {
                    //fatalError("Error: consistency failure (%i should be %i)",
                    //        cmd->consistancy, consistancy[i][buf]);

                    doom_strcpy(error_buf, "Error: consistency failure (");
                    doom_concat(error_buf, doom_itoa(cmd->consistancy, 10));
                    doom_concat(error_buf, " should be ");
                    doom_concat(error_buf, doom_itoa(net.consistancy[i][buf], 10));
                    doom_concat(error_buf, ")");
                    fatalError(error_buf);
                }
                if (players_.players[i].mo)
                    net.consistancy[i][buf] = players_.players[i].mo->x;
                else
                    net.consistancy[i][buf] = randomness().menuIndex;
            }
        }
    }

    // check for special buttons
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (players_.playeringame[i])
        {
            if (players_.players[i].cmd.buttons & BT_SPECIAL)
            {
                switch (players_.players[i].cmd.buttons & BT_SPECIALMASK)
                {
                    case BTS_PAUSE:
                        refreshFlags().paused ^= 1;
                        if (refreshFlags().paused)
                            Doom::pauseSound();
                        else
                            Doom::resumeSound();
                        break;

                    case BTS_SAVEGAME:
                        if (!savedescription[0])
                            doom_strcpy(savedescription.data(), "NET GAME");
                        savegameslot =
                            (players_.players[i].cmd.buttons & BTS_SAVEMASK)
                            >> BTS_SAVESHIFT;
                        flow.gameaction = ga_savegame;
                        break;
                }
            }
        }
    }

    // do main actions
    switch (flow.gamestate)
    {
        case GS_LEVEL:
            Doom::ticker();
            Doom::statusBarTicker();
            Doom::automapTicker();
            Doom::hudTicker();
            break;

        case GS_INTERMISSION:
            Doom::intermissionTicker();
            break;

        case GS_FINALE:
            Doom::finaleTicker();
            break;

        case GS_DEMOSCREEN:
            Doom::pageTicker();
            break;
    }
}

//
// PLAYER STRUCTURE FUNCTIONS
// also see Doom::spawnPlayer in P_Things
//

//
// initPlayer
// Called at the start.
// Called by the game initialization functions.
//
void initPlayer(int player)
{
    // clear everything else to defaults
    playerReborn(player);
}

//
// playerFinishLevel
// Can when a player completes a level.
//
void playerFinishLevel(int player)
{
    Player* p;

    p = &playerState().players[player];

    doom_memset(p->powers, 0, sizeof(p->powers));
    doom_memset(p->cards, 0, sizeof(p->cards));
    p->mo->flags &= ~MF_SHADOW; // cancel invisibility
    p->extralight = 0; // cancel gun flashes
    p->fixedcolormap = 0; // cancel ir gogles
    p->damagecount = 0; // no palette changes
    p->bonuscount = 0;
}

//
// playerReborn
// Called after a player dies
// almost everything is cleared and initialized
//
void playerReborn(int player)
{
    auto& ammo = ammoLimits();
    auto& players_ = playerState();

    Player* p;
    EA::Array<int, MAXPLAYERS> frags;
    int killcount;
    int itemcount;
    int secretcount;

    doom_memcpy(frags.data(), players_.players[player].frags, sizeof(frags));
    killcount = players_.players[player].killcount;
    itemcount = players_.players[player].itemcount;
    secretcount = players_.players[player].secretcount;

    p = &players_.players[player];
    doom_memset(p, 0, sizeof(*p));

    doom_memcpy(players_.players[player].frags,
                frags.data(),
                sizeof(players_.players[player].frags));
    players_.players[player].killcount = killcount;
    players_.players[player].itemcount = itemcount;
    players_.players[player].secretcount = secretcount;

    p->usedown = p->attackdown = true; // don't do anything immediately
    p->playerstate = PST_LIVE;
    p->health = MAXHEALTH;
    p->readyweapon = p->pendingweapon = wp_pistol;
    p->weaponowned[wp_fist] = true;
    p->weaponowned[wp_pistol] = true;
    p->ammo[am_clip] = 50;

    for (int i = 0; i < NUMAMMO; i++)
        p->maxammo[i] = ammo.maxammo[i];
}

//
// checkSpot
// Returns false if the player cannot be respawned
// at the given MapThing spot
// because something is occupying it
//

doom_boolean checkSpot(int playernum, MapThing* mthing)
{
    auto& players_ = playerState();
    auto& corpses = corpseQueue();

    fixed_t x;
    fixed_t y;
    SubSector* ss;
    unsigned an;
    Mobj* mo;

    if (!players_.players[playernum].mo)
    {
        // first spawn of level, before corpses
        for (int i = 0; i < playernum; i++)
            if (players_.players[i].mo->x == mthing->x << FRACBITS
                && players_.players[i].mo->y == mthing->y << FRACBITS)
                return false;
        return true;
    }

    x = mthing->x << FRACBITS;
    y = mthing->y << FRACBITS;

    if (!Doom::checkPosition(players_.players[playernum].mo, x, y))
        return false;

    // flush an old corpse if needed
    if (corpses.bodyqueslot >= BODYQUESIZE)
        Doom::removeMobj(corpses.bodyque[corpses.bodyqueslot % BODYQUESIZE]);
    corpses.bodyque[corpses.bodyqueslot % BODYQUESIZE] =
        players_.players[playernum].mo;
    corpses.bodyqueslot++;

    // spawn a teleport fog
    ss = Doom::pointInSubsector(x, y);
    an = (ANG45 * (mthing->angle / 45)) >> ANGLETOFINESHIFT;

    mo = Doom::spawnMobj(x + 20 * finecosine[an],
                         y + 20 * finesine[an],
                         ss->sector->floorheight,
                         MT_TFOG);

    if (players_.players[players_.consoleplayer].viewz != 1)
        Doom::startSound(mo, sfx_telept); // don't start sound on first frame

    return true;
}

//
// deathMatchSpawnPlayer
// Spawns a player at one of the random death match spots
// called at level load and each death
//
void deathMatchSpawnPlayer(int playernum)
{
    auto& spawns = mapSpawns();

    int i;
    int selections;

    selections = static_cast<int>((spawns.deathmatch_p - spawns.deathmatchstarts));
    if (selections < 4)
    {
        //fatalError("Error: Only %i deathmatch spots, 4 required", selections);

        doom_strcpy(error_buf, "Error: Only ");
        doom_concat(error_buf, doom_itoa(selections, 10));
        doom_concat(error_buf, " deathmatch spots, 4 required");
        fatalError(error_buf);
    }

    for (int j = 0; j < 20; j++)
    {
        i = Doom::randomness().forPlay() % selections;
        if (checkSpot(playernum, &spawns.deathmatchstarts[i]))
        {
            spawns.deathmatchstarts[i].type = playernum + 1;
            Doom::spawnPlayer(&spawns.deathmatchstarts[i]);
            return;
        }
    }

    // no good spot, so the player will probably get stuck
    Doom::spawnPlayer(&spawns.playerstarts[playernum]);
}

//
// doReborn
//
void doReborn(int playernum)
{
    auto& spawns = mapSpawns();
    auto& session = gameSession();

    if (!session.netgame)
    {
        // reload the level from scratch
        gameFlow().gameaction = ga_loadlevel;
    }
    else
    {
        // respawn at the start

        // first dissasociate the corpse
        playerState().players[playernum].mo->player = nullptr;

        // spawn at random spot if in death match
        if (session.deathmatch)
        {
            deathMatchSpawnPlayer(playernum);
            return;
        }

        if (checkSpot(playernum, &spawns.playerstarts[playernum]))
        {
            Doom::spawnPlayer(&spawns.playerstarts[playernum]);
            return;
        }

        // try to spawn at one of the other players spots
        for (int i = 0; i < MAXPLAYERS; i++)
        {
            if (checkSpot(playernum, &spawns.playerstarts[i]))
            {
                spawns.playerstarts[i].type = playernum + 1; // fake as other player
                Doom::spawnPlayer(&spawns.playerstarts[i]);
                spawns.playerstarts[i].type = i + 1; // restore
                return;
            }
            // he's going to be inside something.  Too bad.
        }
        Doom::spawnPlayer(&spawns.playerstarts[playernum]);
    }
}

void takeScreenshot()
{
    gameFlow().gameaction = ga_screenshot;
}

//
// doCompleted
//
void exitLevel()
{
    secretexit = false;
    gameFlow().gameaction = ga_completed;
}

// Here's for the german edition.
void secretExitLevel()
{
    // IF NO WOLF3D LEVELS, NO SECRET EXIT!
    if ((gameVersion().gamemode == commercial) && (Doom::wad().find("map31") < 0))
        secretexit = false;
    else
        secretexit = true;
    gameFlow().gameaction = ga_completed;
}

void doCompleted()
{
    auto& overlay = overlayState();
    auto& flow = gameFlow();
    auto& session = gameSession();
    auto& players_ = playerState();
    auto& stats = levelStats();
    auto& par = parTimes();
    auto& wminfo_ = intermissionInfo().wminfo;
    const auto mode = gameVersion().gamemode;

    flow.gameaction = ga_nothing;

    for (int i = 0; i < MAXPLAYERS; i++)
        if (players_.playeringame[i])
            playerFinishLevel(i); // take away cards and stuff

    if (overlay.automapactive)
        Doom::stopAutomap();

    if (mode != commercial)
        switch (session.gamemap)
        {
            case 8:
                flow.gameaction = ga_victory;
                return;
            case 9:
                for (int i = 0; i < MAXPLAYERS; i++)
                    players_.players[i].didsecret = true;
                break;
        }

    if ((session.gamemap == 8) && (mode != commercial))
    {
        // victory
        flow.gameaction = ga_victory;
        return;
    }

    if ((session.gamemap == 9) && (mode != commercial))
    {
        // exit secret level
        for (int i = 0; i < MAXPLAYERS; i++)
            players_.players[i].didsecret = true;
    }

    wminfo_.didsecret = players_.players[players_.consoleplayer].didsecret;
    wminfo_.epsd = session.gameepisode - 1;
    wminfo_.last = session.gamemap - 1;

    // wminfo.next is 0 biased, unlike gamemap
    if (mode == commercial)
    {
        if (secretexit)
            switch (session.gamemap)
            {
                case 15:
                    wminfo_.next = 30;
                    break;
                case 31:
                    wminfo_.next = 31;
                    break;
            }
        else
            switch (session.gamemap)
            {
                case 31:
                case 32:
                    wminfo_.next = 15;
                    break;
                default:
                    wminfo_.next = session.gamemap;
            }
    }
    else
    {
        if (secretexit)
            wminfo_.next = 8; // go to secret level
        else if (session.gamemap == 9)
        {
            // returning from secret level
            switch (session.gameepisode)
            {
                case 1:
                    wminfo_.next = 3;
                    break;
                case 2:
                    wminfo_.next = 5;
                    break;
                case 3:
                    wminfo_.next = 6;
                    break;
                case 4:
                    wminfo_.next = 2;
                    break;
            }
        }
        else
            wminfo_.next = session.gamemap; // go to next level
    }

    wminfo_.maxkills = stats.totalkills;
    wminfo_.maxitems = stats.totalitems;
    wminfo_.maxsecret = stats.totalsecret;
    wminfo_.maxfrags = 0;
    if (mode == commercial)
        wminfo_.partime = 35 * par.cpars[session.gamemap - 1];
    else
        wminfo_.partime = 35 * par.pars[session.gameepisode][session.gamemap];
    wminfo_.pnum = players_.consoleplayer;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        wminfo_.plyr[i].in = players_.playeringame[i];
        wminfo_.plyr[i].skills = players_.players[i].killcount;
        wminfo_.plyr[i].sitems = players_.players[i].itemcount;
        wminfo_.plyr[i].ssecret = players_.players[i].secretcount;
        wminfo_.plyr[i].stime = stats.leveltime;
        doom_memcpy(wminfo_.plyr[i].frags,
                    players_.players[i].frags,
                    sizeof(wminfo_.plyr[i].frags));
    }

    flow.gamestate = GS_INTERMISSION;
    refreshFlags().viewactive = false;
    overlay.automapactive = false;

    if (statcopy)
        doom_memcpy(statcopy, &wminfo_, sizeof(wminfo_));

    Doom::startIntermission(&wminfo_);
}

//
// worldDone
//
void worldDone()
{
    gameFlow().gameaction = ga_worlddone;

    if (secretexit)
    {
        auto& players_ = playerState();
        players_.players[players_.consoleplayer].didsecret = true;
    }

    if (gameVersion().gamemode == commercial)
    {
        switch (gameSession().gamemap)
        {
            case 15:
            case 31:
                if (!secretexit)
                    break;
            case 6:
            case 11:
            case 20:
            case 30:
                Doom::startFinale();
                break;
        }
    }
}

void doWorldDone()
{
    auto& flow = gameFlow();

    flow.gamestate = GS_LEVEL;
    gameSession().gamemap = intermissionInfo().wminfo.next + 1;
    doLoadLevel();
    flow.gameaction = ga_nothing;
    refreshFlags().viewactive = true;
}

//
// G_InitFromSavegame
// Can be called by the startup code or the menu task.
//

void loadGame(char* name)
{
    doom_strcpy(saveGameState().name, name);
    gameFlow().gameaction = ga_loadgame;
}

void doLoadGame()
{
    auto& save = saveGameState();
    auto& session = gameSession();
    auto& players_ = playerState();

    int a, b, c;
    EA::Array<char, VERSIONSIZE> vcheck;

    gameFlow().gameaction = ga_nothing;

    Doom::readFile(save.name, &save.buffer);
    save.cursor = save.buffer + SAVESTRINGSIZE;

    // skip the description field
    doom_memset(vcheck.data(), 0, sizeof(vcheck));
    //doom_sprintf(vcheck, "version %i", VERSION);
    doom_strcpy(vcheck.data(), "version ");
    doom_concat(vcheck.data(), doom_itoa(VERSION, 10));
    if (doom_strcmp(reinterpret_cast<const char*>(save.cursor),
                    const_cast<const char*>(vcheck.data())))
        return; // bad version
    save.cursor += VERSIONSIZE;

    session.gameskill = static_cast<Skill>((*save.cursor++));
    session.gameepisode = *save.cursor++;
    session.gamemap = *save.cursor++;
    for (int i = 0; i < MAXPLAYERS; i++)
        players_.playeringame[i] = *save.cursor++;

    // load a base level
    initNewGame(session.gameskill, session.gameepisode, session.gamemap);

    // get the times
    a = *save.cursor++;
    b = *save.cursor++;
    c = *save.cursor++;
    levelStats().leveltime = (a << 16) + (b << 8) + c;

    // dearchive all the modifications
    Doom::unArchivePlayers();
    Doom::unArchiveWorld();
    Doom::unArchiveThinkers();
    Doom::unArchiveSpecials();

    if (*save.cursor != 0x1d)
        fatalError("Error: Bad savegame");

    // done
    doom_free(save.buffer);

    if (viewWindow().setsizeneeded)
        Doom::executeSetViewSize();

    // draw the pattern into the back screen
    Doom::fillBackScreen();
}

//
// saveGame
// Called by the menu task.
// Description is a 24 byte text string
//
void saveGame(int slot, char* description)
{
    savegameslot = slot;
    doom_strcpy(savedescription.data(), description);
    pendingCommands().sendsave = true;
}

void doSaveGame()
{
    auto& save = saveGameState();
    auto& stats = levelStats();
    auto& session = gameSession();
    auto& players_ = playerState();

    EA::Array<char, 100> name;
    EA::Array<char, VERSIONSIZE> name2;
    char* description;
    int length;

#if 0
    if (Doom::checkParm("-cdrom"))
        doom_sprintf(name, "c:\\doomdata\\"SAVEGAMENAME"%d.dsg", savegameslot);
    else
#endif
    {
        //doom_sprintf(name, SAVEGAMENAME"%d.dsg", savegameslot);
        doom_strcpy(name.data(), SAVEGAMENAME);
        doom_concat(name.data(), doom_itoa(savegameslot, 10));
        doom_concat(name.data(), ".dsg");
    }
    description = savedescription.data();

    save.cursor = save.buffer = screens[1] + 0x4000;

    doom_memcpy(save.cursor, description, SAVESTRINGSIZE);
    save.cursor += SAVESTRINGSIZE;
    doom_memset(name2.data(), 0, sizeof(name2));
    //doom_sprintf(name2, "version %i", VERSION);
    doom_strcpy(name2.data(), "version ");
    doom_concat(name2.data(), doom_itoa(VERSION, 10));
    doom_memcpy(save.cursor, name2.data(), VERSIONSIZE);
    save.cursor += VERSIONSIZE;

    *save.cursor++ = session.gameskill;
    *save.cursor++ = session.gameepisode;
    *save.cursor++ = session.gamemap;
    for (int i = 0; i < MAXPLAYERS; i++)
        *save.cursor++ = players_.playeringame[i];
    *save.cursor++ = stats.leveltime >> 16;
    *save.cursor++ = stats.leveltime >> 8;
    *save.cursor++ = stats.leveltime;

    Doom::archivePlayers();
    Doom::archiveWorld();
    Doom::archiveThinkers();
    Doom::archiveSpecials();

    *save.cursor++ = 0x1d; // consistancy marker

    length = static_cast<int>((save.cursor - save.buffer));
    if (length > SAVEGAMESIZE)
        fatalError("Error: Savegame buffer overrun");
    Doom::writeFile(name.data(), save.buffer, length);
    gameFlow().gameaction = ga_nothing;
    savedescription[0] = 0;

    players_.players[players_.consoleplayer].message = GGSAVED;

    // draw the pattern into the back screen
    Doom::fillBackScreen();
}

//
// initNewGame
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayer, playeringame[] should be set.
//

void deferInitNew(Skill skill, int episode, int map)
{
    auto& deferred = deferredNewGame();

    deferred.d_skill = skill;
    deferred.d_episode = episode;
    deferred.d_map = map;
    gameFlow().gameaction = ga_newgame;
}

void doNewGame()
{
    auto& demo = demoState();
    auto& session = gameSession();
    auto& players_ = playerState();
    auto& opts = launchOptions();
    auto& deferred = deferredNewGame();

    demo.demoplayback = false;
    demo.netdemo = false;
    session.netgame = false;
    session.deathmatch = false;
    players_.playeringame[1] = players_.playeringame[2] = players_.playeringame[3] =
        0;
    opts.respawnparm = false;
    opts.fastparm = false;
    opts.nomonsters = false;
    players_.consoleplayer = 0;
    initNewGame(deferred.d_skill, deferred.d_episode, deferred.d_map);
    gameFlow().gameaction = ga_nothing;
}

void initNewGame(Skill skill, int episode, int map)
{
    auto& sky = skyState();
    auto& refresh = refreshFlags();
    auto& session = gameSession();
    auto& opts = launchOptions();
    const auto mode = gameVersion().gamemode;

    if (refresh.paused)
    {
        refresh.paused = false;
        Doom::resumeSound();
    }

    if (skill > sk_nightmare)
        skill = sk_nightmare;

    // This was quite messy with SPECIAL and commented parts.
    // Supposedly hacks to make the latest edition work.
    // It might not work properly.
    if (episode < 1)
        episode = 1;

    if (mode == retail)
    {
        if (episode > 4)
            episode = 4;
    }
    else if (mode == shareware)
    {
        if (episode > 1)
            episode = 1; // only start episode 1 on shareware
    }
    else
    {
        if (episode > 3)
            episode = 3;
    }

    if (map < 1)
        map = 1;

    if ((map > 9) && (mode != commercial))
        map = 9;

    Doom::randomness().clear();

    if (skill == sk_nightmare || opts.respawnparm)
        session.respawnmonsters = true;
    else
        session.respawnmonsters = false;

    if (opts.fastparm
        || (skill == sk_nightmare && session.gameskill != sk_nightmare))
    {
        for (int i = S_SARG_RUN1; i <= S_SARG_PAIN2; i++)
            states[i].tics >>= 1;
        mobjinfo[MT_BRUISERSHOT].speed = 20 * FRACUNIT;
        mobjinfo[MT_HEADSHOT].speed = 20 * FRACUNIT;
        mobjinfo[MT_TROOPSHOT].speed = 20 * FRACUNIT;
    }
    else if (skill != sk_nightmare && session.gameskill == sk_nightmare)
    {
        for (int i = S_SARG_RUN1; i <= S_SARG_PAIN2; i++)
            states[i].tics <<= 1;
        mobjinfo[MT_BRUISERSHOT].speed = 15 * FRACUNIT;
        mobjinfo[MT_HEADSHOT].speed = 10 * FRACUNIT;
        mobjinfo[MT_TROOPSHOT].speed = 10 * FRACUNIT;
    }

    // force players to be initialized upon first level load
    for (int i = 0; i < MAXPLAYERS; i++)
        playerState().players[i].playerstate = PST_REBORN;

    demoState().usergame = true; // will be set false if a demo
    refresh.paused = false;
    demoState().demoplayback = false;
    overlayState().automapactive = false;
    refresh.viewactive = true;
    session.gameepisode = episode;
    session.gamemap = map;
    session.gameskill = skill;

    refresh.viewactive = true;

    // set the sky map for the episode
    if (mode == commercial)
    {
        sky.skytexture = Doom::textureNumForName("SKY3");
        if (session.gamemap < 12)
            sky.skytexture = Doom::textureNumForName("SKY1");
        else if (session.gamemap < 21)
            sky.skytexture = Doom::textureNumForName("SKY2");
    }
    else
        switch (episode)
        {
            case 1:
                sky.skytexture = Doom::textureNumForName("SKY1");
                break;
            case 2:
                sky.skytexture = Doom::textureNumForName("SKY2");
                break;
            case 3:
                sky.skytexture = Doom::textureNumForName("SKY3");
                break;
            case 4: // Special Edition sky
                sky.skytexture = Doom::textureNumForName("SKY4");
                break;
        }

    doLoadLevel();
}

//
// DEMO RECORDING
//

void readDemoTiccmd(Ticcmd* cmd)
{
    auto& demo = demoState();

    if (*demo.demo_p == DEMOMARKER)
    {
        // end of demo data stream
        checkDemoStatus();
        return;
    }
    cmd->forwardmove = (static_cast<signed char>(*demo.demo_p++));
    cmd->sidemove = (static_cast<signed char>(*demo.demo_p++));
    cmd->angleturn = (static_cast<unsigned char>(*demo.demo_p++)) << 8;
    cmd->buttons = static_cast<unsigned char>(*demo.demo_p++);
}

void writeDemoTiccmd(Ticcmd* cmd)
{
    auto& demo = demoState();

    if (ticcmdInput().gamekeydown['q']) // press q to end demo recording
        checkDemoStatus();
    *demo.demo_p++ = cmd->forwardmove;
    *demo.demo_p++ = cmd->sidemove;
    *demo.demo_p++ = (cmd->angleturn + 128) >> 8;
    *demo.demo_p++ = cmd->buttons;
    demo.demo_p -= 4;
    if (demo.demo_p > demo.demoend - 16)
    {
        // no more space
        checkDemoStatus();
        return;
    }

    readDemoTiccmd(cmd); // make SURE it is exactly the same
}

//
// recordDemo
//
void recordDemo(char* name)
{
    auto& demo = demoState();

    int i;
    int maxsize;

    demo.usergame = false;
    doom_strcpy(demo.demoname, name);
    doom_concat(demo.demoname, ".lmp");
    maxsize = 0x20000;
    i = Doom::checkParm("-maxdemo");
    if (i && i < myargc - 1)
        maxsize = doom_atoi(myargv[i + 1]) * 1024;
    demo.demobuffer = static_cast<byte*>((doom_malloc(maxsize)));
    demo.demoend = demo.demobuffer + maxsize;

    demo.demorecording = true;
}

void beginRecording()
{
    auto& demo = demoState();
    auto& session = gameSession();
    auto& opts = launchOptions();
    auto& players_ = playerState();

    demo.demo_p = demo.demobuffer;

    *demo.demo_p++ = VERSION;
    *demo.demo_p++ = session.gameskill;
    *demo.demo_p++ = session.gameepisode;
    *demo.demo_p++ = session.gamemap;
    *demo.demo_p++ = session.deathmatch;
    *demo.demo_p++ = opts.respawnparm;
    *demo.demo_p++ = opts.fastparm;
    *demo.demo_p++ = opts.nomonsters;
    *demo.demo_p++ = players_.consoleplayer;

    for (int i = 0; i < MAXPLAYERS; i++)
        *demo.demo_p++ = players_.playeringame[i];
}

//
// gPlayDemo
//

void deferPlayDemo(const char* name)
{
    defdemoname = name;
    gameFlow().gameaction = ga_playdemo;
}

void doPlayDemo()
{
    auto& flow = gameFlow();
    auto& demo = demoState();
    auto& session = gameSession();
    auto& opts = launchOptions();
    auto& players_ = playerState();

    Skill skill;
    int episode, map;

    flow.gameaction = ga_nothing;
    demo.demobuffer = demo.demo_p =
        static_cast<byte*>((Doom::cacheLumpName(defdemoname)));
    byte demo_version = *demo.demo_p++;
    if (demo_version != VERSION
        && demo_version != 109) // Demos seem to run fine with version 109
    {
        //doom_print("Demo is from a different game version! Demo Verson = %i, this version = %i\n", (int)demo_version, VERSION);
        doom_print("Demo is from a different game version! Demo Verson = ");
        doom_print(doom_itoa(static_cast<int>(demo_version), 10));
        doom_print(", this version = ");
        doom_print(doom_itoa(VERSION, 10));
        doom_print("\n");
        flow.gameaction = ga_nothing;
        return;
    }

    skill = static_cast<Skill>((*demo.demo_p++));
    episode = *demo.demo_p++;
    map = *demo.demo_p++;
    session.deathmatch = *demo.demo_p++;
    opts.respawnparm = *demo.demo_p++;
    opts.fastparm = *demo.demo_p++;
    opts.nomonsters = *demo.demo_p++;
    players_.consoleplayer = *demo.demo_p++;

    for (int i = 0; i < MAXPLAYERS; i++)
        players_.playeringame[i] = *demo.demo_p++;
    if (players_.playeringame[1])
    {
        session.netgame = true;
        demo.netdemo = true;
    }

    // don't spend a lot of time in loadlevel
    engineParams().precache = false;
    initNewGame(skill, episode, map);
    engineParams().precache = true;

    demo.usergame = false;
    demo.demoplayback = true;
}

//
// startTimeDemo
//
void startTimeDemo(char* name)
{
    auto& refresh = refreshFlags();

    refresh.nodrawers = Doom::checkParm("-nodraw");
    refresh.noblit = Doom::checkParm("-noblit");
    timeDemo().timingdemo = true;
    engineParams().singletics = true;

    defdemoname = name;
    gameFlow().gameaction = ga_playdemo;
}

/*
===================
=
= checkDemoStatus
=
= Called after a death or level completion to allow demos to be cleaned up
= Returns true if a new demo loop action will take place
===================
*/

doom_boolean checkDemoStatus()
{
    auto& demo = demoState();
    auto& timedemo = timeDemo();

    int endtime;

    if (timedemo.timingdemo)
    {
        endtime = currentTic();
        //fatalError("Error: timed %i gametics in %i realtics", gametic
        //        , endtime - starttime);

        doom_strcpy(error_buf, "Error: timed ");
        doom_concat(error_buf, doom_itoa(gameClock().gametic, 10));
        doom_concat(error_buf, " gametics in ");
        doom_concat(error_buf, doom_itoa(endtime - timedemo.starttime, 10));
        doom_concat(error_buf, " realtics");
        fatalError(error_buf);
    }

    if (demo.demoplayback)
    {
        auto& session = gameSession();
        auto& players_ = playerState();
        auto& opts = launchOptions();

        if (demo.singledemo)
            quitGame();

        demo.demoplayback = false;
        demo.netdemo = false;
        session.netgame = false;
        session.deathmatch = false;
        players_.playeringame[1] = players_.playeringame[2] =
            players_.playeringame[3] = 0;
        opts.respawnparm = false;
        opts.fastparm = false;
        opts.nomonsters = false;
        players_.consoleplayer = 0;
        Doom::advanceDemo();
        return true;
    }

    if (demo.demorecording)
    {
        *demo.demo_p++ = DEMOMARKER;
        Doom::writeFile(demo.demoname,
                        demo.demobuffer,
                        static_cast<int>((demo.demo_p - demo.demobuffer)));
        doom_free(demo.demobuffer);
        demo.demorecording = false;
        //fatalError("Error: Demo %s recorded", demoname);

        doom_strcpy(error_buf, "Error: Demo ");
        doom_concat(error_buf, demo.demoname);
        doom_concat(error_buf, " recorded");
        fatalError(error_buf);
    }

    return false;
}

} // namespace Doom
