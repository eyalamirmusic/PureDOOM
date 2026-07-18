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
#include "InputConfig.h"
#include "IntermissionInfo.h"
#include "LevelStats.h"
#include "MovementSpeeds.h"
#include "NetState.h"
#include "ParTimes.h"
#include "PendingCommands.h"
#include "PlayerState.h"
#include "RefreshFlags.h"
#include "SaveGameState.h"
#include "TiccmdInput.h"
#include "TimeDemo.h"

#include "../Render/Data.h"
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
#define MAXPLMOVE (forwardmove[1])
#define TURBOTHRESHOLD 0x32
#define SLOWTURNTICS 6
#define NUMKEYS 256
#define BODYQUESIZE 32
#define VERSIONSIZE 16
#define DEMOMARKER 0x80

// Prototypes for other subsystems' functions.
void Doom::spawnPlayer(Doom::MapThing* mthing);
void Doom::executeSetViewSize();

// gameaction, gamestate and wipegamestate are a Doom::GameFlow owned by the Engine now; these
// (and the extern wipegamestate below) are references onto it (REFACTOR.md, Step 5).
Doom::GameAction& gameaction = Doom::gameFlow().gameaction;
Doom::GameState& gamestate = Doom::gameFlow().gamestate;

// The current game's rules are a Doom::GameSession owned by the Engine now; these (and
// netgame/deathmatch below) are references onto it (REFACTOR.md, Step 5).
Doom::Skill& gameskill = Doom::gameSession().gameskill;
doom_boolean& respawnmonsters = Doom::gameSession().respawnmonsters;
int& gameepisode = Doom::gameSession().gameepisode;
int& gamemap = Doom::gameSession().gamemap;

// paused (with viewactive/nodrawers/noblit below) is a Doom::RefreshFlags owned by the
// Engine now; these are references onto it (REFACTOR.md, Step 5).
doom_boolean& paused = Doom::refreshFlags().paused;
// sendpause/sendsave are a Doom::PendingCommands owned by the Engine now, moved by the
// file-scope-statics sweep; these vanilla names are references onto the members (REFACTOR.md,
// Step 5).
doom_boolean& sendpause =
    Doom::pendingCommands().sendpause; // send a pause event next tic
doom_boolean& sendsave =
    Doom::pendingCommands().sendsave; // send a save event next tic

// usergame (with the demo flags below) is a Doom::DemoState owned by the Engine now; these
// are references onto it (REFACTOR.md, Step 5).
doom_boolean& usergame = Doom::demoState().usergame; // ok to save / end game

// timingdemo (with starttime below) is the -timedemo benchmark state, a Doom::TimeDemo owned by
// the Engine now; these vanilla names are references onto it (REFACTOR.md, Step 5).
doom_boolean& timingdemo =
    Doom::timeDemo().timingdemo; // if true, exit with report on completion
doom_boolean& nodrawers = Doom::refreshFlags().nodrawers; // comparative timing
doom_boolean& noblit = Doom::refreshFlags().noblit; // comparative timing
int& starttime = Doom::timeDemo().starttime; // for comparative timing purposes

doom_boolean& viewactive = Doom::refreshFlags().viewactive;

doom_boolean& deathmatch = Doom::gameSession().deathmatch; // net deathmatch
doom_boolean& netgame = Doom::gameSession().netgame; // packets are broadcast
// The player roster and view selection is a Doom::PlayerState owned by the Engine now;
// these are references onto it (the arrays as references-to-array) (REFACTOR.md, Step 5).
doom_boolean (&playeringame)[MAXPLAYERS] = Doom::playerState().playeringame;
Doom::Player (&players)[MAXPLAYERS] = Doom::playerState().players;

int& consoleplayer =
    Doom::playerState().consoleplayer; // taking events and displaying
int& displayplayer = Doom::playerState().displayplayer; // view being displayed
int& gametic = Doom::gameClock().gametic; // Doom::GameClock (Engine member)

// The level's progress (levelstarttic + the intermission totals, and leveltime over in
// p_tick) is a Doom::LevelStats owned by the Engine now; these are references onto it.
int& levelstarttic = Doom::levelStats().levelstarttic; // gametic at level start
int& totalkills = Doom::levelStats().totalkills; // for intermission
int& totalitems = Doom::levelStats().totalitems;
int& totalsecret = Doom::levelStats().totalsecret;

// The demo flags and buffer are a Doom::DemoState owned by the Engine now; these vanilla names
// are references onto it (the buffer state folded in by the file-scope-statics sweep - REFACTOR.md,
// Step 5). demoname is a reference-to-array, the buffer pointers references-to-pointer.
char (&demoname)[32] = Doom::demoState().demoname;
doom_boolean& demorecording = Doom::demoState().demorecording;
doom_boolean& demoplayback = Doom::demoState().demoplayback;
doom_boolean& netdemo = Doom::demoState().netdemo;
byte*& demobuffer = Doom::demoState().demobuffer;
byte*& demo_p = Doom::demoState().demo_p;
byte*& demoend = Doom::demoState().demoend;
doom_boolean& singledemo = Doom::demoState().singledemo; // quit after one demo

// precache is a Doom::EngineParams owned by the Engine now; this is a reference onto it (default
// true - load all graphics at start; REFACTOR.md, Step 5).
doom_boolean& precache = Doom::engineParams().precache;

// wminfo is a Doom::IntermissionInfo owned by the Engine now; this is a reference onto it
// (REFACTOR.md, Step 5).
Doom::IntermissionStart& wminfo =
    Doom::intermissionInfo().wminfo; // world map / intermission parms

// consistancy folded into Doom::NetState (the netcode bookkeeping) by the file-scope-statics
// sweep; this vanilla name is a reference-to-array onto the member (REFACTOR.md, Step 5).
short (&consistancy)[MAXPLAYERS][BACKUPTICS] = Doom::netState().consistancy;

byte*& savebuffer = Doom::saveGameState().buffer;

//
// controls (have defaults). The config-backed control bindings are a Doom::InputConfig owned by
// the Engine now (Game/InputConfig.h); these are references onto its members. Config.cpp binds
// its defaults[] entries to the members at runtime rather than capturing their addresses at
// static-init, which is what unblocked the migration.
//
int& key_right = Doom::inputConfig().key_right;
int& key_left = Doom::inputConfig().key_left;

int& key_up = Doom::inputConfig().key_up;
int& key_down = Doom::inputConfig().key_down;
int& key_strafeleft = Doom::inputConfig().key_strafeleft;
int& key_straferight = Doom::inputConfig().key_straferight;
int& key_fire = Doom::inputConfig().key_fire;
int& key_use = Doom::inputConfig().key_use;
int& key_strafe = Doom::inputConfig().key_strafe;
int& key_speed = Doom::inputConfig().key_speed;

int& mousebfire = Doom::inputConfig().mousebfire;
int& mousebstrafe = Doom::inputConfig().mousebstrafe;
int& mousebforward = Doom::inputConfig().mousebforward;
int& mousemove = Doom::inputConfig().mousemove;

int& joybfire = Doom::inputConfig().joybfire;
int& joybstrafe = Doom::inputConfig().joybstrafe;
int& joybuse = Doom::inputConfig().joybuse;
int& joybspeed = Doom::inputConfig().joybspeed;

// The movement-speed tables Doom::buildTiccmd applies to the player's input are a Doom::MovementSpeeds
// owned by the Engine now, moved by the file-scope-statics sweep; these vanilla names are
// references-to-array onto the members (REFACTOR.md, Step 5). forwardmove/sidemove are also scaled
// by -turbo over in Game/DoomMain.cpp, whose externs move to references in lockstep.
EA::Array<fixed_t, 2>& forwardmove = Doom::movementSpeeds().forwardmove;
EA::Array<fixed_t, 2>& sidemove = Doom::movementSpeeds().sidemove;
EA::Array<fixed_t, 3>& angleturn = Doom::movementSpeeds().angleturn; // + slow turn

// The per-tic input accumulators are a Doom::TiccmdInput owned by the Engine now; these
// vanilla names are references onto it (REFACTOR.md, Step 5, the file-scope-statics sweep). The
// arrays become references-to-array; the interior view pointers mousebuttons/joybuttons stay
// here, indexing into the referenced arrays to allow a [-1] index.
doom_boolean (&gamekeydown)[NUMKEYS] = Doom::ticcmdInput().gamekeydown;
int& turnheld = Doom::ticcmdInput().turnheld; // for accelerative turning

doom_boolean (&mousearray)[4] = Doom::ticcmdInput().mousearray;
doom_boolean* mousebuttons = &mousearray[1]; // allow [-1]

// mouse values are used once
int& mousex = Doom::ticcmdInput().mousex;
int& mousey = Doom::ticcmdInput().mousey;

int& dclicktime = Doom::ticcmdInput().dclicktime;
int& dclickstate = Doom::ticcmdInput().dclickstate;
int& dclicks = Doom::ticcmdInput().dclicks;
int& dclicktime2 = Doom::ticcmdInput().dclicktime2;
int& dclickstate2 = Doom::ticcmdInput().dclickstate2;
int& dclicks2 = Doom::ticcmdInput().dclicks2;

// joystick values are repeated
int& joyxmove = Doom::ticcmdInput().joyxmove;
int& joyymove = Doom::ticcmdInput().joyymove;
doom_boolean (&joyarray)[5] = Doom::ticcmdInput().joyarray;
doom_boolean* joybuttons = &joyarray[1]; // allow [-1]

int savegameslot;
EA::Array<char, 32> savedescription;

// The corpse queue (bodyque + bodyqueslot) is a Doom::CorpseQueue owned by the Engine now;
// these are references onto it, bodyque as a reference-to-array (REFACTOR.md, Step 5).
Doom::Mobj* (&bodyque)[BODYQUESIZE] = Doom::corpseQueue().bodyque;
int& bodyqueslot = Doom::corpseQueue().bodyqueslot;

void* statcopy; // for statistics driver

// The par-time tables (DOOM's pars[episode][map] and DOOM II's flat cpars[]) are a Doom::ParTimes
// owned by the Engine now, moved by the file-scope-statics sweep; these vanilla names are
// references-to-array onto the members (REFACTOR.md, Step 5).
int (&pars)[4][10] = Doom::parTimes().pars;
int (&cpars)[32] = Doom::parTimes().cpars;

doom_boolean secretexit;

char (&savename)[256] = Doom::saveGameState().name;

// The deferred new-game request is a Doom::DeferredNewGame owned by the Engine now; these vanilla
// names are references onto it (the file-scope-statics sweep - REFACTOR.md, Step 5).
Doom::Skill& d_skill = Doom::deferredNewGame().d_skill;
int& d_episode = Doom::deferredNewGame().d_episode;
int& d_map = Doom::deferredNewGame().d_map;

const char* defdemoname;

extern Doom::GameState& wipegamestate; // Doom::GameFlow (Engine member)
extern const char*& pagename; // Doom::AttractMode (Engine member)
extern doom_boolean& setsizeneeded;

// The sky texture to be used instead of the F_SKY1 dummy (Doom::SkyState member).
extern int& skytexture;

// Other subsystems' globals this file reads (declared at global scope so the
// namespace code below resolves them to ::, not Doom::).
extern int& always_run; // Doom::InputConfig member (Engine)
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
    doom_boolean strafe;
    doom_boolean bstrafe;
    int speed;
    int tspeed;
    int forward;
    int side;

    Ticcmd* base;

    base = baseTiccmd(); // empty, or external driver
    doom_memcpy(cmd, base, sizeof(*cmd));

    cmd->consistancy = consistancy[consoleplayer][maketic % BACKUPTICS];

    strafe = gamekeydown[key_strafe] || mousebuttons[mousebstrafe]
             || joybuttons[joybstrafe];

    doom_boolean running = always_run ? (gamekeydown[key_speed] ? false : true)
                                      : (gamekeydown[key_speed] ? true : false);
    speed = running || joybuttons[joybspeed];

    forward = side = 0;

    // use two stage accelerative turning
    // on the keyboard and joystick
    if (joyxmove < 0 || joyxmove > 0 || gamekeydown[key_right]
        || gamekeydown[key_left])
        turnheld += ticdup;
    else
        turnheld = 0;

    if (turnheld < SLOWTURNTICS)
        tspeed = 2; // slow turn
    else
        tspeed = speed;

    // let movement keys cancel each other out
    if (strafe)
    {
        if (gamekeydown[key_right])
        {
            side += sidemove[speed];
        }
        if (gamekeydown[key_left])
        {
            side -= sidemove[speed];
        }
        if (joyxmove > 0)
            side += sidemove[speed];
        if (joyxmove < 0)
            side -= sidemove[speed];
    }
    else
    {
        if (gamekeydown[key_right])
            cmd->angleturn -= angleturn[tspeed];
        if (gamekeydown[key_left])
            cmd->angleturn += angleturn[tspeed];
        if (joyxmove > 0)
            cmd->angleturn -= angleturn[tspeed];
        if (joyxmove < 0)
            cmd->angleturn += angleturn[tspeed];
    }

    if (gamekeydown[key_up])
    {
        forward += forwardmove[speed];
    }
    if (gamekeydown[key_down])
    {
        forward -= forwardmove[speed];
    }
    if (joyymove < 0)
        forward += forwardmove[speed];
    if (joyymove > 0)
        forward -= forwardmove[speed];
    if (gamekeydown[key_straferight])
        side += sidemove[speed];
    if (gamekeydown[key_strafeleft])
        side -= sidemove[speed];

    // buttons
    cmd->chatchar = Doom::dequeueChatChar();

    if (gamekeydown[key_fire] || mousebuttons[mousebfire] || joybuttons[joybfire])
        cmd->buttons |= BT_ATTACK;

    if (gamekeydown[key_use] || joybuttons[joybuse])
    {
        cmd->buttons |= BT_USE;
        // clear double clicks if hit use button
        dclicks = 0;
    }

    // chainsaw overrides
    for (int i = 0; i < NUMWEAPONS - 1; i++)
        if (gamekeydown['1' + i])
        {
            cmd->buttons |= BT_CHANGE;
            cmd->buttons |= i << BT_WEAPONSHIFT;
            break;
        }

    // mouse
    if (mousebuttons[mousebforward])
        forward += forwardmove[speed];

    // forward double click
    if (mousebuttons[mousebforward] != dclickstate && dclicktime > 1)
    {
        dclickstate = mousebuttons[mousebforward];
        if (dclickstate)
            dclicks++;
        if (dclicks == 2)
        {
            cmd->buttons |= BT_USE;
            dclicks = 0;
        }
        else
            dclicktime = 0;
    }
    else
    {
        dclicktime += ticdup;
        if (dclicktime > 20)
        {
            dclicks = 0;
            dclickstate = 0;
        }
    }

    // strafe double click
    bstrafe = mousebuttons[mousebstrafe] || joybuttons[joybstrafe];
    if (bstrafe != dclickstate2 && dclicktime2 > 1)
    {
        dclickstate2 = bstrafe;
        if (dclickstate2)
            dclicks2++;
        if (dclicks2 == 2)
        {
            cmd->buttons |= BT_USE;
            dclicks2 = 0;
        }
        else
            dclicktime2 = 0;
    }
    else
    {
        dclicktime2 += ticdup;
        if (dclicktime2 > 20)
        {
            dclicks2 = 0;
            dclickstate2 = 0;
        }
    }

    if (mousemove)
        forward += mousey;
    if (strafe)
        side += mousex * 2;
    else
        cmd->angleturn -= mousex * 0x8;

    mousex = mousey = 0;

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
    if (sendpause)
    {
        sendpause = false;
        cmd->buttons = BT_SPECIAL | BTS_PAUSE;
    }

    if (sendsave)
    {
        sendsave = false;
        cmd->buttons = BT_SPECIAL | BTS_SAVEGAME | (savegameslot << BTS_SAVESHIFT);
    }
}

//
// doLoadLevel
//
void doLoadLevel()
{
    // Set the sky map.
    // First thing, we have a dummy sky texture name,
    //  a flat. The data is in the WAD only because
    //  we look for an actual index, instead of simply
    //  setting one.
    skyflatnum = Doom::flatNumForName(SKYFLATNAME);

    // DOOM determines the sky texture to be used
    // depending on the current episode, and the game version.
    if ((gamemode == commercial)
        || (static_cast<int>(gamemode) == static_cast<int>(pack_tnt))
        || (static_cast<int>(gamemode) == static_cast<int>(pack_plut)))
    {
        skytexture = Doom::textureNumForName("SKY3");
        if (gamemap < 12)
            skytexture = Doom::textureNumForName("SKY1");
        else if (gamemap < 21)
            skytexture = Doom::textureNumForName("SKY2");
    }

    levelstarttic = gametic; // for time calculation

    if (wipegamestate == GS_LEVEL)
        wipegamestate = static_cast<GameState>((-1)); // force a wipe

    gamestate = GS_LEVEL;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (playeringame[i] && players[i].playerstate == PST_DEAD)
            players[i].playerstate = PST_REBORN;
        doom_memset(players[i].frags, 0, sizeof(players[i].frags));
    }

    Doom::setupLevel(gameepisode, gamemap, 0, gameskill);
    displayplayer = consoleplayer; // view the guy you are playing
    starttime = currentTic();
    gameaction = ga_nothing;

    // clear cmd building stuff
    doom_memset(gamekeydown, 0, sizeof(gamekeydown));
    joyxmove = joyymove = 0;
    mousex = mousey = 0;
    sendpause = sendsave = paused = false;
    doom_memset(mousebuttons, 0, sizeof(*mousebuttons) * 3);
    doom_memset(joybuttons, 0, sizeof(*joybuttons) * 4);
}

//
// gameResponder
// Get info needed to make ticcmd_ts for the players.
//
doom_boolean gameResponder(Event* ev)
{
    // allow spy mode changes even during the demo
    if (gamestate == GS_LEVEL && ev->type == ev_keydown && ev->data1 == KEY_F12
        && (singledemo || !deathmatch))
    {
        // spy mode
        do
        {
            displayplayer++;
            if (displayplayer == MAXPLAYERS)
                displayplayer = 0;
        } while (!playeringame[displayplayer] && displayplayer != consoleplayer);
        return true;
    }

    // any other key pops up menu if in demos
    if (gameaction == ga_nothing && !singledemo
        && (demoplayback || gamestate == GS_DEMOSCREEN))
    {
        if (ev->type == ev_keydown || (ev->type == ev_mouse && ev->data1)
            || (ev->type == ev_joystick && ev->data1))
        {
            startControlPanel();
            return true;
        }
        return false;
    }

    if (gamestate == GS_LEVEL)
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

    if (gamestate == GS_FINALE)
    {
        if (Doom::finaleResponder(ev))
            return true; // finale ate the event
    }

    switch (ev->type)
    {
        case ev_keydown:
            if (ev->data1 == KEY_PAUSE)
            {
                sendpause = true;
                return true;
            }
            if (ev->data1 < NUMKEYS)
                gamekeydown[ev->data1] = true;
            return true; // eat key down events

        case ev_keyup:
            if (ev->data1 < NUMKEYS)
                gamekeydown[ev->data1] = false;
            return false; // always let key up events filter down

        case ev_mouse:
            mousebuttons[0] = ev->data1 & 1;
            mousebuttons[1] = ev->data1 & 2;
            mousebuttons[2] = ev->data1 & 4;
            mousex = ev->data2 * (mouseSensitivity + 5) / 10;
            mousey = ev->data3 * (mouseSensitivity + 5) / 10;
            return true; // eat events

        case ev_joystick:
            joybuttons[0] = ev->data1 & 1;
            joybuttons[1] = ev->data1 & 2;
            joybuttons[2] = ev->data1 & 4;
            joybuttons[3] = ev->data1 & 8;
            joyxmove = ev->data2;
            joyymove = ev->data3;
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
    int buf;
    Ticcmd* cmd;

    // do player reborns if needed
    for (int i = 0; i < MAXPLAYERS; i++)
        if (playeringame[i] && players[i].playerstate == PST_REBORN)
            doReborn(i);

    // do things to change the game state
    while (gameaction != ga_nothing)
    {
        switch (gameaction)
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
                gameaction = ga_nothing;
                break;
            case ga_nothing:
                break;
        }
    }

    // get commands, check consistancy,
    // and build new consistancy check
    buf = (gametic / ticdup) % BACKUPTICS;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (playeringame[i])
        {
            cmd = &players[i].cmd;

            doom_memcpy(cmd, &netcmds[i][buf], sizeof(Ticcmd));

            if (demoplayback)
                readDemoTiccmd(cmd);
            if (demorecording)
                writeDemoTiccmd(cmd);

            // check for turbo cheats
            if (cmd->forwardmove > TURBOTHRESHOLD && !(gametic & 31)
                && ((gametic >> 5) & 3) == i)
            {
                static EA::Array<char, 80> turbomessage;
                //doom_sprintf(turbomessage, "%s is turbo!", player_names[i]);
                doom_strcpy(turbomessage.data(), player_names[i]);
                doom_concat(turbomessage.data(), " is turbo!");
                players[consoleplayer].message = turbomessage.data();
            }

            if (netgame && !netdemo && !(gametic % ticdup))
            {
                if (gametic > BACKUPTICS && consistancy[i][buf] != cmd->consistancy)
                {
                    //fatalError("Error: consistency failure (%i should be %i)",
                    //        cmd->consistancy, consistancy[i][buf]);

                    doom_strcpy(error_buf, "Error: consistency failure (");
                    doom_concat(error_buf, doom_itoa(cmd->consistancy, 10));
                    doom_concat(error_buf, " should be ");
                    doom_concat(error_buf, doom_itoa(consistancy[i][buf], 10));
                    doom_concat(error_buf, ")");
                    fatalError(error_buf);
                }
                if (players[i].mo)
                    consistancy[i][buf] = players[i].mo->x;
                else
                    consistancy[i][buf] = rndindex;
            }
        }
    }

    // check for special buttons
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (playeringame[i])
        {
            if (players[i].cmd.buttons & BT_SPECIAL)
            {
                switch (players[i].cmd.buttons & BT_SPECIALMASK)
                {
                    case BTS_PAUSE:
                        paused ^= 1;
                        if (paused)
                            Doom::pauseSound();
                        else
                            Doom::resumeSound();
                        break;

                    case BTS_SAVEGAME:
                        if (!savedescription[0])
                            doom_strcpy(savedescription.data(), "NET GAME");
                        savegameslot =
                            (players[i].cmd.buttons & BTS_SAVEMASK) >> BTS_SAVESHIFT;
                        gameaction = ga_savegame;
                        break;
                }
            }
        }
    }

    // do main actions
    switch (gamestate)
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

    p = &players[player];

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
    Player* p;
    EA::Array<int, MAXPLAYERS> frags;
    int killcount;
    int itemcount;
    int secretcount;

    doom_memcpy(frags.data(), players[player].frags, sizeof(frags));
    killcount = players[player].killcount;
    itemcount = players[player].itemcount;
    secretcount = players[player].secretcount;

    p = &players[player];
    doom_memset(p, 0, sizeof(*p));

    doom_memcpy(players[player].frags, frags.data(), sizeof(players[player].frags));
    players[player].killcount = killcount;
    players[player].itemcount = itemcount;
    players[player].secretcount = secretcount;

    p->usedown = p->attackdown = true; // don't do anything immediately
    p->playerstate = PST_LIVE;
    p->health = MAXHEALTH;
    p->readyweapon = p->pendingweapon = wp_pistol;
    p->weaponowned[wp_fist] = true;
    p->weaponowned[wp_pistol] = true;
    p->ammo[am_clip] = 50;

    for (int i = 0; i < NUMAMMO; i++)
        p->maxammo[i] = maxammo[i];
}

//
// checkSpot
// Returns false if the player cannot be respawned
// at the given MapThing spot
// because something is occupying it
//

doom_boolean checkSpot(int playernum, MapThing* mthing)
{
    fixed_t x;
    fixed_t y;
    SubSector* ss;
    unsigned an;
    Mobj* mo;

    if (!players[playernum].mo)
    {
        // first spawn of level, before corpses
        for (int i = 0; i < playernum; i++)
            if (players[i].mo->x == mthing->x << FRACBITS
                && players[i].mo->y == mthing->y << FRACBITS)
                return false;
        return true;
    }

    x = mthing->x << FRACBITS;
    y = mthing->y << FRACBITS;

    if (!Doom::checkPosition(players[playernum].mo, x, y))
        return false;

    // flush an old corpse if needed
    if (bodyqueslot >= BODYQUESIZE)
        Doom::removeMobj(bodyque[bodyqueslot % BODYQUESIZE]);
    bodyque[bodyqueslot % BODYQUESIZE] = players[playernum].mo;
    bodyqueslot++;

    // spawn a teleport fog
    ss = Doom::pointInSubsector(x, y);
    an = (ANG45 * (mthing->angle / 45)) >> ANGLETOFINESHIFT;

    mo = Doom::spawnMobj(x + 20 * finecosine[an],
                         y + 20 * finesine[an],
                         ss->sector->floorheight,
                         MT_TFOG);

    if (players[consoleplayer].viewz != 1)
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
    int i;
    int selections;

    selections = static_cast<int>((deathmatch_p - deathmatchstarts));
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
        if (checkSpot(playernum, &deathmatchstarts[i]))
        {
            deathmatchstarts[i].type = playernum + 1;
            Doom::spawnPlayer(&deathmatchstarts[i]);
            return;
        }
    }

    // no good spot, so the player will probably get stuck
    Doom::spawnPlayer(&playerstarts[playernum]);
}

//
// doReborn
//
void doReborn(int playernum)
{
    if (!netgame)
    {
        // reload the level from scratch
        gameaction = ga_loadlevel;
    }
    else
    {
        // respawn at the start

        // first dissasociate the corpse
        players[playernum].mo->player = nullptr;

        // spawn at random spot if in death match
        if (deathmatch)
        {
            deathMatchSpawnPlayer(playernum);
            return;
        }

        if (checkSpot(playernum, &playerstarts[playernum]))
        {
            Doom::spawnPlayer(&playerstarts[playernum]);
            return;
        }

        // try to spawn at one of the other players spots
        for (int i = 0; i < MAXPLAYERS; i++)
        {
            if (checkSpot(playernum, &playerstarts[i]))
            {
                playerstarts[i].type = playernum + 1; // fake as other player
                Doom::spawnPlayer(&playerstarts[i]);
                playerstarts[i].type = i + 1; // restore
                return;
            }
            // he's going to be inside something.  Too bad.
        }
        Doom::spawnPlayer(&playerstarts[playernum]);
    }
}

void takeScreenshot()
{
    gameaction = ga_screenshot;
}

//
// doCompleted
//
void exitLevel()
{
    secretexit = false;
    gameaction = ga_completed;
}

// Here's for the german edition.
void secretExitLevel()
{
    // IF NO WOLF3D LEVELS, NO SECRET EXIT!
    if ((gamemode == commercial) && (Doom::wad().find("map31") < 0))
        secretexit = false;
    else
        secretexit = true;
    gameaction = ga_completed;
}

void doCompleted()
{
    gameaction = ga_nothing;

    for (int i = 0; i < MAXPLAYERS; i++)
        if (playeringame[i])
            playerFinishLevel(i); // take away cards and stuff

    if (automapactive)
        Doom::stopAutomap();

    if (gamemode != commercial)
        switch (gamemap)
        {
            case 8:
                gameaction = ga_victory;
                return;
            case 9:
                for (int i = 0; i < MAXPLAYERS; i++)
                    players[i].didsecret = true;
                break;
        }

    if ((gamemap == 8) && (gamemode != commercial))
    {
        // victory
        gameaction = ga_victory;
        return;
    }

    if ((gamemap == 9) && (gamemode != commercial))
    {
        // exit secret level
        for (int i = 0; i < MAXPLAYERS; i++)
            players[i].didsecret = true;
    }

    wminfo.didsecret = players[consoleplayer].didsecret;
    wminfo.epsd = gameepisode - 1;
    wminfo.last = gamemap - 1;

    // wminfo.next is 0 biased, unlike gamemap
    if (gamemode == commercial)
    {
        if (secretexit)
            switch (gamemap)
            {
                case 15:
                    wminfo.next = 30;
                    break;
                case 31:
                    wminfo.next = 31;
                    break;
            }
        else
            switch (gamemap)
            {
                case 31:
                case 32:
                    wminfo.next = 15;
                    break;
                default:
                    wminfo.next = gamemap;
            }
    }
    else
    {
        if (secretexit)
            wminfo.next = 8; // go to secret level
        else if (gamemap == 9)
        {
            // returning from secret level
            switch (gameepisode)
            {
                case 1:
                    wminfo.next = 3;
                    break;
                case 2:
                    wminfo.next = 5;
                    break;
                case 3:
                    wminfo.next = 6;
                    break;
                case 4:
                    wminfo.next = 2;
                    break;
            }
        }
        else
            wminfo.next = gamemap; // go to next level
    }

    wminfo.maxkills = totalkills;
    wminfo.maxitems = totalitems;
    wminfo.maxsecret = totalsecret;
    wminfo.maxfrags = 0;
    if (gamemode == commercial)
        wminfo.partime = 35 * cpars[gamemap - 1];
    else
        wminfo.partime = 35 * pars[gameepisode][gamemap];
    wminfo.pnum = consoleplayer;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        wminfo.plyr[i].in = playeringame[i];
        wminfo.plyr[i].skills = players[i].killcount;
        wminfo.plyr[i].sitems = players[i].itemcount;
        wminfo.plyr[i].ssecret = players[i].secretcount;
        wminfo.plyr[i].stime = leveltime;
        doom_memcpy(
            wminfo.plyr[i].frags, players[i].frags, sizeof(wminfo.plyr[i].frags));
    }

    gamestate = GS_INTERMISSION;
    viewactive = false;
    automapactive = false;

    if (statcopy)
        doom_memcpy(statcopy, &wminfo, sizeof(wminfo));

    Doom::startIntermission(&wminfo);
}

//
// worldDone
//
void worldDone()
{
    gameaction = ga_worlddone;

    if (secretexit)
        players[consoleplayer].didsecret = true;

    if (gamemode == commercial)
    {
        switch (gamemap)
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
    gamestate = GS_LEVEL;
    gamemap = wminfo.next + 1;
    doLoadLevel();
    gameaction = ga_nothing;
    viewactive = true;
}

//
// G_InitFromSavegame
// Can be called by the startup code or the menu task.
//

void loadGame(char* name)
{
    doom_strcpy(savename, name);
    gameaction = ga_loadgame;
}

void doLoadGame()
{
    int a, b, c;
    EA::Array<char, VERSIONSIZE> vcheck;

    gameaction = ga_nothing;

    Doom::readFile(savename, &savebuffer);
    save_p = savebuffer + SAVESTRINGSIZE;

    // skip the description field
    doom_memset(vcheck.data(), 0, sizeof(vcheck));
    //doom_sprintf(vcheck, "version %i", VERSION);
    doom_strcpy(vcheck.data(), "version ");
    doom_concat(vcheck.data(), doom_itoa(VERSION, 10));
    if (doom_strcmp(reinterpret_cast<const char*>(save_p),
                    const_cast<const char*>(vcheck.data())))
        return; // bad version
    save_p += VERSIONSIZE;

    gameskill = static_cast<Skill>((*save_p++));
    gameepisode = *save_p++;
    gamemap = *save_p++;
    for (int i = 0; i < MAXPLAYERS; i++)
        playeringame[i] = *save_p++;

    // load a base level
    initNewGame(gameskill, gameepisode, gamemap);

    // get the times
    a = *save_p++;
    b = *save_p++;
    c = *save_p++;
    leveltime = (a << 16) + (b << 8) + c;

    // dearchive all the modifications
    Doom::unArchivePlayers();
    Doom::unArchiveWorld();
    Doom::unArchiveThinkers();
    Doom::unArchiveSpecials();

    if (*save_p != 0x1d)
        fatalError("Error: Bad savegame");

    // done
    doom_free(savebuffer);

    if (setsizeneeded)
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
    sendsave = true;
}

void doSaveGame()
{
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

    save_p = savebuffer = screens[1] + 0x4000;

    doom_memcpy(save_p, description, SAVESTRINGSIZE);
    save_p += SAVESTRINGSIZE;
    doom_memset(name2.data(), 0, sizeof(name2));
    //doom_sprintf(name2, "version %i", VERSION);
    doom_strcpy(name2.data(), "version ");
    doom_concat(name2.data(), doom_itoa(VERSION, 10));
    doom_memcpy(save_p, name2.data(), VERSIONSIZE);
    save_p += VERSIONSIZE;

    *save_p++ = gameskill;
    *save_p++ = gameepisode;
    *save_p++ = gamemap;
    for (int i = 0; i < MAXPLAYERS; i++)
        *save_p++ = playeringame[i];
    *save_p++ = leveltime >> 16;
    *save_p++ = leveltime >> 8;
    *save_p++ = leveltime;

    Doom::archivePlayers();
    Doom::archiveWorld();
    Doom::archiveThinkers();
    Doom::archiveSpecials();

    *save_p++ = 0x1d; // consistancy marker

    length = static_cast<int>((save_p - savebuffer));
    if (length > SAVEGAMESIZE)
        fatalError("Error: Savegame buffer overrun");
    Doom::writeFile(name.data(), savebuffer, length);
    gameaction = ga_nothing;
    savedescription[0] = 0;

    players[consoleplayer].message = GGSAVED;

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
    d_skill = skill;
    d_episode = episode;
    d_map = map;
    gameaction = ga_newgame;
}

void doNewGame()
{
    demoplayback = false;
    netdemo = false;
    netgame = false;
    deathmatch = false;
    playeringame[1] = playeringame[2] = playeringame[3] = 0;
    respawnparm = false;
    fastparm = false;
    nomonsters = false;
    consoleplayer = 0;
    initNewGame(d_skill, d_episode, d_map);
    gameaction = ga_nothing;
}

void initNewGame(Skill skill, int episode, int map)
{
    if (paused)
    {
        paused = false;
        Doom::resumeSound();
    }

    if (skill > sk_nightmare)
        skill = sk_nightmare;

    // This was quite messy with SPECIAL and commented parts.
    // Supposedly hacks to make the latest edition work.
    // It might not work properly.
    if (episode < 1)
        episode = 1;

    if (gamemode == retail)
    {
        if (episode > 4)
            episode = 4;
    }
    else if (gamemode == shareware)
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

    if ((map > 9) && (gamemode != commercial))
        map = 9;

    Doom::randomness().clear();

    if (skill == sk_nightmare || respawnparm)
        respawnmonsters = true;
    else
        respawnmonsters = false;

    if (fastparm || (skill == sk_nightmare && gameskill != sk_nightmare))
    {
        for (int i = S_SARG_RUN1; i <= S_SARG_PAIN2; i++)
            states[i].tics >>= 1;
        mobjinfo[MT_BRUISERSHOT].speed = 20 * FRACUNIT;
        mobjinfo[MT_HEADSHOT].speed = 20 * FRACUNIT;
        mobjinfo[MT_TROOPSHOT].speed = 20 * FRACUNIT;
    }
    else if (skill != sk_nightmare && gameskill == sk_nightmare)
    {
        for (int i = S_SARG_RUN1; i <= S_SARG_PAIN2; i++)
            states[i].tics <<= 1;
        mobjinfo[MT_BRUISERSHOT].speed = 15 * FRACUNIT;
        mobjinfo[MT_HEADSHOT].speed = 10 * FRACUNIT;
        mobjinfo[MT_TROOPSHOT].speed = 10 * FRACUNIT;
    }

    // force players to be initialized upon first level load
    for (int i = 0; i < MAXPLAYERS; i++)
        players[i].playerstate = PST_REBORN;

    usergame = true; // will be set false if a demo
    paused = false;
    demoplayback = false;
    automapactive = false;
    viewactive = true;
    gameepisode = episode;
    gamemap = map;
    gameskill = skill;

    viewactive = true;

    // set the sky map for the episode
    if (gamemode == commercial)
    {
        skytexture = Doom::textureNumForName("SKY3");
        if (gamemap < 12)
            skytexture = Doom::textureNumForName("SKY1");
        else if (gamemap < 21)
            skytexture = Doom::textureNumForName("SKY2");
    }
    else
        switch (episode)
        {
            case 1:
                skytexture = Doom::textureNumForName("SKY1");
                break;
            case 2:
                skytexture = Doom::textureNumForName("SKY2");
                break;
            case 3:
                skytexture = Doom::textureNumForName("SKY3");
                break;
            case 4: // Special Edition sky
                skytexture = Doom::textureNumForName("SKY4");
                break;
        }

    doLoadLevel();
}

//
// DEMO RECORDING
//

void readDemoTiccmd(Ticcmd* cmd)
{
    if (*demo_p == DEMOMARKER)
    {
        // end of demo data stream
        checkDemoStatus();
        return;
    }
    cmd->forwardmove = (static_cast<signed char>(*demo_p++));
    cmd->sidemove = (static_cast<signed char>(*demo_p++));
    cmd->angleturn = (static_cast<unsigned char>(*demo_p++)) << 8;
    cmd->buttons = static_cast<unsigned char>(*demo_p++);
}

void writeDemoTiccmd(Ticcmd* cmd)
{
    if (gamekeydown['q']) // press q to end demo recording
        checkDemoStatus();
    *demo_p++ = cmd->forwardmove;
    *demo_p++ = cmd->sidemove;
    *demo_p++ = (cmd->angleturn + 128) >> 8;
    *demo_p++ = cmd->buttons;
    demo_p -= 4;
    if (demo_p > demoend - 16)
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
    int i;
    int maxsize;

    usergame = false;
    doom_strcpy(demoname, name);
    doom_concat(demoname, ".lmp");
    maxsize = 0x20000;
    i = Doom::checkParm("-maxdemo");
    if (i && i < myargc - 1)
        maxsize = doom_atoi(myargv[i + 1]) * 1024;
    demobuffer = static_cast<byte*>((doom_malloc(maxsize)));
    demoend = demobuffer + maxsize;

    demorecording = true;
}

void beginRecording()
{
    demo_p = demobuffer;

    *demo_p++ = VERSION;
    *demo_p++ = gameskill;
    *demo_p++ = gameepisode;
    *demo_p++ = gamemap;
    *demo_p++ = deathmatch;
    *demo_p++ = respawnparm;
    *demo_p++ = fastparm;
    *demo_p++ = nomonsters;
    *demo_p++ = consoleplayer;

    for (int i = 0; i < MAXPLAYERS; i++)
        *demo_p++ = playeringame[i];
}

//
// gPlayDemo
//

void deferPlayDemo(const char* name)
{
    defdemoname = name;
    gameaction = ga_playdemo;
}

void doPlayDemo()
{
    Skill skill;
    int episode, map;

    gameaction = ga_nothing;
    demobuffer = demo_p = static_cast<byte*>((Doom::cacheLumpName(defdemoname)));
    byte demo_version = *demo_p++;
    if (demo_version != VERSION
        && demo_version != 109) // Demos seem to run fine with version 109
    {
        //doom_print("Demo is from a different game version! Demo Verson = %i, this version = %i\n", (int)demo_version, VERSION);
        doom_print("Demo is from a different game version! Demo Verson = ");
        doom_print(doom_itoa(static_cast<int>(demo_version), 10));
        doom_print(", this version = ");
        doom_print(doom_itoa(VERSION, 10));
        doom_print("\n");
        gameaction = ga_nothing;
        return;
    }

    skill = static_cast<Skill>((*demo_p++));
    episode = *demo_p++;
    map = *demo_p++;
    deathmatch = *demo_p++;
    respawnparm = *demo_p++;
    fastparm = *demo_p++;
    nomonsters = *demo_p++;
    consoleplayer = *demo_p++;

    for (int i = 0; i < MAXPLAYERS; i++)
        playeringame[i] = *demo_p++;
    if (playeringame[1])
    {
        netgame = true;
        netdemo = true;
    }

    // don't spend a lot of time in loadlevel
    precache = false;
    initNewGame(skill, episode, map);
    precache = true;

    usergame = false;
    demoplayback = true;
}

//
// startTimeDemo
//
void startTimeDemo(char* name)
{
    nodrawers = Doom::checkParm("-nodraw");
    noblit = Doom::checkParm("-noblit");
    timingdemo = true;
    singletics = true;

    defdemoname = name;
    gameaction = ga_playdemo;
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
    int endtime;

    if (timingdemo)
    {
        endtime = currentTic();
        //fatalError("Error: timed %i gametics in %i realtics", gametic
        //        , endtime - starttime);

        doom_strcpy(error_buf, "Error: timed ");
        doom_concat(error_buf, doom_itoa(gametic, 10));
        doom_concat(error_buf, " gametics in ");
        doom_concat(error_buf, doom_itoa(endtime - starttime, 10));
        doom_concat(error_buf, " realtics");
        fatalError(error_buf);
    }

    if (demoplayback)
    {
        if (singledemo)
            quitGame();

        demoplayback = false;
        netdemo = false;
        netgame = false;
        deathmatch = false;
        playeringame[1] = playeringame[2] = playeringame[3] = 0;
        respawnparm = false;
        fastparm = false;
        nomonsters = false;
        consoleplayer = 0;
        Doom::advanceDemo();
        return true;
    }

    if (demorecording)
    {
        *demo_p++ = DEMOMARKER;
        Doom::writeFile(
            demoname, demobuffer, static_cast<int>((demo_p - demobuffer)));
        doom_free(demobuffer);
        demorecording = false;
        //fatalError("Error: Demo %s recorded", demoname);

        doom_strcpy(error_buf, "Error: Demo ");
        doom_concat(error_buf, demoname);
        doom_concat(error_buf, " recorded");
        fatalError(error_buf);
    }

    return false;
}

} // namespace Doom
