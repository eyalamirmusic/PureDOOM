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
// DESCRIPTION:
//        Status bar code.
//        Does the face/direction indicator animatin.
//        Does palette indicators as well (red pain/berserk, bright pickup)
//
//-----------------------------------------------------------------------------

// Rewritten out of vanilla st_stuff into namespace Doom.
//
// The status bar: health/armor/ammo/arms/keys widgets, the animated face, the
// palette flashes, and the cheat-code responder. st_stuff.cpp shims the five ST_
// entry points and owns st_statusbaron (the app reads it to know whether the bar
// is up); everything else — the widget/patch state and the cheat sequences — is
// file-local here. Covered by the frame goldens (the bar lands in screens[0]).

#include "../Host/Platform.h"

#include "AutomapTypes.h"
#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h" // State.
#include "../Game/Strings.h" // Data.
#include "CheatTypes.h"
#include "../Sim/Random.h"
#include "../Sim/SimDefs.h"
#include "../Game/SoundData.h"
#include "StatusWidgetTypes.h"
#include "StatusBarTypes.h"
#include "../Wad/WadFile.h"

#include "../Render/VideoState.h"
#include "StatusBar.h"
#include "StatusBarFace.h"
#include "StatusBarGraphics.h"
#include "StatusBarState.h"
#include "StatusBarWidgets.h"

#include "../Render/Video.h"
#include "Cheat.h"
#include "StatusWidgets.h"
#include <ea_data_structures/Structures/Array.h>

#include "../Game/Game.h"
#include "../Game/GameSession.h"
#include "../Game/GameVersion.h"
#include "../Game/OverlayState.h"
#include "../Game/PlayerState.h"
#include "../Game/Sound.h"
#include "../Host/Video.h"
#include "../Render/Main.h"
#include "../Sim/Interaction.h"
#include "../Sim/Random.h"

// mapnames (hu_stuff) and doom_flags are other subsystems' globals this file reads.
extern char* mapnames[];
extern int doom_flags;

//
// STATUS BAR DATA
//

// Palette indices.
// For damage/bonus red-/gold-shifts
#define STARTREDPALS 1
#define STARTBONUSPALS 9
#define NUMREDPALS 8
#define NUMBONUSPALS 4
// Radiation suit, green shift.
#define RADIATIONPAL 13

// N/256*100% probability
//  that the normal face state will change
#define ST_FACEPROBABILITY 96

// For Responder
#define ST_TOGGLECHAT KEY_ENTER

// Location of status bar
#define ST_X 0
#define ST_X2 104

#define ST_FX 143
#define ST_FY 169

// Number of status faces.
#define ST_NUMPAINFACES 5
#define ST_NUMSTRAIGHTFACES 3
#define ST_NUMTURNFACES 2
#define ST_NUMSPECIALFACES 3

#define ST_FACESTRIDE (ST_NUMSTRAIGHTFACES + ST_NUMTURNFACES + ST_NUMSPECIALFACES)

#define ST_NUMEXTRAFACES 2

#define ST_NUMFACES (ST_FACESTRIDE * ST_NUMPAINFACES + ST_NUMEXTRAFACES)

#define ST_TURNOFFSET (ST_NUMSTRAIGHTFACES)
#define ST_OUCHOFFSET (ST_TURNOFFSET + ST_NUMTURNFACES)
#define ST_EVILGRINOFFSET (ST_OUCHOFFSET + 1)
#define ST_RAMPAGEOFFSET (ST_EVILGRINOFFSET + 1)
#define ST_GODFACE (ST_NUMPAINFACES * ST_FACESTRIDE)
#define ST_DEADFACE (ST_GODFACE + 1)

#define ST_FACESX 143
#define ST_FACESY 168

#define ST_EVILGRINCOUNT (2 * TICRATE)
#define ST_STRAIGHTFACECOUNT (TICRATE / 2)
#define ST_TURNCOUNT (1 * TICRATE)
#define ST_OUCHCOUNT (1 * TICRATE)
#define ST_RAMPAGEDELAY (2 * TICRATE)

#define ST_MUCHPAIN 20

// Location and size of statistics,
// justified according to widget type.
// Problem is, within which space? STbar? Screen?
// Note: this could be read in by a lump.
//       Problem is, is the stuff rendered
//       into a buffer,
//       or into the frame buffer?

// AMMO number pos.
#define ST_AMMOWIDTH 3
#define ST_AMMOX 44
#define ST_AMMOY 171

// HEALTH number pos.
#define ST_HEALTHWIDTH 3
#define ST_HEALTHX 90
#define ST_HEALTHY 171

// Weapon pos.
#define ST_ARMSX 111
#define ST_ARMSY 172
#define ST_ARMSBGX 104
#define ST_ARMSBGY 168
#define ST_ARMSXSPACE 12
#define ST_ARMSYSPACE 10

// Frags pos.
#define ST_FRAGSX 138
#define ST_FRAGSY 171
#define ST_FRAGSWIDTH 2

// ARMOR number pos.
#define ST_ARMORWIDTH 3
#define ST_ARMORX 221
#define ST_ARMORY 171

// Key icon positions.
#define ST_KEY0WIDTH 8
#define ST_KEY0HEIGHT 5
#define ST_KEY0X 239
#define ST_KEY0Y 171
#define ST_KEY1WIDTH ST_KEY0WIDTH
#define ST_KEY1X 239
#define ST_KEY1Y 181
#define ST_KEY2WIDTH ST_KEY0WIDTH
#define ST_KEY2X 239
#define ST_KEY2Y 191

// Ammunition counter.
#define ST_AMMO0WIDTH 3
#define ST_AMMO0HEIGHT 6
#define ST_AMMO0X 288
#define ST_AMMO0Y 173
#define ST_AMMO1WIDTH ST_AMMO0WIDTH
#define ST_AMMO1X 288
#define ST_AMMO1Y 179
#define ST_AMMO2WIDTH ST_AMMO0WIDTH
#define ST_AMMO2X 288
#define ST_AMMO2Y 191
#define ST_AMMO3WIDTH ST_AMMO0WIDTH
#define ST_AMMO3X 288
#define ST_AMMO3Y 185

// Indicate maximum ammunition.
// Only needed because backpack exists.
#define ST_MAXAMMO0WIDTH 3
#define ST_MAXAMMO0HEIGHT 5
#define ST_MAXAMMO0X 314
#define ST_MAXAMMO0Y 173
#define ST_MAXAMMO1WIDTH ST_MAXAMMO0WIDTH
#define ST_MAXAMMO1X 314
#define ST_MAXAMMO1Y 179
#define ST_MAXAMMO2WIDTH ST_MAXAMMO0WIDTH
#define ST_MAXAMMO2X 314
#define ST_MAXAMMO2Y 191
#define ST_MAXAMMO3WIDTH ST_MAXAMMO0WIDTH
#define ST_MAXAMMO3X 314
#define ST_MAXAMMO3Y 185

// pistol
#define ST_WEAPON0X 110
#define ST_WEAPON0Y 172

// shotgun
#define ST_WEAPON1X 122
#define ST_WEAPON1Y 172

// chain gun
#define ST_WEAPON2X 134
#define ST_WEAPON2Y 172

// missile launcher
#define ST_WEAPON3X 110
#define ST_WEAPON3Y 181

// plasma gun
#define ST_WEAPON4X 122
#define ST_WEAPON4Y 181

// bfg
#define ST_WEAPON5X 134
#define ST_WEAPON5Y 181

// WPNS title
#define ST_WPNSX 109
#define ST_WPNSY 191

// DETH title
#define ST_DETHX 109
#define ST_DETHY 191

//Incoming messages window location
#define ST_MSGTEXTX 0
#define ST_MSGTEXTY 0
// Dimensions given in characters.
#define ST_MSGWIDTH 52
// Or shall I say, in lines?
#define ST_MSGHEIGHT 1

#define ST_OUTTEXTX 0
#define ST_OUTTEXTY 6

// Width, in characters again.
#define ST_OUTWIDTH 52
// Height, in lines.
#define ST_OUTHEIGHT 1

#define ST_MAPWIDTH                                                                 \
    (doom_strlen(mapnames[(gameSession().gameepisode - 1) * 9                       \
                          + (gameSession().gamemap - 1)]))

#define ST_MAPTITLEX (SCREENWIDTH - ST_MAPWIDTH * ST_CHATFONTWIDTH)

#define ST_MAPTITLEY 0
#define ST_MAPHEIGHT 1

namespace Doom
{

// The status bar's residual runtime state is a Doom::StatusBarState owned by the Engine
// (StatusBarState.h); the loaded patches are a Doom::StatusBarGraphics (StatusBarGraphics.h,
// faces[ST_NUMFACES] sized off the same ST_NUMFACES macro); the STlib widgets are a
// Doom::StatusBarWidgets (StatusBarWidgets.h); and the animated face's selection state is a
// Doom::StatusBarFace (StatusBarFace.h). All four used to be reached through file-scope
// `static T& x = cluster().x;` reference aliases (moved in by the file-scope-statics sweep,
// REFACTOR.md Step 5); the file-local-alias sweep (REFACTOR.md, Step 9 strand (a)) retired them -
// every function below reaches its cluster(s) through a hoisted local instead, taken once per
// function and reused for however many of that cluster's members the function touches.

// Massive bunches of cheat shit
//  to keep it from being easy to figure them out.
// Yeah, right...
EA::Array<unsigned char, 9> cheat_mus_seq = {
    0xb2, 0x26, 0xb6, 0xae, 0xea, 1, 0, 0, 0xff};

EA::Array<unsigned char, 11> cheat_choppers_seq = {
    0xb2, 0x26, 0xe2, 0x32, 0xf6, 0x2a, 0x2a, 0xa6, 0x6a, 0xea, 0xff // id...
};

EA::Array<unsigned char, 6> cheat_god_seq = {
    0xb2, 0x26, 0x26, 0xaa, 0x26, 0xff // iddqd
};

EA::Array<unsigned char, 6> cheat_ammo_seq = {
    0xb2, 0x26, 0xf2, 0x66, 0xa2, 0xff // idkfa
};

EA::Array<unsigned char, 5> cheat_ammonokey_seq = {
    0xb2, 0x26, 0x66, 0xa2, 0xff // idfa
};

// Smashing Pumpkins Into Samml Piles Of Putried Debris.
EA::Array<unsigned char, 11> cheat_noclip_seq = {0xb2,
                                                 0x26,
                                                 0xea,
                                                 0x2a,
                                                 0xb2, // idspispopd
                                                 0xea,
                                                 0x2a,
                                                 0xf6,
                                                 0x2a,
                                                 0x26,
                                                 0xff};

//
EA::Array<unsigned char, 7> cheat_commercial_noclip_seq = {
    0xb2, 0x26, 0xe2, 0x36, 0xb2, 0x2a, 0xff // idclip
};

EA::Array<EA::Array<unsigned char, 10>, 7> cheat_powerup_seq = {
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x6e, 0xff}, // beholdv
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xea, 0xff}, // beholds
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xb2, 0xff}, // beholdi
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x6a, 0xff}, // beholdr
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xa2, 0xff}, // beholda
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x36, 0xff}, // beholdl
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xff} // behold
};

EA::Array<unsigned char, 10> cheat_clev_seq = {
    0xb2, 0x26, 0xe2, 0x36, 0xa6, 0x6e, 1, 0, 0, 0xff // idclev
};

// my position cheat
EA::Array<unsigned char, 8> cheat_mypos_seq = {
    0xb2, 0x26, 0xb6, 0xba, 0x2a, 0xf6, 0xea, 0xff // idmypos
};

// Now what?
CheatSequence cheat_mus = {cheat_mus_seq.data(), 0};
CheatSequence cheat_god = {cheat_god_seq.data(), 0};
CheatSequence cheat_ammo = {cheat_ammo_seq.data(), 0};
CheatSequence cheat_ammonokey = {cheat_ammonokey_seq.data(), 0};
CheatSequence cheat_noclip = {cheat_noclip_seq.data(), 0};
CheatSequence cheat_commercial_noclip = {cheat_commercial_noclip_seq.data(), 0};

EA::Array<CheatSequence, 7> cheat_powerup = {{cheat_powerup_seq[0].data(), 0},
                                             {cheat_powerup_seq[1].data(), 0},
                                             {cheat_powerup_seq[2].data(), 0},
                                             {cheat_powerup_seq[3].data(), 0},
                                             {cheat_powerup_seq[4].data(), 0},
                                             {cheat_powerup_seq[5].data(), 0},
                                             {cheat_powerup_seq[6].data(), 0}};

CheatSequence cheat_choppers = {cheat_choppers_seq.data(), 0};
CheatSequence cheat_clev = {cheat_clev_seq.data(), 0};
CheatSequence cheat_mypos = {cheat_mypos_seq.data(), 0};

void stopStatusBar();

//
// STATUS BAR CODE
//

void refreshBackground()
{
    if (statusBarState().st_statusbaron)
    {
        auto& gfx = statusBarGraphics();

        Doom::drawPatch(ST_X, 0, STLIB_BG, gfx.sbar);

        if (gameSession().netgame)
            Doom::drawPatch(ST_FX, 0, STLIB_BG, gfx.faceback);

        Doom::copyRect(ST_X, 0, STLIB_BG, ST_WIDTH, ST_HEIGHT, ST_X, ST_Y, STLIB_FG);
    }
}

// Respond to keyboard input events,
//  intercept cheats.
bool statusBarResponder(Event* ev)
{
    auto& bar = statusBarState();

    // Filter automap on/off.
    if (ev->type == ev_keyup && ((ev->data1 & 0xffff0000) == AM_MSGHEADER))
    {
        switch (ev->data1)
        {
            case AM_MSGENTERED:
                bar.st_firsttime = true;
                break;

            case AM_MSGEXITED:
                //        doom_print("AM exited\n");
                break;
        }
    }

    // if a user keypress...
    else if (ev->type == ev_keydown)
    {
        if (!gameSession().netgame)
        {
            // b. - enabled for more debug fun.
            // if (gameskill != sk_nightmare) {

            // 'dqd' cheat for toggleable god mode
            if (Doom::checkCheat(&cheat_god, ev->data1))
            {
                bar.plyr->cheats ^= CF_GODMODE;
                if (bar.plyr->cheats & CF_GODMODE)
                {
                    if (bar.plyr->mo)
                        bar.plyr->mo->health = 100;

                    bar.plyr->health = 100;
                    bar.plyr->message = STSTR_DQDON;
                }
                else
                    bar.plyr->message = STSTR_DQDOFF;
            }
            // 'fa' cheat for killer fucking arsenal
            else if (Doom::checkCheat(&cheat_ammonokey, ev->data1))
            {
                bar.plyr->armorpoints = 200;
                bar.plyr->armortype = 2;

                for (int i = 0; i < NUMWEAPONS; i++)
                    bar.plyr->weaponowned[i] = true;

                for (int i = 0; i < NUMAMMO; i++)
                    bar.plyr->ammo[i] = bar.plyr->maxammo[i];

                bar.plyr->message = STSTR_FAADDED;
            }
            // 'kfa' cheat for key full ammo
            else if (Doom::checkCheat(&cheat_ammo, ev->data1))
            {
                bar.plyr->armorpoints = 200;
                bar.plyr->armortype = 2;

                for (int i = 0; i < NUMWEAPONS; i++)
                    bar.plyr->weaponowned[i] = true;

                for (int i = 0; i < NUMAMMO; i++)
                    bar.plyr->ammo[i] = bar.plyr->maxammo[i];

                for (int i = 0; i < NUMCARDS; i++)
                    bar.plyr->cards[i] = true;

                bar.plyr->message = STSTR_KFAADDED;
            }
            // 'mus' cheat for changing music
            else if (Doom::checkCheat(&cheat_mus, ev->data1))
            {
                EA::Array<char, 3> buf;
                int musnum;

                bar.plyr->message = STSTR_MUS;
                Doom::getParam(&cheat_mus, buf.data());

                if (gameVersion().gamemode == commercial)
                {
                    musnum = mus_runnin + (buf[0] - '0') * 10 + buf[1] - '0' - 1;

                    if (((buf[0] - '0') * 10 + buf[1] - '0') > 35)
                        bar.plyr->message = STSTR_NOMUS;
                    else
                        Doom::changeMusic(musnum, 1);
                }
                else
                {
                    musnum = mus_e1m1 + (buf[0] - '1') * 9 + (buf[1] - '1');

                    if (((buf[0] - '1') * 9 + buf[1] - '1') > 31)
                        bar.plyr->message = STSTR_NOMUS;
                    else
                        Doom::changeMusic(musnum, 1);
                }
            }
            // Simplified, accepting both "noclip" and "idspispopd".
            // no clipping mode cheat
            else if (Doom::checkCheat(&cheat_noclip, ev->data1)
                     || Doom::checkCheat(&cheat_commercial_noclip, ev->data1))
            {
                bar.plyr->cheats ^= CF_NOCLIP;

                if (bar.plyr->cheats & CF_NOCLIP)
                    bar.plyr->message = STSTR_NCON;
                else
                    bar.plyr->message = STSTR_NCOFF;
            }
            // 'behold?' power-up cheats
            for (int i = 0; i < 6; i++)
            {
                if (Doom::checkCheat(&cheat_powerup[i], ev->data1))
                {
                    if (!bar.plyr->powers[i])
                        Doom::givePower(bar.plyr, i);
                    else if (i != pw_strength)
                        bar.plyr->powers[i] = 1;
                    else
                        bar.plyr->powers[i] = 0;

                    bar.plyr->message = STSTR_BEHOLDX;
                }
            }

            // 'behold' power-up menu
            if (Doom::checkCheat(&cheat_powerup[6], ev->data1))
            {
                bar.plyr->message = STSTR_BEHOLD;
            }
            // 'choppers' invulnerability & chainsaw
            else if (Doom::checkCheat(&cheat_choppers, ev->data1))
            {
                bar.plyr->weaponowned[wp_chainsaw] = true;
                bar.plyr->powers[pw_invulnerability] = true;
                bar.plyr->message = STSTR_CHOPPERS;
            }
            // 'mypos' for player position
            else if (Doom::checkCheat(&cheat_mypos, ev->data1))
            {
                static EA::Array<char, ST_MSGWIDTH> buf;
                //doom_sprintf(buf, "ang=0x%x;x,y=(0x%x,0x%x)",
                //        players[consoleplayer].mo->angle,
                //        players[consoleplayer].mo->x,
                //        players[consoleplayer].mo->y);
                auto& players_ = playerState();
                const auto* mo = players_.players[players_.consoleplayer].mo;

                doom_strcpy(buf.data(), "ang=0x");
                doom_concat(buf.data(), doom_itoa((int) mo->angle.raw, 16));
                doom_concat(buf.data(), ";x,y=(0x");
                doom_concat(buf.data(), doom_itoa(mo->x.raw, 16));
                doom_concat(buf.data(), ",0x");
                doom_concat(buf.data(), doom_itoa(mo->y.raw, 16));
                doom_concat(buf.data(), ")");
                bar.plyr->message = buf.data();
            }
        }

        // 'clev' change-level cheat
        if (Doom::checkCheat(&cheat_clev, ev->data1))
        {
            EA::Array<char, 3> buf;
            int epsd;
            int map;

            Doom::getParam(&cheat_clev, buf.data());

            const auto& version = gameVersion();

            if (version.gamemode == commercial)
            {
                epsd = 0;
                map = (buf[0] - '0') * 10 + buf[1] - '0';
            }
            else
            {
                epsd = buf[0] - '0';
                map = buf[1] - '0';
            }

            // Catch invalid maps.
            if (epsd < 1)
                return false;

            if (map < 1)
                return false;

            // Ohmygod - this is not going to work.
            if ((version.gamemode == retail) && ((epsd > 4) || (map > 9)))
                return false;

            if ((version.gamemode == registered) && ((epsd > 3) || (map > 9)))
                return false;

            if ((version.gamemode == shareware) && ((epsd > 1) || (map > 9)))
                return false;

            if ((version.gamemode == commercial) && ((epsd > 1) || (map > 34)))
                return false;

            // So be it.
            bar.plyr->message = STSTR_CLEV;
            Doom::deferInitNew(gameSession().gameskill, epsd, map);
        }
    }
    return false;
}

int calcPainOffset()
{
    int health;
    // The pain-offset cache: Doom::StatusBarFace members (Engine) now, reached by a local
    // reference (formerly function-local statics, never reset - identical persistence).
    int& lastcalc = statusBarFace().lastcalc;
    int& oldhealth = statusBarFace().oldhealth;
    auto& bar = statusBarState();

    health = bar.plyr->health > 100 ? 100 : bar.plyr->health;

    if (health != oldhealth)
    {
        lastcalc = ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
        oldhealth = health;
    }
    return lastcalc;
}

//
// This is a not-very-pretty routine which handles
//  the face states and their timing.
// the precedence of expressions is:
//  dead > evil grin > turned head > straight ahead
//
void updateFaceWidget()
{
    int i;
    angle_t badguyangle;
    angle_t diffang;
    // The face state machine's carry: Doom::StatusBarFace members (Engine) now, reached by a
    // hoisted local (formerly function-local statics, never reset - identical persistence).
    auto& face = statusBarFace();
    auto& bar = statusBarState();
    bool doevilgrin;

    if (face.priority < 10)
    {
        // dead
        if (!bar.plyr->health)
        {
            face.priority = 9;
            face.st_faceindex = ST_DEADFACE;
            face.st_facecount = 1;
        }
    }

    if (face.priority < 9)
    {
        if (bar.plyr->bonuscount)
        {
            // picking up bonus
            doevilgrin = false;

            for (i = 0; i < NUMWEAPONS; i++)
            {
                if (face.oldweaponsowned[i] != bar.plyr->weaponowned[i])
                {
                    doevilgrin = true;
                    face.oldweaponsowned[i] = bar.plyr->weaponowned[i];
                }
            }
            if (doevilgrin)
            {
                // evil grin if just picked up weapon
                face.priority = 8;
                face.st_facecount = ST_EVILGRINCOUNT;
                face.st_faceindex = calcPainOffset() + ST_EVILGRINOFFSET;
            }
        }
    }

    if (face.priority < 8)
    {
        if (bar.plyr->damagecount && bar.plyr->attacker
            && bar.plyr->attacker != bar.plyr->mo)
        {
            // being attacked
            face.priority = 7;

            if (bar.plyr->health - face.st_oldhealth > ST_MUCHPAIN)
            {
                face.st_facecount = ST_TURNCOUNT;
                face.st_faceindex = calcPainOffset() + ST_OUCHOFFSET;
            }
            else
            {
                badguyangle = Doom::pointToAngle2(bar.plyr->mo->x,
                                                  bar.plyr->mo->y,
                                                  bar.plyr->attacker->x,
                                                  bar.plyr->attacker->y);

                if (badguyangle > bar.plyr->mo->angle)
                {
                    // whether right or left
                    diffang = badguyangle - bar.plyr->mo->angle;
                    i = diffang > ANG180;
                }
                else
                {
                    // whether left or right
                    diffang = bar.plyr->mo->angle - badguyangle;
                    i = diffang <= ANG180;
                } // confusing, aint it?

                face.st_facecount = ST_TURNCOUNT;
                face.st_faceindex = calcPainOffset();

                if (diffang < ANG45)
                {
                    // head-on
                    face.st_faceindex += ST_RAMPAGEOFFSET;
                }
                else if (i)
                {
                    // turn face right
                    face.st_faceindex += ST_TURNOFFSET;
                }
                else
                {
                    // turn face left
                    face.st_faceindex += ST_TURNOFFSET + 1;
                }
            }
        }
    }

    if (face.priority < 7)
    {
        // getting hurt because of your own damn stupidity
        if (bar.plyr->damagecount)
        {
            if (bar.plyr->health - face.st_oldhealth > ST_MUCHPAIN)
            {
                face.priority = 7;
                face.st_facecount = ST_TURNCOUNT;
                face.st_faceindex = calcPainOffset() + ST_OUCHOFFSET;
            }
            else
            {
                face.priority = 6;
                face.st_facecount = ST_TURNCOUNT;
                face.st_faceindex = calcPainOffset() + ST_RAMPAGEOFFSET;
            }
        }
    }

    if (face.priority < 6)
    {
        // rapid firing
        if (bar.plyr->attackdown)
        {
            if (face.lastattackdown == -1)
                face.lastattackdown = ST_RAMPAGEDELAY;
            else if (!--face.lastattackdown)
            {
                face.priority = 5;
                face.st_faceindex = calcPainOffset() + ST_RAMPAGEOFFSET;
                face.st_facecount = 1;
                face.lastattackdown = 1;
            }
        }
        else
            face.lastattackdown = -1;
    }

    if (face.priority < 5)
    {
        // invulnerability
        if ((bar.plyr->cheats & CF_GODMODE) || bar.plyr->powers[pw_invulnerability])
        {
            face.priority = 4;

            face.st_faceindex = ST_GODFACE;
            face.st_facecount = 1;
        }
    }

    // look left or look right if the facecount has timed out
    if (!face.st_facecount)
    {
        face.st_faceindex = calcPainOffset() + (face.st_randomnumber % 3);
        face.st_facecount = ST_STRAIGHTFACECOUNT;
        face.priority = 0;
    }

    face.st_facecount--;
}

void updateWidgets()
{
    auto& bar = statusBarState();
    // The "n/a" ammo sentinel: a Doom::StatusBarWidgets member (Engine) now, reached through the
    // hoisted cluster (w_ready.num takes its address, so the member's stable address is what it
    // needs).
    auto& widgets = statusBarWidgets();

    if (weaponinfo[bar.plyr->readyweapon].ammo == am_noammo)
        widgets.w_ready.num = &widgets.largeammo;
    else
        widgets.w_ready.num =
            &bar.plyr->ammo[weaponinfo[bar.plyr->readyweapon].ammo];

    widgets.w_ready.data = bar.plyr->readyweapon;

    // update keycard multiple widgets
    for (int i = 0; i < 3; i++)
    {
        bar.keyboxes[i] = bar.plyr->cards[i] ? i : -1;

        if (bar.plyr->cards[i + 3])
            bar.keyboxes[i] = i + 3;
    }

    // refresh everything if this is him coming back to life
    updateFaceWidget();

    const auto& session = gameSession();

    // used by the w_armsbg widget
    bar.st_notdeathmatch = !session.deathmatch;

    // used by w_arms[] widgets
    bar.st_armson = bar.st_statusbaron && !session.deathmatch;

    // used by w_frags widget
    bar.st_fragson = session.deathmatch && bar.st_statusbaron;
    bar.st_fragscount = 0;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (i != playerState().consoleplayer)
            bar.st_fragscount += bar.plyr->frags[i];
        else
            bar.st_fragscount -= bar.plyr->frags[i];
    }
}

void statusBarTicker()
{
    auto& bar = statusBarState();
    auto& face = statusBarFace();

    face.st_randomnumber = Doom::randomness().forMenu();
    updateWidgets();
    face.st_oldhealth = bar.plyr->health;
}

void doPaletteStuff()
{
    auto& bar = statusBarState();

    int palette;
    byte* pal;
    int cnt;
    int bzc;

    cnt = bar.plyr->damagecount;

    if (bar.plyr->powers[pw_strength])
    {
        // slowly fade the berzerk out
        bzc = 12 - (bar.plyr->powers[pw_strength] >> 6);

        if (bzc > cnt)
            cnt = bzc;
    }

    if (cnt)
    {
        palette = (cnt + 7) >> 3;

        if (palette >= NUMREDPALS)
            palette = NUMREDPALS - 1;

        palette += STARTREDPALS;
    }

    else if (bar.plyr->bonuscount)
    {
        palette = (bar.plyr->bonuscount + 7) >> 3;

        if (palette >= NUMBONUSPALS)
            palette = NUMBONUSPALS - 1;

        palette += STARTBONUSPALS;
    }

    else if (bar.plyr->powers[pw_ironfeet] > 4 * 32
             || bar.plyr->powers[pw_ironfeet] & 8)
        palette = RADIATIONPAL;
    else
        palette = 0;

    if (palette != bar.st_palette)
    {
        bar.st_palette = palette;
        pal = static_cast<byte*>(Doom::cacheLumpNum(bar.lu_palette)) + palette * 768;
        setPalette(pal);
    }
}

void drawWidgets(bool refresh)
{
    auto& bar = statusBarState();
    auto& widgets = statusBarWidgets();
    const auto& session = gameSession();

    // used by w_arms[] widgets
    bar.st_armson = bar.st_statusbaron && !session.deathmatch;

    // used by w_frags widget
    bar.st_fragson = session.deathmatch && bar.st_statusbaron;

    Doom::updateNum(widgets.w_ready, refresh);

    for (int i = 0; i < 4; i++)
    {
        Doom::updateNum(widgets.w_ammo[i], refresh);
        Doom::updateNum(widgets.w_maxammo[i], refresh);
    }

    Doom::updatePercent(widgets.w_health, refresh);
    Doom::updatePercent(widgets.w_armor, refresh);

    Doom::updateBinIcon(widgets.w_armsbg, refresh);

    // The arms icons read w_armsindex, not the player directly - see StatusBarWidgets.h.
    // Refreshed here, immediately before the update, so each icon sees exactly the
    // weaponowned value the old (int*) pun would have read at this same point.
    for (int i = 0; i < 6; i++)
        widgets.w_armsindex[i] = bar.plyr->weaponowned[i + 1];

    for (int i = 0; i < 6; i++)
        Doom::updateMultIcon(widgets.w_arms[i], refresh);

    Doom::updateMultIcon(widgets.w_faces, refresh);

    for (int i = 0; i < 3; i++)
        Doom::updateMultIcon(widgets.w_keyboxes[i], refresh);

    Doom::updateNum(widgets.w_frags, refresh);
}

void doRefresh()
{
    statusBarState().st_firsttime = false;

    // draw status bar background to off-screen buff
    refreshBackground();

    // and refresh all widgets
    drawWidgets(true);
}

void diffDraw()
{
    // update all widgets
    drawWidgets(false);
}

void drawStatusBar(bool fullscreen, bool refresh)
{
    auto& bar = statusBarState();

    bar.st_statusbaron = (!fullscreen) || overlayState().automapactive;
    bar.st_firsttime = bar.st_firsttime || refresh;

    // Do red-/gold-shifts from damage/items
    doPaletteStuff();

    // If just after startStatusBar(), refresh all
    if (doom_flags & DOOM_FLAG_MENU_DARKEN_BG)
    {
        doRefresh();
    }
    else
    {
        if (bar.st_firsttime)
            doRefresh();
        // Otherwise, update as little as possible
        else
            diffDraw();
    }
}

void loadGraphics()
{
    int facenum;
    auto& gfx = statusBarGraphics();

    EA::Array<char, 9> namebuf;

    // Load the numbers, tall and short
    for (int i = 0; i < 10; i++)
    {
        //doom_sprintf(namebuf, "STTNUM%d", i);
        doom_strcpy(namebuf.data(), "STTNUM");
        doom_concat(namebuf.data(), doom_itoa(i, 10));
        gfx.tallnum[i] = static_cast<Patch*>(Doom::cacheLumpName(namebuf.data()));

        //doom_sprintf(namebuf, "STYSNUM%d", i);
        doom_strcpy(namebuf.data(), "STYSNUM");
        doom_concat(namebuf.data(), doom_itoa(i, 10));
        gfx.shortnum[i] = static_cast<Patch*>(Doom::cacheLumpName(namebuf.data()));
    }

    // Load percent key.
    //Note: why not load STMINUS here, too?
    gfx.tallpercent = static_cast<Patch*>(Doom::cacheLumpName("STTPRCNT"));

    // key cards
    for (int i = 0; i < NUMCARDS; i++)
    {
        //doom_sprintf(namebuf, "STKEYS%d", i);
        doom_strcpy(namebuf.data(), "STKEYS");
        doom_concat(namebuf.data(), doom_itoa(i, 10));
        gfx.keys[i] = static_cast<Patch*>(Doom::cacheLumpName(namebuf.data()));
    }

    // arms background
    gfx.armsbg = static_cast<Patch*>(Doom::cacheLumpName("STARMS"));

    // arms ownership widgets
    for (int i = 0; i < 6; i++)
    {
        //doom_sprintf(namebuf, "STGNUM%d", i + 2);
        doom_strcpy(namebuf.data(), "STGNUM");
        doom_concat(namebuf.data(), doom_itoa(i + 2, 10));

        // gray #
        gfx.arms[i][0] = static_cast<Patch*>(Doom::cacheLumpName(namebuf.data()));

        // yellow #
        gfx.arms[i][1] = gfx.shortnum[i + 2];
    }

    // face backgrounds for different color players
    //doom_sprintf(namebuf, "STFB%d", consoleplayer);
    doom_strcpy(namebuf.data(), "STFB");
    doom_concat(namebuf.data(), doom_itoa(playerState().consoleplayer, 10));
    gfx.faceback = static_cast<Patch*>(Doom::cacheLumpName(namebuf.data()));

    // status bar background bits
    gfx.sbar = static_cast<Patch*>(Doom::cacheLumpName("STBAR"));

    // face states
    facenum = 0;
    for (int i = 0; i < ST_NUMPAINFACES; i++)
    {
        for (int j = 0; j < ST_NUMSTRAIGHTFACES; j++)
        {
            //doom_sprintf(namebuf, "STFST%d%d", i, j);
            doom_strcpy(namebuf.data(), "STFST");
            doom_concat(namebuf.data(), doom_itoa(i, 10));
            doom_concat(namebuf.data(), doom_itoa(j, 10));
            gfx.faces[facenum++] =
                static_cast<Patch*>(Doom::cacheLumpName(namebuf.data()));
        }
        //doom_sprintf(namebuf, "STFTR%d0", i);        // turn right
        doom_strcpy(namebuf.data(), "STFTR");
        doom_concat(namebuf.data(), doom_itoa(i, 10));
        doom_concat(namebuf.data(), "0");
        gfx.faces[facenum++] =
            static_cast<Patch*>(Doom::cacheLumpName(namebuf.data()));
        //doom_sprintf(namebuf, "STFTL%d0", i);        // turn left
        doom_strcpy(namebuf.data(), "STFTL");
        doom_concat(namebuf.data(), doom_itoa(i, 10));
        doom_concat(namebuf.data(), "0");
        gfx.faces[facenum++] =
            static_cast<Patch*>(Doom::cacheLumpName(namebuf.data()));
        //doom_sprintf(namebuf, "STFOUCH%d", i);        // ouch!
        doom_strcpy(namebuf.data(), "STFOUCH");
        doom_concat(namebuf.data(), doom_itoa(i, 10));
        gfx.faces[facenum++] =
            static_cast<Patch*>(Doom::cacheLumpName(namebuf.data()));
        //doom_sprintf(namebuf, "STFEVL%d", i);        // evil grin ;)
        doom_strcpy(namebuf.data(), "STFEVL");
        doom_concat(namebuf.data(), doom_itoa(i, 10));
        gfx.faces[facenum++] =
            static_cast<Patch*>(Doom::cacheLumpName(namebuf.data()));
        //doom_sprintf(namebuf, "STFKILL%d", i);        // pissed off
        doom_strcpy(namebuf.data(), "STFKILL");
        doom_concat(namebuf.data(), doom_itoa(i, 10));
        gfx.faces[facenum++] =
            static_cast<Patch*>(Doom::cacheLumpName(namebuf.data()));
    }
    gfx.faces[facenum++] = static_cast<Patch*>(Doom::cacheLumpName("STFGOD0"));
    gfx.faces[facenum++] = static_cast<Patch*>(Doom::cacheLumpName("STFDEAD0"));
}

void loadData()
{
    statusBarState().lu_palette = Doom::wad().number("PLAYPAL");
    loadGraphics();
}

void unloadGraphics()
{
    // Nothing to unload any more: Doom::WadFile owns the lumps and they are
    // permanent (Wad/WadFile.h). This used to hand each patch back to the zone
    // as PU_CACHE, meaning "purge me if you need the space".
}

void unloadData()
{
    unloadGraphics();
}

void initStatusBarData()
{
    auto& players_ = playerState();
    auto& bar = statusBarState();
    auto& face = statusBarFace();

    bar.st_firsttime = true;
    bar.plyr = &players_.players[players_.consoleplayer];

    bar.st_statusbaron = true;

    face.st_faceindex = 0;
    bar.st_palette = -1;

    face.st_oldhealth = -1;

    for (int i = 0; i < NUMWEAPONS; i++)
        face.oldweaponsowned[i] = bar.plyr->weaponowned[i];

    for (int i = 0; i < 3; i++)
        bar.keyboxes[i] = -1;

    Doom::initStatusWidgets();
}

void createWidgets()
{
    auto& bar = statusBarState();
    auto& widgets = statusBarWidgets();
    auto& gfx = statusBarGraphics();

    // ready weapon ammo
    Doom::initNum(widgets.w_ready,
                  ST_AMMOX,
                  ST_AMMOY,
                  gfx.tallnum,
                  &bar.plyr->ammo[weaponinfo[bar.plyr->readyweapon].ammo],
                  &bar.st_statusbaron,
                  ST_AMMOWIDTH);

    // the last weapon type
    widgets.w_ready.data = bar.plyr->readyweapon;

    // health percentage
    Doom::initPercent(widgets.w_health,
                      ST_HEALTHX,
                      ST_HEALTHY,
                      gfx.tallnum,
                      &bar.plyr->health,
                      &bar.st_statusbaron,
                      gfx.tallpercent);

    // arms background
    Doom::initBinIcon(widgets.w_armsbg,
                      ST_ARMSBGX,
                      ST_ARMSBGY,
                      gfx.armsbg,
                      &bar.st_notdeathmatch,
                      &bar.st_statusbaron);

    // weapons owned
    for (int i = 0; i < 6; i++)
    {
        Doom::initMultIcon(widgets.w_arms[i],
                           ST_ARMSX + (i % 3) * ST_ARMSXSPACE,
                           ST_ARMSY + (i / 3) * ST_ARMSYSPACE,
                           gfx.arms[i],
                           &widgets.w_armsindex[i],
                           &bar.st_armson);
    }

    // frags sum
    Doom::initNum(widgets.w_frags,
                  ST_FRAGSX,
                  ST_FRAGSY,
                  gfx.tallnum,
                  &bar.st_fragscount,
                  &bar.st_fragson,
                  ST_FRAGSWIDTH);

    // faces
    Doom::initMultIcon(widgets.w_faces,
                       ST_FACESX,
                       ST_FACESY,
                       gfx.faces,
                       &statusBarFace().st_faceindex,
                       &bar.st_statusbaron);

    // armor percentage - should be colored later
    Doom::initPercent(widgets.w_armor,
                      ST_ARMORX,
                      ST_ARMORY,
                      gfx.tallnum,
                      &bar.plyr->armorpoints,
                      &bar.st_statusbaron,
                      gfx.tallpercent);

    // keyboxes 0-2
    Doom::initMultIcon(widgets.w_keyboxes[0],
                       ST_KEY0X,
                       ST_KEY0Y,
                       gfx.keys,
                       &bar.keyboxes[0],
                       &bar.st_statusbaron);

    Doom::initMultIcon(widgets.w_keyboxes[1],
                       ST_KEY1X,
                       ST_KEY1Y,
                       gfx.keys,
                       &bar.keyboxes[1],
                       &bar.st_statusbaron);

    Doom::initMultIcon(widgets.w_keyboxes[2],
                       ST_KEY2X,
                       ST_KEY2Y,
                       gfx.keys,
                       &bar.keyboxes[2],
                       &bar.st_statusbaron);

    // ammo count (all four kinds)
    Doom::initNum(widgets.w_ammo[0],
                  ST_AMMO0X,
                  ST_AMMO0Y,
                  gfx.shortnum,
                  &bar.plyr->ammo[0],
                  &bar.st_statusbaron,
                  ST_AMMO0WIDTH);

    Doom::initNum(widgets.w_ammo[1],
                  ST_AMMO1X,
                  ST_AMMO1Y,
                  gfx.shortnum,
                  &bar.plyr->ammo[1],
                  &bar.st_statusbaron,
                  ST_AMMO1WIDTH);

    Doom::initNum(widgets.w_ammo[2],
                  ST_AMMO2X,
                  ST_AMMO2Y,
                  gfx.shortnum,
                  &bar.plyr->ammo[2],
                  &bar.st_statusbaron,
                  ST_AMMO2WIDTH);

    Doom::initNum(widgets.w_ammo[3],
                  ST_AMMO3X,
                  ST_AMMO3Y,
                  gfx.shortnum,
                  &bar.plyr->ammo[3],
                  &bar.st_statusbaron,
                  ST_AMMO3WIDTH);

    // max ammo count (all four kinds)
    Doom::initNum(widgets.w_maxammo[0],
                  ST_MAXAMMO0X,
                  ST_MAXAMMO0Y,
                  gfx.shortnum,
                  &bar.plyr->maxammo[0],
                  &bar.st_statusbaron,
                  ST_MAXAMMO0WIDTH);

    Doom::initNum(widgets.w_maxammo[1],
                  ST_MAXAMMO1X,
                  ST_MAXAMMO1Y,
                  gfx.shortnum,
                  &bar.plyr->maxammo[1],
                  &bar.st_statusbaron,
                  ST_MAXAMMO1WIDTH);

    Doom::initNum(widgets.w_maxammo[2],
                  ST_MAXAMMO2X,
                  ST_MAXAMMO2Y,
                  gfx.shortnum,
                  &bar.plyr->maxammo[2],
                  &bar.st_statusbaron,
                  ST_MAXAMMO2WIDTH);

    Doom::initNum(widgets.w_maxammo[3],
                  ST_MAXAMMO3X,
                  ST_MAXAMMO3Y,
                  gfx.shortnum,
                  &bar.plyr->maxammo[3],
                  &bar.st_statusbaron,
                  ST_MAXAMMO3WIDTH);
}

void startStatusBar()
{
    auto& bar = statusBarState();

    if (!bar.st_stopped)
        stopStatusBar();

    initStatusBarData();
    createWidgets();
    bar.st_stopped = false;
}

void stopStatusBar()
{
    auto& bar = statusBarState();

    if (bar.st_stopped)
        return;

    setPalette(static_cast<byte*>(Doom::cacheLumpNum(bar.lu_palette)));

    bar.st_stopped = true;
}

void initStatusBar()
{
    loadData();
    // RAII now (Step 9): the status bar back-buffer is a VideoState-owned vector;
    // screens[4] is the raw view onto its data(). initStatusBar runs once at boot.
    auto& statusBar = videoState().statusBar;
    statusBar.resize(ST_WIDTH * ST_HEIGHT);
    screens[4] = statusBar.data();
}

} // namespace Doom
