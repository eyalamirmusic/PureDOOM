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

#include "../Host/Platform.h"

#include "../UI/AutomapTypes.h"
#include "DoomMain.h"
#include "GameDefs.h"
#include "MapSpawns.h"
#include "Strings.h" // Data.
#include "../UI/Hud.h"
#include "Args.h"
#include "../UI/Menu.h"
#include "ConfigTypes.h"
#include "../Sim/Random.h"
#include "../Sim/SimDefs.h"
#include "SoundData.h"
#include "../UI/StatusBarTypes.h"
#include "../Wad/WadFile.h"
#include "../UI/IntermissionTypes.h"

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
#include "../UI/MenuState.h"
#include "../UI/MenuSettings.h"
#include "../Containers.h"

#include "Config.h"
#include "../Host/System.h"
#include "../Render/Main.h"
#include "../Sim/Mobj.h"
#include "../Sim/Movement.h"
#include "Sound.h"
#include "../Sim/Random.h"
#include "../Render/Sky.h"
#include "../Render/Video.h"

#include <algorithm>
// The run speed, and the cap on the combined forward/side move. forwardmove[]
// holds the small raw integers that go into ticcmd's char fields - P_MovePlayer is
// what turns them into a velocity, by multiplying by 2048. So the cap is that raw
// integer, not a whole-unit conversion of it.
//
// Not a constexpr candidate: Doom::movementSpeeds().forwardmove[1] is a runtime
// accessor into per-session state (Doom::doomLoop's -turbo handling scales it at
// startup), not a compile-time constant. It was a macro for exactly that reason
// until Step 9's last pass, which is a reason not to be a constant and never was a
// reason to be a macro; maxPlayerMove() is defined in namespace Doom below.

// Prototypes for other subsystems' functions.
void Doom::spawnPlayer(Doom::MapThing& mthing);
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

// The level's progress (the intermission totals, and leveltime over in p_tick) is a
// Doom::LevelStats owned by the Engine now; these are references onto it.

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
bool* mousebuttons = &Doom::ticcmdInput().mousearray[1];
bool* joybuttons = &Doom::ticcmdInput().joyarray[1];

int savegameslot;
std::string savedescription;

// bodyqueslot is a Doom::CorpseQueue owned by the Engine now; this is a reference onto it
// (doomstat.h externs it, bodyque[] alongside it does not need one) (REFACTOR.md, Step 5).

void* statcopy; // for statistics driver

bool secretexit;

std::string defdemoname;

// Other subsystems' globals this file reads (declared at global scope so the
// namespace code below resolves them to ::, not Doom::).
extern Doom::Array<std::string_view, 4> player_names; // hu_stuff

namespace Doom
{

constexpr int SAVEGAMESIZE = 0x2c000;

// The .dsg's description field width. UI/Menu sizes its ten slot buffers from
// menuSaveStringSize and reads a whole field into one of them, so the two must agree or that read
// either truncates or overruns the buffer. They were both a bare 24 with nothing relating them -
// see MenuState.h - so the assert is the compile-time link that was missing.
constexpr int SAVESTRINGSIZE = 24;
static_assert(
    SAVESTRINGSIZE == menuSaveStringSize,
    "the savegame description field and the menu's slot buffers must be the "
    "same size: Menu::readSaveStrings reads a whole field into one buffer");
constexpr int TURBOTHRESHOLD = 0x32;
constexpr int SLOWTURNTICS = 6;
constexpr int NUMKEYS = 256;

// The cap on the combined forward/side move - see the note above the includes for
// why it is a run-time read rather than a constant.
int maxPlayerMove()
{
    return movementSpeeds().forwardmove[1];
}
constexpr int BODYQUESIZE = 32;
constexpr int VERSIONSIZE = 16;
constexpr int DEMOMARKER = 0x80;

// Forward declarations so call order needs no rearranging.
bool checkDemoStatus();
void readDemoTiccmd(Ticcmd& cmd);
void writeDemoTiccmd(Ticcmd& cmd);
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

int cmdChecksum(Ticcmd& cmd)
{
    int sum = 0;

    for (int i = 0; i < static_cast<int>((sizeof(cmd) / 4 - 1)); i++)
        sum += (reinterpret_cast<int*>(&cmd))[i];

    return sum;
}

//
// buildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer.
// If recording a demo, write it out
//
void buildTiccmd(Ticcmd& cmd)
{
    auto& config = inputConfig();
    auto& input = ticcmdInput();
    auto& speeds = movementSpeeds();
    auto& net = netState();
    auto& pending = pendingCommands();

    bool strafe;
    bool bstrafe;
    int speed;
    int tspeed;
    int forward;
    int side;

    Ticcmd* base;

    base = baseTiccmd(); // empty, or external driver
    doom_memcpy(&cmd, base, sizeof(cmd));

    cmd.consistancy =
        net.consistancy[playerState().consoleplayer][net.maketic % BACKUPTICS];

    strafe = input.gamekeydown[config.key_strafe]
             || mousebuttons[config.mousebstrafe] || joybuttons[config.joybstrafe];

    bool running = config.always_run
                       ? (input.gamekeydown[config.key_speed] ? false : true)
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
            cmd.angleturn -= speeds.angleturn[tspeed];
        if (input.gamekeydown[config.key_left])
            cmd.angleturn += speeds.angleturn[tspeed];
        if (input.joyxmove > 0)
            cmd.angleturn -= speeds.angleturn[tspeed];
        if (input.joyxmove < 0)
            cmd.angleturn += speeds.angleturn[tspeed];
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
    cmd.chatchar = Doom::dequeueChatChar();

    if (input.gamekeydown[config.key_fire] || mousebuttons[config.mousebfire]
        || joybuttons[config.joybfire])
        cmd.buttons |= BT_ATTACK;

    if (input.gamekeydown[config.key_use] || joybuttons[config.joybuse])
    {
        cmd.buttons |= BT_USE;
        // clear double clicks if hit use button
        input.dclicks = 0;
    }

    // chainsaw overrides
    for (int i = 0; i < numWeapons - 1; i++)
        if (input.gamekeydown['1' + i])
        {
            cmd.buttons |= BT_CHANGE;
            cmd.buttons |= i << BT_WEAPONSHIFT;
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
            cmd.buttons |= BT_USE;
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
            cmd.buttons |= BT_USE;
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
        cmd.angleturn -= input.mousex * 0x8;

    input.mousex = input.mousey = 0;

    const int maxmove = maxPlayerMove();

    if (forward > maxmove)
        forward = maxmove;
    else if (forward < -maxmove)
        forward = -maxmove;
    if (side > maxmove)
        side = maxmove;
    else if (side < -maxmove)
        side = -maxmove;

    cmd.forwardmove += forward;
    cmd.sidemove += side;

    // special buttons
    if (pending.sendpause)
    {
        pending.sendpause = false;
        cmd.buttons = BT_SPECIAL | BTS_PAUSE;
    }

    if (pending.sendsave)
    {
        pending.sendsave = false;
        cmd.buttons = BT_SPECIAL | BTS_SAVEGAME | (savegameslot << BTS_SAVESHIFT);
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
    if ((mode == GameMode::Commercial)
        || (static_cast<int>(mode) == static_cast<int>(GameMission::PackTnt))
        || (static_cast<int>(mode) == static_cast<int>(GameMission::PackPlut)))
    {
        sky.skytexture = Doom::textureNumForName("SKY3");
        if (session.gamemap < 12)
            sky.skytexture = Doom::textureNumForName("SKY1");
        else if (session.gamemap < 21)
            sky.skytexture = Doom::textureNumForName("SKY2");
    }

    if (flow.wipegamestate == GameState::Level)
        flow.wipegamestate = GS_FORCE_WIPE;

    flow.gamestate = GameState::Level;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (players_.playeringame[i]
            && players_.players[i].playerstate == PlayerLifeState::Dead)
            players_.players[i].playerstate = PlayerLifeState::Reborn;
        doom_memset(players_.players[i].frags, 0, sizeof(players_.players[i].frags));
    }

    Doom::setupLevel(session.gameepisode, session.gamemap, 0, session.gameskill);
    players_.displayplayer = players_.consoleplayer; // view the guy you are playing
    timeDemo().starttime = currentTic();
    flow.gameaction = GameAction::Nothing;

    // clear cmd building stuff
    input.gamekeydown.fill(false);
    input.joyxmove = input.joyymove = 0;
    input.mousex = input.mousey = 0;
    pendingCommands().sendpause = pendingCommands().sendsave =
        refreshFlags().paused = false;
    // Both views start at [1] so that a [-1] index is legal, which is why they run
    // one short of their arrays - and why the count comes off the array rather than
    // being written out again here.
    std::fill_n(mousebuttons, input.mousearray.size() - 1, false);
    std::fill_n(joybuttons, input.joyarray.size() - 1, false);
}

//
// gameResponder
// Get info needed to make ticcmd_ts for the players.
//
bool gameResponder(Event& ev)
{
    auto& flow = gameFlow();
    auto& demo = demoState();
    auto& players_ = playerState();
    auto& input = ticcmdInput();

    // allow spy mode changes even during the demo
    if (flow.gamestate == GameState::Level && ev.type == EventType::KeyDown
        && ev.data1 == KEY_F12 && (demo.singledemo || !gameSession().deathmatch))
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
    if (flow.gameaction == GameAction::Nothing && !demo.singledemo
        && (demo.demoplayback || flow.gamestate == GameState::DemoScreen))
    {
        if (ev.type == EventType::KeyDown
            || (ev.type == EventType::Mouse && ev.data1)
            || (ev.type == EventType::Joystick && ev.data1))
        {
            startControlPanel();
            return true;
        }
        return false;
    }

    if (flow.gamestate == GameState::Level)
    {
#if 0 
        if (devparm && ev.type == EventType::KeyDown && ev.data1 == ';')
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

    if (flow.gamestate == GameState::Finale)
    {
        if (Doom::finaleResponder(ev))
            return true; // finale ate the event
    }

    switch (ev.type)
    {
        case EventType::KeyDown:
            if (ev.data1 == KEY_PAUSE)
            {
                pendingCommands().sendpause = true;
                return true;
            }
            if (ev.data1 < NUMKEYS)
                input.gamekeydown[ev.data1] = true;
            return true; // eat key down events

        case EventType::KeyUp:
            if (ev.data1 < NUMKEYS)
                input.gamekeydown[ev.data1] = false;
            return false; // always let key up events filter down

        case EventType::Mouse:
        {
            const auto sensitivity = menuSettings().mouseSensitivity;
            mousebuttons[0] = ev.data1 & 1;
            mousebuttons[1] = ev.data1 & 2;
            mousebuttons[2] = ev.data1 & 4;
            input.mousex = ev.data2 * (sensitivity + 5) / 10;
            input.mousey = ev.data3 * (sensitivity + 5) / 10;
            return true; // eat events
        }

        case EventType::Joystick:
            joybuttons[0] = ev.data1 & 1;
            joybuttons[1] = ev.data1 & 2;
            joybuttons[2] = ev.data1 & 4;
            joybuttons[3] = ev.data1 & 8;
            input.joyxmove = ev.data2;
            input.joyymove = ev.data3;
            return true; // eat events
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
            && players_.players[i].playerstate == PlayerLifeState::Reborn)
            doReborn(i);

    // do things to change the game state
    while (flow.gameaction != GameAction::Nothing)
    {
        switch (flow.gameaction)
        {
            case GameAction::LoadLevel:
                doLoadLevel();
                break;
            case GameAction::NewGame:
                doNewGame();
                break;
            case GameAction::LoadGame:
                doLoadGame();
                break;
            case GameAction::SaveGame:
                doSaveGame();
                break;
            case GameAction::PlayDemo:
                doPlayDemo();
                break;
            case GameAction::Completed:
                doCompleted();
                break;
            case GameAction::Victory:
                Doom::startFinale();
                break;
            case GameAction::WorldDone:
                doWorldDone();
                break;
            case GameAction::Screenshot:
                Doom::writeScreenshot();
                flow.gameaction = GameAction::Nothing;
                break;
            case GameAction::Nothing:
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
                readDemoTiccmd(*cmd);
            if (demo.demorecording)
                writeDemoTiccmd(*cmd);

            // check for turbo cheats
            if (cmd->forwardmove > TURBOTHRESHOLD && !(clock.gametic & 31)
                && ((clock.gametic >> 5) & 3) == i)
            {
                static std::string turbomessage;
                //doom_sprintf(turbomessage, "%s is turbo!", player_names[i]);
                turbomessage = concat(player_names[i], " is turbo!");
                players_.players[players_.consoleplayer].message =
                    turbomessage.c_str();
            }

            if (gameSession().netgame && !demo.netdemo
                && !(clock.gametic % net.ticdup))
            {
                if (clock.gametic > BACKUPTICS
                    && net.consistancy[i][buf] != cmd->consistancy)
                {
                    fatalError("Error: consistency failure (",
                               cmd->consistancy,
                               " should be ",
                               net.consistancy[i][buf],
                               ")");
                }
                if (players_.players[i].mo)
                    // Vanilla's checksum is the low 16 bits of the raw fixed x,
                    // truncated into a short - not the whole-unit part of it.
                    net.consistancy[i][buf] =
                        static_cast<short>(players_.players[i].mo->x.raw);
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
                        if (savedescription.empty())
                            savedescription = "NET GAME";
                        savegameslot =
                            (players_.players[i].cmd.buttons & BTS_SAVEMASK)
                            >> BTS_SAVESHIFT;
                        flow.gameaction = GameAction::SaveGame;
                        break;
                }
            }
        }
    }

    // do main actions
    switch (flow.gamestate)
    {
        case GameState::Level:
            Doom::ticker();
            Doom::statusBarTicker();
            Doom::automapTicker();
            Doom::hudTicker();
            break;

        case GameState::Intermission:
            Doom::intermissionTicker();
            break;

        case GameState::Finale:
            Doom::finaleTicker();
            break;

        case GameState::DemoScreen:
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
    Array<int, MAXPLAYERS> frags;
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
    p->playerstate = PlayerLifeState::Live;
    p->health = MAXHEALTH;
    p->readyweapon = p->pendingweapon = WeaponType::Pistol;
    p->weaponowned[toIndex(WeaponType::Fist)] = true;
    p->weaponowned[toIndex(WeaponType::Pistol)] = true;
    p->ammo[toIndex(AmmoType::Clip)] = 50;

    std::copy_n(ammo.maxammo.begin(), numAmmo, p->maxammo);
}

//
// checkSpot
// Returns false if the player cannot be respawned
// at the given MapThing spot
// because something is occupying it
//

bool checkSpot(int playernum, MapThing& mthing)
{
    auto& players_ = playerState();
    auto& corpses = corpseQueue();

    fixed_t x;
    fixed_t y;
    SubSector* ss;
    Mobj* mo;

    if (!players_.players[playernum].mo)
    {
        // first spawn of level, before corpses
        for (int i = 0; i < playernum; i++)
            if (players_.players[i].mo->x == Doom::Fixed::fromInt(mthing.x)
                && players_.players[i].mo->y == Doom::Fixed::fromInt(mthing.y))
                return false;
        return true;
    }

    x = Doom::Fixed::fromInt(mthing.x);
    y = Doom::Fixed::fromInt(mthing.y);

    if (!Doom::checkPosition(*players_.players[playernum].mo, x, y))
        return false;

    // flush an old corpse if needed
    if (corpses.bodyqueslot >= BODYQUESIZE)
        Doom::removeMobj(*corpses.bodyque[corpses.bodyqueslot % BODYQUESIZE]);
    corpses.bodyque[corpses.bodyqueslot % BODYQUESIZE] =
        players_.players[playernum].mo;
    corpses.bodyqueslot++;

    // spawn a teleport fog
    ss = Doom::pointInSubsector(x, y);
    const auto anFine = (ang45 * (mthing.angle / 45)).fineIndex();

    mo = Doom::spawnMobj(x + 20 * finecosine[anFine],
                         y + 20 * finesine[anFine],
                         ss->sector->floorheight,
                         MobjType::Tfog);

    // viewz is the raw sentinel 1 that Doom::setupLevel plants, not one world unit.
    if (players_.players[players_.consoleplayer].viewz != fixed_t {1})
        Doom::startSound(mo, SfxEnum::Telept); // don't start sound on first frame

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

    selections =
        static_cast<int>((spawns.deathmatch_p - spawns.deathmatchstarts.data()));
    if (selections < 4)
    {
        fatalError("Error: Only ", selections, " deathmatch spots, 4 required");
    }

    for (int j = 0; j < 20; j++)
    {
        i = Doom::randomness().forPlay() % selections;
        if (checkSpot(playernum, spawns.deathmatchstarts[i]))
        {
            spawns.deathmatchstarts[i].type = playernum + 1;
            Doom::spawnPlayer(spawns.deathmatchstarts[i]);
            return;
        }
    }

    // no good spot, so the player will probably get stuck
    Doom::spawnPlayer(spawns.playerstarts[playernum]);
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
        gameFlow().gameaction = GameAction::LoadLevel;
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

        if (checkSpot(playernum, spawns.playerstarts[playernum]))
        {
            Doom::spawnPlayer(spawns.playerstarts[playernum]);
            return;
        }

        // try to spawn at one of the other players spots
        for (int i = 0; i < MAXPLAYERS; i++)
        {
            if (checkSpot(playernum, spawns.playerstarts[i]))
            {
                spawns.playerstarts[i].type = playernum + 1; // fake as other player
                Doom::spawnPlayer(spawns.playerstarts[i]);
                spawns.playerstarts[i].type = i + 1; // restore
                return;
            }
            // he's going to be inside something.  Too bad.
        }
        Doom::spawnPlayer(spawns.playerstarts[playernum]);
    }
}

void takeScreenshot()
{
    gameFlow().gameaction = GameAction::Screenshot;
}

//
// doCompleted
//
void exitLevel()
{
    secretexit = false;
    gameFlow().gameaction = GameAction::Completed;
}

// Here's for the german edition.
void secretExitLevel()
{
    // IF NO WOLF3D LEVELS, NO SECRET EXIT!
    if ((gameVersion().gamemode == GameMode::Commercial)
        && (Doom::wad().find("map31") < 0))
        secretexit = false;
    else
        secretexit = true;
    gameFlow().gameaction = GameAction::Completed;
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

    flow.gameaction = GameAction::Nothing;

    for (int i = 0; i < MAXPLAYERS; i++)
        if (players_.playeringame[i])
            playerFinishLevel(i); // take away cards and stuff

    if (overlay.automapactive)
        Doom::stopAutomap();

    if (mode != GameMode::Commercial)
        switch (session.gamemap)
        {
            case 8:
                flow.gameaction = GameAction::Victory;
                return;
            case 9:
                for (auto& player: players_.players)
                    player.didsecret = true;
                break;
        }

    if ((session.gamemap == 8) && (mode != GameMode::Commercial))
    {
        // victory
        flow.gameaction = GameAction::Victory;
        return;
    }

    if ((session.gamemap == 9) && (mode != GameMode::Commercial))
    {
        // exit secret level
        for (auto& player: players_.players)
            player.didsecret = true;
    }

    wminfo_.didsecret = players_.players[players_.consoleplayer].didsecret;
    wminfo_.epsd = session.gameepisode - 1;
    wminfo_.last = session.gamemap - 1;

    // wminfo.next is 0 biased, unlike gamemap
    if (mode == GameMode::Commercial)
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
    if (mode == GameMode::Commercial)
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

    flow.gamestate = GameState::Intermission;
    refreshFlags().viewactive = false;
    overlay.automapactive = false;

    // wminfo_'s layout (IntermissionStart/IntermissionPlayer, including their
    // doom_boolean-turned-bool fields) is nominally an external ABI here - statcopy
    // is a raw address parsed from the DOS-era -statcopy flag, for some other
    // process to read. The doom_boolean -> bool flip moved that layout. Nothing in
    // this repository consumes statcopy, and the flag cannot work against a modern
    // process anyway (an absolute pointer between two processes), so this is left
    // to move with the rest of the struct rather than carved out.
    if (statcopy)
        doom_memcpy(statcopy, &wminfo_, sizeof(wminfo_));

    Doom::startIntermission(&wminfo_);
}

//
// worldDone
//
void worldDone()
{
    gameFlow().gameaction = GameAction::WorldDone;

    if (secretexit)
    {
        auto& players_ = playerState();
        players_.players[players_.consoleplayer].didsecret = true;
    }

    if (gameVersion().gamemode == GameMode::Commercial)
    {
        switch (gameSession().gamemap)
        {
            case 15:
            case 31:
                if (!secretexit)
                    break;
                // A secret exit from 15 or 31 reaches the finale, so the
                // fallthrough is the point of the case rather than an omission.
                [[fallthrough]];
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

    flow.gamestate = GameState::Level;
    gameSession().gamemap = intermissionInfo().wminfo.next + 1;
    doLoadLevel();
    flow.gameaction = GameAction::Nothing;
    refreshFlags().viewactive = true;
}

//
// G_InitFromSavegame
// Can be called by the startup code or the menu task.
//

void loadGame(std::string_view name)
{
    saveGameState().name = name;
    gameFlow().gameaction = GameAction::LoadGame;
}

void doLoadGame()
{
    auto& save = saveGameState();
    auto& session = gameSession();
    auto& players_ = playerState();

    int a, b, c;

    gameFlow().gameaction = GameAction::Nothing;

    // readFile() fills the owner directly; buffer is left a view onto it, as it is a
    // view onto the framebuffer scratch on the save path (see SaveGameState.h).
    Doom::readFile(save.name, save.loadStorage);
    save.buffer = save.loadStorage.data();
    save.cursor = save.buffer + SAVESTRINGSIZE;

    // skip the description field
    //doom_sprintf(vcheck, "version %i", VERSION);
    auto vcheck = concat("version ", VERSION);
    // Bounded: the on-disk version field is VERSIONSIZE bytes zero-padded by
    // fillField, so it is only NUL-terminated when short of the full width.
    if (vcheck != nameView(reinterpret_cast<const char*>(save.cursor), VERSIONSIZE))
        return; // bad version
    save.cursor += VERSIONSIZE;

    session.gameskill = static_cast<Skill>((*save.cursor++));
    session.gameepisode = *save.cursor++;
    session.gamemap = *save.cursor++;
    for (bool& ingame: players_.playeringame)
        ingame = *save.cursor++;

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

    // done: loadStorage (not buffer, which also views the save-path framebuffer scratch)
    // owns this memory now; no manual free needed.

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
void saveGame(int slot, std::string_view description)
{
    savegameslot = slot;
    savedescription = description;
    pendingCommands().sendsave = true;
}

void doSaveGame()
{
    auto& save = saveGameState();
    auto& stats = levelStats();
    auto& session = gameSession();
    auto& players_ = playerState();

    int length;

#if 0
    if (Doom::checkParm("-cdrom"))
        doom_sprintf(name, "c:\\doomdata\\"SAVEGAMENAME"%d.dsg", savegameslot);
#endif
    //doom_sprintf(name, SAVEGAMENAME"%d.dsg", savegameslot);
    auto name = concat(SAVEGAMENAME, savegameslot, ".dsg");

    save.cursor = save.buffer = screens[1] + 0x4000;

    fillField(save.cursor, SAVESTRINGSIZE, savedescription);
    save.cursor += SAVESTRINGSIZE;
    //doom_sprintf(name2, "version %i", VERSION);
    fillField(save.cursor, VERSIONSIZE, concat("version ", VERSION));
    save.cursor += VERSIONSIZE;

    *save.cursor++ = static_cast<byte>(session.gameskill);
    *save.cursor++ = session.gameepisode;
    *save.cursor++ = session.gamemap;
    for (bool ingame: players_.playeringame)
        *save.cursor++ = ingame;
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
    Doom::writeFile(name, save.buffer, length);
    gameFlow().gameaction = GameAction::Nothing;
    savedescription.clear();

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
    gameFlow().gameaction = GameAction::NewGame;
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
        false;
    opts.respawnparm = false;
    opts.fastparm = false;
    opts.nomonsters = false;
    players_.consoleplayer = 0;
    initNewGame(deferred.d_skill, deferred.d_episode, deferred.d_map);
    gameFlow().gameaction = GameAction::Nothing;
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

    if (skill > Skill::Nightmare)
        skill = Skill::Nightmare;

    // This was quite messy with SPECIAL and commented parts.
    // Supposedly hacks to make the latest edition work.
    // It might not work properly.
    if (episode < 1)
        episode = 1;

    if (mode == GameMode::Retail)
    {
        if (episode > 4)
            episode = 4;
    }
    else if (mode == GameMode::Shareware)
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

    if ((map > 9) && (mode != GameMode::Commercial))
        map = 9;

    Doom::randomness().clear();

    if (skill == Skill::Nightmare || opts.respawnparm)
        session.respawnmonsters = true;
    else
        session.respawnmonsters = false;

    if (opts.fastparm
        || (skill == Skill::Nightmare && session.gameskill != Skill::Nightmare))
    {
        for (int i = toIndex(StateNum::SargRun1); i <= toIndex(StateNum::SargPain2);
             i++)
            states[i].tics >>= 1;
        mobjinfo[toIndex(MobjType::Bruisershot)].speed = (20 * FRACUNIT).raw;
        mobjinfo[toIndex(MobjType::Headshot)].speed = (20 * FRACUNIT).raw;
        mobjinfo[toIndex(MobjType::Troopshot)].speed = (20 * FRACUNIT).raw;
    }
    else if (skill != Skill::Nightmare && session.gameskill == Skill::Nightmare)
    {
        for (int i = toIndex(StateNum::SargRun1); i <= toIndex(StateNum::SargPain2);
             i++)
            states[i].tics <<= 1;
        mobjinfo[toIndex(MobjType::Bruisershot)].speed = (15 * FRACUNIT).raw;
        mobjinfo[toIndex(MobjType::Headshot)].speed = (10 * FRACUNIT).raw;
        mobjinfo[toIndex(MobjType::Troopshot)].speed = (10 * FRACUNIT).raw;
    }

    // force players to be initialized upon first level load
    for (auto& player: playerState().players)
        player.playerstate = PlayerLifeState::Reborn;

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
    if (mode == GameMode::Commercial)
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

void readDemoTiccmd(Ticcmd& cmd)
{
    auto& demo = demoState();

    if (*demo.demo_p == DEMOMARKER)
    {
        // end of demo data stream
        checkDemoStatus();
        return;
    }
    cmd.forwardmove = (static_cast<signed char>(*demo.demo_p++));
    cmd.sidemove = (static_cast<signed char>(*demo.demo_p++));
    cmd.angleturn = (static_cast<unsigned char>(*demo.demo_p++)) << 8;
    cmd.buttons = static_cast<unsigned char>(*demo.demo_p++);
}

void writeDemoTiccmd(Ticcmd& cmd)
{
    auto& demo = demoState();

    if (ticcmdInput().gamekeydown['q']) // press q to end demo recording
        checkDemoStatus();
    *demo.demo_p++ = cmd.forwardmove;
    *demo.demo_p++ = cmd.sidemove;
    *demo.demo_p++ = (cmd.angleturn + 128) >> 8;
    *demo.demo_p++ = cmd.buttons;
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
void recordDemo(std::string_view name)
{
    auto& demo = demoState();

    int i;
    int maxsize;

    demo.usergame = false;
    demo.demoname = concat(name, ".lmp");
    maxsize = 0x20000;
    i = Doom::checkParm("-maxdemo");
    if (i && i < myargCount() - 1)
        maxsize = parseInt(myargv[i + 1]) * 1024;
    demo.demoRecordBuffer.resize(maxsize);
    demo.demobuffer = demo.demoRecordBuffer.data();
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
    *demo.demo_p++ = static_cast<byte>(session.gameskill);
    *demo.demo_p++ = session.gameepisode;
    *demo.demo_p++ = session.gamemap;
    *demo.demo_p++ = session.deathmatch;
    *demo.demo_p++ = opts.respawnparm;
    *demo.demo_p++ = opts.fastparm;
    *demo.demo_p++ = opts.nomonsters;
    *demo.demo_p++ = players_.consoleplayer;

    for (bool ingame: players_.playeringame)
        *demo.demo_p++ = ingame;
}

//
// gPlayDemo
//

void deferPlayDemo(std::string_view name)
{
    defdemoname = name;
    gameFlow().gameaction = GameAction::PlayDemo;
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

    flow.gameaction = GameAction::Nothing;
    demo.demobuffer = demo.demo_p =
        static_cast<byte*>((Doom::cacheLumpName(defdemoname)));
    byte demo_version = *demo.demo_p++;
    if (demo_version != VERSION
        && demo_version != 109) // Demos seem to run fine with version 109
    {
        //doom_print("Demo is from a different game version! Demo Verson = %i, this version = %i\n", (int)demo_version, VERSION);
        print("Demo is from a different game version! Demo Verson = ",
              static_cast<int>(demo_version),
              ", this version = ",
              VERSION,
              "\n");
        flow.gameaction = GameAction::Nothing;
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

    for (bool& ingame: players_.playeringame)
        ingame = *demo.demo_p++;
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
void startTimeDemo(std::string_view name)
{
    auto& refresh = refreshFlags();

    refresh.nodrawers = Doom::checkParm("-nodraw");
    refresh.noblit = Doom::checkParm("-noblit");
    timeDemo().timingdemo = true;
    engineParams().singletics = true;

    defdemoname = name;
    gameFlow().gameaction = GameAction::PlayDemo;
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

bool checkDemoStatus()
{
    auto& demo = demoState();
    auto& timedemo = timeDemo();

    int endtime;

    if (timedemo.timingdemo)
    {
        endtime = currentTic();
        fatalError("Error: timed ",
                   gameClock().gametic,
                   " gametics in ",
                   endtime - timedemo.starttime,
                   " realtics");
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
            players_.playeringame[3] = false;
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
        // demoRecordBuffer (not demobuffer, which also views the playback lump) owns this
        // memory now; no manual free needed.
        demo.demorecording = false;
        fatalError("Error: Demo ", demo.demoname, " recorded");
    }

    return false;
}

} // namespace Doom
