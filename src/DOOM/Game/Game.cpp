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
#include "../w_wad.h"
#include "../wi_stuff.h"
#include "../g_game.h"

#include "CorpseQueue.h"
#include "DemoState.h"
#include "Game.h"
#include "GameClock.h"
#include "GameFlow.h"
#include "GameSession.h"
#include "IntermissionInfo.h"
#include "LevelStats.h"
#include "PlayerState.h"
#include "RefreshFlags.h"

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
void P_SpawnPlayer(mapthing_t* mthing);
void R_ExecuteSetViewSize(void);

// gameaction, gamestate and wipegamestate are a Doom::GameFlow owned by the Engine now; these
// (and the extern wipegamestate below) are references onto it (REFACTOR.md, Step 5).
gameaction_t& gameaction = Doom::gameFlow().gameaction;
gamestate_t& gamestate = Doom::gameFlow().gamestate;

// The current game's rules are a Doom::GameSession owned by the Engine now; these (and
// netgame/deathmatch below) are references onto it (REFACTOR.md, Step 5).
skill_t& gameskill = Doom::gameSession().gameskill;
doom_boolean& respawnmonsters = Doom::gameSession().respawnmonsters;
int& gameepisode = Doom::gameSession().gameepisode;
int& gamemap = Doom::gameSession().gamemap;

// paused (with viewactive/nodrawers/noblit below) is a Doom::RefreshFlags owned by the
// Engine now; these are references onto it (REFACTOR.md, Step 5).
doom_boolean& paused = Doom::refreshFlags().paused;
doom_boolean sendpause; // send a pause event next tic
doom_boolean sendsave; // send a save event next tic

// usergame (with the demo flags below) is a Doom::DemoState owned by the Engine now; these
// are references onto it (REFACTOR.md, Step 5).
doom_boolean& usergame = Doom::demoState().usergame; // ok to save / end game

doom_boolean timingdemo; // if true, exit with report on completion
doom_boolean& nodrawers = Doom::refreshFlags().nodrawers; // comparative timing
doom_boolean& noblit = Doom::refreshFlags().noblit; // comparative timing
int starttime; // for comparative timing purposes

doom_boolean& viewactive = Doom::refreshFlags().viewactive;

doom_boolean& deathmatch = Doom::gameSession().deathmatch; // net deathmatch
doom_boolean& netgame = Doom::gameSession().netgame; // packets are broadcast
// The player roster and view selection is a Doom::PlayerState owned by the Engine now;
// these are references onto it (the arrays as references-to-array) (REFACTOR.md, Step 5).
doom_boolean (&playeringame)[MAXPLAYERS] = Doom::playerState().playeringame;
player_t (&players)[MAXPLAYERS] = Doom::playerState().players;

int& consoleplayer = Doom::playerState().consoleplayer; // taking events and displaying
int& displayplayer = Doom::playerState().displayplayer; // view being displayed
int& gametic = Doom::gameClock().gametic; // Doom::GameClock (Engine member)

// The level's progress (levelstarttic + the intermission totals, and leveltime over in
// p_tick) is a Doom::LevelStats owned by the Engine now; these are references onto it.
int& levelstarttic = Doom::levelStats().levelstarttic; // gametic at level start
int& totalkills = Doom::levelStats().totalkills;       // for intermission
int& totalitems = Doom::levelStats().totalitems;
int& totalsecret = Doom::levelStats().totalsecret;

char demoname[32];
doom_boolean& demorecording = Doom::demoState().demorecording;
doom_boolean& demoplayback = Doom::demoState().demoplayback;
doom_boolean netdemo;
byte* demobuffer;
byte* demo_p;
byte* demoend;
doom_boolean& singledemo = Doom::demoState().singledemo; // quit after one demo

doom_boolean precache = true; // if true, load all graphics at start

// wminfo is a Doom::IntermissionInfo owned by the Engine now; this is a reference onto it
// (REFACTOR.md, Step 5).
wbstartstruct_t& wminfo = Doom::intermissionInfo().wminfo; // world map / intermission parms

short consistancy[MAXPLAYERS][BACKUPTICS];

byte* savebuffer;

//
// controls (have defaults)
//
int key_right;
int key_left;

int key_up;
int key_down;
int key_strafeleft;
int key_straferight;
int key_fire;
int key_use;
int key_strafe;
int key_speed;

int mousebfire;
int mousebstrafe;
int mousebforward;
int mousemove;

int joybfire;
int joybstrafe;
int joybuse;
int joybspeed;

fixed_t forwardmove[2] = {0x19, 0x32};
fixed_t sidemove[2] = {0x18, 0x28};
fixed_t angleturn[3] = {640, 1280, 320}; // + slow turn

doom_boolean gamekeydown[NUMKEYS];
int turnheld; // for accelerative turning

doom_boolean mousearray[4];
doom_boolean* mousebuttons = &mousearray[1]; // allow [-1]

// mouse values are used once
int mousex;
int mousey;

int dclicktime;
int dclickstate;
int dclicks;
int dclicktime2;
int dclickstate2;
int dclicks2;

// joystick values are repeated
int joyxmove;
int joyymove;
doom_boolean joyarray[5];
doom_boolean* joybuttons = &joyarray[1]; // allow [-1]

int savegameslot;
char savedescription[32];

// The corpse queue (bodyque + bodyqueslot) is a Doom::CorpseQueue owned by the Engine now;
// these are references onto it, bodyque as a reference-to-array (REFACTOR.md, Step 5).
mobj_t* (&bodyque)[BODYQUESIZE] = Doom::corpseQueue().bodyque;
int& bodyqueslot = Doom::corpseQueue().bodyqueslot;

void* statcopy; // for statistics driver

// DOOM Par Times
int pars[4][10] = {{0},
                   {0, 30, 75, 120, 90, 165, 180, 180, 30, 165},
                   {0, 90, 90, 90, 120, 90, 360, 240, 30, 170},
                   {0, 90, 45, 90, 150, 90, 90, 165, 30, 135}};

// DOOM II Par Times
int cpars[32] = {
    30,  90,  120, 120, 90,  150, 120, 120, 270, 90, //  1-10
    210, 150, 150, 150, 210, 150, 420, 150, 210, 150, // 11-20
    240, 150, 180, 150, 150, 300, 330, 420, 300, 180, // 21-30
    120, 30 // 31-32
};

doom_boolean secretexit;

char savename[256];

skill_t d_skill;
int d_episode;
int d_map;

const char* defdemoname;

extern gamestate_t& wipegamestate; // Doom::GameFlow (Engine member)
extern const char*& pagename; // Doom::AttractMode (Engine member)
extern doom_boolean& setsizeneeded;

// The sky texture to be used instead of the F_SKY1 dummy.
extern int skytexture;

// Other subsystems' globals this file reads (declared at global scope so the
// namespace code below resolves them to ::, not Doom::).
extern int always_run; // m_menu
extern char* player_names[4]; // hu_stuff

namespace Doom
{

// Forward declarations so call order needs no rearranging.
doom_boolean gCheckDemoStatus(void);
void gReadDemoTiccmd(ticcmd_t* cmd);
void gWriteDemoTiccmd(ticcmd_t* cmd);
void gPlayerReborn(int player);
void gInitNew(skill_t skill, int episode, int map);
void gDoReborn(int playernum);
void gDoLoadLevel(void);
void gDoNewGame(void);
void gDoLoadGame(void);
void gDoPlayDemo(void);
void gDoCompleted(void);
void gDoWorldDone(void);
void gDoSaveGame(void);

int gCmdChecksum(ticcmd_t* cmd)
{
    int i;
    int sum = 0;

    for (i = 0; i < (int) (sizeof(*cmd) / 4 - 1); i++)
        sum += ((int*) cmd)[i];

    return sum;
}

//
// gBuildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer.
// If recording a demo, write it out
//
void gBuildTiccmd(ticcmd_t* cmd)
{
    int i;
    doom_boolean strafe;
    doom_boolean bstrafe;
    int speed;
    int tspeed;
    int forward;
    int side;

    ticcmd_t* base;

    base = I_BaseTiccmd(); // empty, or external driver
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
    cmd->chatchar = HU_dequeueChatChar();

    if (gamekeydown[key_fire] || mousebuttons[mousebfire] || joybuttons[joybfire])
        cmd->buttons |= BT_ATTACK;

    if (gamekeydown[key_use] || joybuttons[joybuse])
    {
        cmd->buttons |= BT_USE;
        // clear double clicks if hit use button
        dclicks = 0;
    }

    // chainsaw overrides
    for (i = 0; i < NUMWEAPONS - 1; i++)
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
// gDoLoadLevel
//
void gDoLoadLevel(void)
{
    int i;

    // Set the sky map.
    // First thing, we have a dummy sky texture name,
    //  a flat. The data is in the WAD only because
    //  we look for an actual index, instead of simply
    //  setting one.
    skyflatnum = R_FlatNumForName(SKYFLATNAME);

    // DOOM determines the sky texture to be used
    // depending on the current episode, and the game version.
    if ((gamemode == commercial) || ((int) gamemode == (int) pack_tnt)
        || ((int) gamemode == (int) pack_plut))
    {
        skytexture = R_TextureNumForName("SKY3");
        if (gamemap < 12)
            skytexture = R_TextureNumForName("SKY1");
        else if (gamemap < 21)
            skytexture = R_TextureNumForName("SKY2");
    }

    levelstarttic = gametic; // for time calculation

    if (wipegamestate == GS_LEVEL)
        wipegamestate = (gamestate_t) (-1); // force a wipe

    gamestate = GS_LEVEL;

    for (i = 0; i < MAXPLAYERS; i++)
    {
        if (playeringame[i] && players[i].playerstate == PST_DEAD)
            players[i].playerstate = PST_REBORN;
        doom_memset(players[i].frags, 0, sizeof(players[i].frags));
    }

    P_SetupLevel(gameepisode, gamemap, 0, gameskill);
    displayplayer = consoleplayer; // view the guy you are playing
    starttime = I_GetTime();
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
// gResponder
// Get info needed to make ticcmd_ts for the players.
//
doom_boolean gResponder(event_t* ev)
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
            M_StartControlPanel();
            return true;
        }
        return false;
    }

    if (gamestate == GS_LEVEL)
    {
#if 0 
        if (devparm && ev->type == ev_keydown && ev->data1 == ';')
        {
            gDeathMatchSpawnPlayer(0);
            return true;
        }
#endif
        if (HU_Responder(ev))
            return true; // chat ate the event
        if (ST_Responder(ev))
            return true; // status window ate it
        if (AM_Responder(ev))
            return true; // automap ate it
    }

    if (gamestate == GS_FINALE)
    {
        if (F_Responder(ev))
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
// gTicker
// Make ticcmd_ts for the players.
//
void gTicker(void)
{
    int i;
    int buf;
    ticcmd_t* cmd;

    // do player reborns if needed
    for (i = 0; i < MAXPLAYERS; i++)
        if (playeringame[i] && players[i].playerstate == PST_REBORN)
            gDoReborn(i);

    // do things to change the game state
    while (gameaction != ga_nothing)
    {
        switch (gameaction)
        {
            case ga_loadlevel:
                gDoLoadLevel();
                break;
            case ga_newgame:
                gDoNewGame();
                break;
            case ga_loadgame:
                gDoLoadGame();
                break;
            case ga_savegame:
                gDoSaveGame();
                break;
            case ga_playdemo:
                gDoPlayDemo();
                break;
            case ga_completed:
                gDoCompleted();
                break;
            case ga_victory:
                F_StartFinale();
                break;
            case ga_worlddone:
                gDoWorldDone();
                break;
            case ga_screenshot:
                M_ScreenShot();
                gameaction = ga_nothing;
                break;
            case ga_nothing:
                break;
        }
    }

    // get commands, check consistancy,
    // and build new consistancy check
    buf = (gametic / ticdup) % BACKUPTICS;

    for (i = 0; i < MAXPLAYERS; i++)
    {
        if (playeringame[i])
        {
            cmd = &players[i].cmd;

            doom_memcpy(cmd, &netcmds[i][buf], sizeof(ticcmd_t));

            if (demoplayback)
                gReadDemoTiccmd(cmd);
            if (demorecording)
                gWriteDemoTiccmd(cmd);

            // check for turbo cheats
            if (cmd->forwardmove > TURBOTHRESHOLD && !(gametic & 31)
                && ((gametic >> 5) & 3) == i)
            {
                static char turbomessage[80];
                //doom_sprintf(turbomessage, "%s is turbo!", player_names[i]);
                doom_strcpy(turbomessage, player_names[i]);
                doom_concat(turbomessage, " is turbo!");
                players[consoleplayer].message = turbomessage;
            }

            if (netgame && !netdemo && !(gametic % ticdup))
            {
                if (gametic > BACKUPTICS && consistancy[i][buf] != cmd->consistancy)
                {
                    //I_Error("Error: consistency failure (%i should be %i)",
                    //        cmd->consistancy, consistancy[i][buf]);

                    doom_strcpy(error_buf, "Error: consistency failure (");
                    doom_concat(error_buf, doom_itoa(cmd->consistancy, 10));
                    doom_concat(error_buf, " should be ");
                    doom_concat(error_buf, doom_itoa(consistancy[i][buf], 10));
                    doom_concat(error_buf, ")");
                    I_Error(error_buf);
                }
                if (players[i].mo)
                    consistancy[i][buf] = players[i].mo->x;
                else
                    consistancy[i][buf] = rndindex;
            }
        }
    }

    // check for special buttons
    for (i = 0; i < MAXPLAYERS; i++)
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
                            S_PauseSound();
                        else
                            S_ResumeSound();
                        break;

                    case BTS_SAVEGAME:
                        if (!savedescription[0])
                            doom_strcpy(savedescription, "NET GAME");
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
            P_Ticker();
            ST_Ticker();
            AM_Ticker();
            HU_Ticker();
            break;

        case GS_INTERMISSION:
            WI_Ticker();
            break;

        case GS_FINALE:
            F_Ticker();
            break;

        case GS_DEMOSCREEN:
            D_PageTicker();
            break;
    }
}

//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Things
//

//
// gInitPlayer
// Called at the start.
// Called by the game initialization functions.
//
void gInitPlayer(int player)
{
    // clear everything else to defaults
    gPlayerReborn(player);
}

//
// gPlayerFinishLevel
// Can when a player completes a level.
//
void gPlayerFinishLevel(int player)
{
    player_t* p;

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
// gPlayerReborn
// Called after a player dies
// almost everything is cleared and initialized
//
void gPlayerReborn(int player)
{
    player_t* p;
    int i;
    int frags[MAXPLAYERS];
    int killcount;
    int itemcount;
    int secretcount;

    doom_memcpy(frags, players[player].frags, sizeof(frags));
    killcount = players[player].killcount;
    itemcount = players[player].itemcount;
    secretcount = players[player].secretcount;

    p = &players[player];
    doom_memset(p, 0, sizeof(*p));

    doom_memcpy(players[player].frags, frags, sizeof(players[player].frags));
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

    for (i = 0; i < NUMAMMO; i++)
        p->maxammo[i] = maxammo[i];
}

//
// gCheckSpot
// Returns false if the player cannot be respawned
// at the given mapthing_t spot
// because something is occupying it
//

doom_boolean gCheckSpot(int playernum, mapthing_t* mthing)
{
    fixed_t x;
    fixed_t y;
    subsector_t* ss;
    unsigned an;
    mobj_t* mo;
    int i;

    if (!players[playernum].mo)
    {
        // first spawn of level, before corpses
        for (i = 0; i < playernum; i++)
            if (players[i].mo->x == mthing->x << FRACBITS
                && players[i].mo->y == mthing->y << FRACBITS)
                return false;
        return true;
    }

    x = mthing->x << FRACBITS;
    y = mthing->y << FRACBITS;

    if (!P_CheckPosition(players[playernum].mo, x, y))
        return false;

    // flush an old corpse if needed
    if (bodyqueslot >= BODYQUESIZE)
        P_RemoveMobj(bodyque[bodyqueslot % BODYQUESIZE]);
    bodyque[bodyqueslot % BODYQUESIZE] = players[playernum].mo;
    bodyqueslot++;

    // spawn a teleport fog
    ss = R_PointInSubsector(x, y);
    an = (ANG45 * (mthing->angle / 45)) >> ANGLETOFINESHIFT;

    mo = P_SpawnMobj(x + 20 * finecosine[an],
                     y + 20 * finesine[an],
                     ss->sector->floorheight,
                     MT_TFOG);

    if (players[consoleplayer].viewz != 1)
        S_StartSound(mo, sfx_telept); // don't start sound on first frame

    return true;
}

//
// gDeathMatchSpawnPlayer
// Spawns a player at one of the random death match spots
// called at level load and each death
//
void gDeathMatchSpawnPlayer(int playernum)
{
    int i, j;
    int selections;

    selections = (int) (deathmatch_p - deathmatchstarts);
    if (selections < 4)
    {
        //I_Error("Error: Only %i deathmatch spots, 4 required", selections);

        doom_strcpy(error_buf, "Error: Only ");
        doom_concat(error_buf, doom_itoa(selections, 10));
        doom_concat(error_buf, " deathmatch spots, 4 required");
        I_Error(error_buf);
    }

    for (j = 0; j < 20; j++)
    {
        i = P_Random() % selections;
        if (gCheckSpot(playernum, &deathmatchstarts[i]))
        {
            deathmatchstarts[i].type = playernum + 1;
            P_SpawnPlayer(&deathmatchstarts[i]);
            return;
        }
    }

    // no good spot, so the player will probably get stuck
    P_SpawnPlayer(&playerstarts[playernum]);
}

//
// gDoReborn
//
void gDoReborn(int playernum)
{
    int i;

    if (!netgame)
    {
        // reload the level from scratch
        gameaction = ga_loadlevel;
    }
    else
    {
        // respawn at the start

        // first dissasociate the corpse
        players[playernum].mo->player = 0;

        // spawn at random spot if in death match
        if (deathmatch)
        {
            gDeathMatchSpawnPlayer(playernum);
            return;
        }

        if (gCheckSpot(playernum, &playerstarts[playernum]))
        {
            P_SpawnPlayer(&playerstarts[playernum]);
            return;
        }

        // try to spawn at one of the other players spots
        for (i = 0; i < MAXPLAYERS; i++)
        {
            if (gCheckSpot(playernum, &playerstarts[i]))
            {
                playerstarts[i].type = playernum + 1; // fake as other player
                P_SpawnPlayer(&playerstarts[i]);
                playerstarts[i].type = i + 1; // restore
                return;
            }
            // he's going to be inside something.  Too bad.
        }
        P_SpawnPlayer(&playerstarts[playernum]);
    }
}

void gScreenShot(void)
{
    gameaction = ga_screenshot;
}

//
// gDoCompleted
//
void gExitLevel(void)
{
    secretexit = false;
    gameaction = ga_completed;
}

// Here's for the german edition.
void gSecretExitLevel(void)
{
    // IF NO WOLF3D LEVELS, NO SECRET EXIT!
    if ((gamemode == commercial) && (W_CheckNumForName("map31") < 0))
        secretexit = false;
    else
        secretexit = true;
    gameaction = ga_completed;
}

void gDoCompleted(void)
{
    int i;

    gameaction = ga_nothing;

    for (i = 0; i < MAXPLAYERS; i++)
        if (playeringame[i])
            gPlayerFinishLevel(i); // take away cards and stuff

    if (automapactive)
        AM_Stop();

    if (gamemode != commercial)
        switch (gamemap)
        {
            case 8:
                gameaction = ga_victory;
                return;
            case 9:
                for (i = 0; i < MAXPLAYERS; i++)
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
        for (i = 0; i < MAXPLAYERS; i++)
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

    for (i = 0; i < MAXPLAYERS; i++)
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

    WI_Start(&wminfo);
}

//
// gWorldDone
//
void gWorldDone(void)
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
                F_StartFinale();
                break;
        }
    }
}

void gDoWorldDone(void)
{
    gamestate = GS_LEVEL;
    gamemap = wminfo.next + 1;
    gDoLoadLevel();
    gameaction = ga_nothing;
    viewactive = true;
}

//
// G_InitFromSavegame
// Can be called by the startup code or the menu task.
//

void gLoadGame(char* name)
{
    doom_strcpy(savename, name);
    gameaction = ga_loadgame;
}

void gDoLoadGame(void)
{
    int i;
    int a, b, c;
    char vcheck[VERSIONSIZE];

    gameaction = ga_nothing;

    M_ReadFile(savename, &savebuffer);
    save_p = savebuffer + SAVESTRINGSIZE;

    // skip the description field
    doom_memset(vcheck, 0, sizeof(vcheck));
    //doom_sprintf(vcheck, "version %i", VERSION);
    doom_strcpy(vcheck, "version ");
    doom_concat(vcheck, doom_itoa(VERSION, 10));
    if (doom_strcmp((const char*) save_p, (const char*) vcheck))
        return; // bad version
    save_p += VERSIONSIZE;

    gameskill = (skill_t) (*save_p++);
    gameepisode = *save_p++;
    gamemap = *save_p++;
    for (i = 0; i < MAXPLAYERS; i++)
        playeringame[i] = *save_p++;

    // load a base level
    gInitNew(gameskill, gameepisode, gamemap);

    // get the times
    a = *save_p++;
    b = *save_p++;
    c = *save_p++;
    leveltime = (a << 16) + (b << 8) + c;

    // dearchive all the modifications
    P_UnArchivePlayers();
    P_UnArchiveWorld();
    P_UnArchiveThinkers();
    P_UnArchiveSpecials();

    if (*save_p != 0x1d)
        I_Error("Error: Bad savegame");

    // done
    doom_free(savebuffer);

    if (setsizeneeded)
        R_ExecuteSetViewSize();

    // draw the pattern into the back screen
    R_FillBackScreen();
}

//
// gSaveGame
// Called by the menu task.
// Description is a 24 byte text string
//
void gSaveGame(int slot, char* description)
{
    savegameslot = slot;
    doom_strcpy(savedescription, description);
    sendsave = true;
}

void gDoSaveGame(void)
{
    char name[100];
    char name2[VERSIONSIZE];
    char* description;
    int length;
    int i;

#if 0
    if (M_CheckParm("-cdrom"))
        doom_sprintf(name, "c:\\doomdata\\"SAVEGAMENAME"%d.dsg", savegameslot);
    else
#endif
    {
        //doom_sprintf(name, SAVEGAMENAME"%d.dsg", savegameslot);
        doom_strcpy(name, SAVEGAMENAME);
        doom_concat(name, doom_itoa(savegameslot, 10));
        doom_concat(name, ".dsg");
    }
    description = savedescription;

    save_p = savebuffer = screens[1] + 0x4000;

    doom_memcpy(save_p, description, SAVESTRINGSIZE);
    save_p += SAVESTRINGSIZE;
    doom_memset(name2, 0, sizeof(name2));
    //doom_sprintf(name2, "version %i", VERSION);
    doom_strcpy(name2, "version ");
    doom_concat(name2, doom_itoa(VERSION, 10));
    doom_memcpy(save_p, name2, VERSIONSIZE);
    save_p += VERSIONSIZE;

    *save_p++ = gameskill;
    *save_p++ = gameepisode;
    *save_p++ = gamemap;
    for (i = 0; i < MAXPLAYERS; i++)
        *save_p++ = playeringame[i];
    *save_p++ = leveltime >> 16;
    *save_p++ = leveltime >> 8;
    *save_p++ = leveltime;

    P_ArchivePlayers();
    P_ArchiveWorld();
    P_ArchiveThinkers();
    P_ArchiveSpecials();

    *save_p++ = 0x1d; // consistancy marker

    length = (int) (save_p - savebuffer);
    if (length > SAVEGAMESIZE)
        I_Error("Error: Savegame buffer overrun");
    M_WriteFile(name, savebuffer, length);
    gameaction = ga_nothing;
    savedescription[0] = 0;

    players[consoleplayer].message = GGSAVED;

    // draw the pattern into the back screen
    R_FillBackScreen();
}

//
// gInitNew
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayer, playeringame[] should be set.
//

void gDeferedInitNew(skill_t skill, int episode, int map)
{
    d_skill = skill;
    d_episode = episode;
    d_map = map;
    gameaction = ga_newgame;
}

void gDoNewGame(void)
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
    gInitNew(d_skill, d_episode, d_map);
    gameaction = ga_nothing;
}

void gInitNew(skill_t skill, int episode, int map)
{
    int i;

    if (paused)
    {
        paused = false;
        S_ResumeSound();
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

    M_ClearRandom();

    if (skill == sk_nightmare || respawnparm)
        respawnmonsters = true;
    else
        respawnmonsters = false;

    if (fastparm || (skill == sk_nightmare && gameskill != sk_nightmare))
    {
        for (i = S_SARG_RUN1; i <= S_SARG_PAIN2; i++)
            states[i].tics >>= 1;
        mobjinfo[MT_BRUISERSHOT].speed = 20 * FRACUNIT;
        mobjinfo[MT_HEADSHOT].speed = 20 * FRACUNIT;
        mobjinfo[MT_TROOPSHOT].speed = 20 * FRACUNIT;
    }
    else if (skill != sk_nightmare && gameskill == sk_nightmare)
    {
        for (i = S_SARG_RUN1; i <= S_SARG_PAIN2; i++)
            states[i].tics <<= 1;
        mobjinfo[MT_BRUISERSHOT].speed = 15 * FRACUNIT;
        mobjinfo[MT_HEADSHOT].speed = 10 * FRACUNIT;
        mobjinfo[MT_TROOPSHOT].speed = 10 * FRACUNIT;
    }

    // force players to be initialized upon first level load
    for (i = 0; i < MAXPLAYERS; i++)
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
        skytexture = R_TextureNumForName("SKY3");
        if (gamemap < 12)
            skytexture = R_TextureNumForName("SKY1");
        else if (gamemap < 21)
            skytexture = R_TextureNumForName("SKY2");
    }
    else
        switch (episode)
        {
            case 1:
                skytexture = R_TextureNumForName("SKY1");
                break;
            case 2:
                skytexture = R_TextureNumForName("SKY2");
                break;
            case 3:
                skytexture = R_TextureNumForName("SKY3");
                break;
            case 4: // Special Edition sky
                skytexture = R_TextureNumForName("SKY4");
                break;
        }

    gDoLoadLevel();
}

//
// DEMO RECORDING
//

void gReadDemoTiccmd(ticcmd_t* cmd)
{
    if (*demo_p == DEMOMARKER)
    {
        // end of demo data stream
        gCheckDemoStatus();
        return;
    }
    cmd->forwardmove = ((signed char) *demo_p++);
    cmd->sidemove = ((signed char) *demo_p++);
    cmd->angleturn = ((unsigned char) *demo_p++) << 8;
    cmd->buttons = (unsigned char) *demo_p++;
}

void gWriteDemoTiccmd(ticcmd_t* cmd)
{
    if (gamekeydown['q']) // press q to end demo recording
        gCheckDemoStatus();
    *demo_p++ = cmd->forwardmove;
    *demo_p++ = cmd->sidemove;
    *demo_p++ = (cmd->angleturn + 128) >> 8;
    *demo_p++ = cmd->buttons;
    demo_p -= 4;
    if (demo_p > demoend - 16)
    {
        // no more space
        gCheckDemoStatus();
        return;
    }

    gReadDemoTiccmd(cmd); // make SURE it is exactly the same
}

//
// gRecordDemo
//
void gRecordDemo(char* name)
{
    int i;
    int maxsize;

    usergame = false;
    doom_strcpy(demoname, name);
    doom_concat(demoname, ".lmp");
    maxsize = 0x20000;
    i = M_CheckParm("-maxdemo");
    if (i && i < myargc - 1)
        maxsize = doom_atoi(myargv[i + 1]) * 1024;
    demobuffer = (byte*) (doom_malloc(maxsize));
    demoend = demobuffer + maxsize;

    demorecording = true;
}

void gBeginRecording(void)
{
    int i;

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

    for (i = 0; i < MAXPLAYERS; i++)
        *demo_p++ = playeringame[i];
}

//
// gPlayDemo
//

void gDeferedPlayDemo(const char* name)
{
    defdemoname = name;
    gameaction = ga_playdemo;
}

void gDoPlayDemo(void)
{
    skill_t skill;
    int i, episode, map;

    gameaction = ga_nothing;
    demobuffer = demo_p = (byte*) (W_CacheLumpName(defdemoname, PU_STATIC));
    byte demo_version = *demo_p++;
    if (demo_version != VERSION
        && demo_version != 109) // Demos seem to run fine with version 109
    {
        //doom_print("Demo is from a different game version! Demo Verson = %i, this version = %i\n", (int)demo_version, VERSION);
        doom_print("Demo is from a different game version! Demo Verson = ");
        doom_print(doom_itoa((int) demo_version, 10));
        doom_print(", this version = ");
        doom_print(doom_itoa(VERSION, 10));
        doom_print("\n");
        gameaction = ga_nothing;
        return;
    }

    skill = (skill_t) (*demo_p++);
    episode = *demo_p++;
    map = *demo_p++;
    deathmatch = *demo_p++;
    respawnparm = *demo_p++;
    fastparm = *demo_p++;
    nomonsters = *demo_p++;
    consoleplayer = *demo_p++;

    for (i = 0; i < MAXPLAYERS; i++)
        playeringame[i] = *demo_p++;
    if (playeringame[1])
    {
        netgame = true;
        netdemo = true;
    }

    // don't spend a lot of time in loadlevel
    precache = false;
    gInitNew(skill, episode, map);
    precache = true;

    usergame = false;
    demoplayback = true;
}

//
// gTimeDemo
//
void gTimeDemo(char* name)
{
    nodrawers = M_CheckParm("-nodraw");
    noblit = M_CheckParm("-noblit");
    timingdemo = true;
    singletics = true;

    defdemoname = name;
    gameaction = ga_playdemo;
}

/*
===================
=
= gCheckDemoStatus
=
= Called after a death or level completion to allow demos to be cleaned up
= Returns true if a new demo loop action will take place
===================
*/

doom_boolean gCheckDemoStatus(void)
{
    int endtime;

    if (timingdemo)
    {
        endtime = I_GetTime();
        //I_Error("Error: timed %i gametics in %i realtics", gametic
        //        , endtime - starttime);

        doom_strcpy(error_buf, "Error: timed ");
        doom_concat(error_buf, doom_itoa(gametic, 10));
        doom_concat(error_buf, " gametics in ");
        doom_concat(error_buf, doom_itoa(endtime - starttime, 10));
        doom_concat(error_buf, " realtics");
        I_Error(error_buf);
    }

    if (demoplayback)
    {
        if (singledemo)
            I_Quit();

        demoplayback = false;
        netdemo = false;
        netgame = false;
        deathmatch = false;
        playeringame[1] = playeringame[2] = playeringame[3] = 0;
        respawnparm = false;
        fastparm = false;
        nomonsters = false;
        consoleplayer = 0;
        D_AdvanceDemo();
        return true;
    }

    if (demorecording)
    {
        *demo_p++ = DEMOMARKER;
        M_WriteFile(demoname, demobuffer, (int) (demo_p - demobuffer));
        doom_free(demobuffer);
        demorecording = false;
        //I_Error("Error: Demo %s recorded", demoname);

        doom_strcpy(error_buf, "Error: Demo ");
        doom_concat(error_buf, demoname);
        doom_concat(error_buf, " recorded");
        I_Error(error_buf);
    }

    return false;
}

} // namespace Doom
