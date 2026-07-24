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
#include "../Containers.h"

#include "../Game/Game.h"
#include "../Game/GameSession.h"
#include "../Game/GameVersion.h"
#include "../Game/OverlayState.h"
#include "../Game/PlayerState.h"
#include "../Game/Sound.h"
#include "../Host/Text.h"
#include "../Host/Video.h"
#include "../Render/Main.h"
#include "../Sim/Interaction.h"
#include "../Sim/Random.h"

#include <algorithm>

//
// STATUS BAR DATA
//

namespace Doom
{

// Palette indices.
// For damage/bonus red-/gold-shifts
constexpr int STARTREDPALS = 1;
constexpr int STARTBONUSPALS = 9;
constexpr int NUMREDPALS = 8;
constexpr int NUMBONUSPALS = 4;
// Radiation suit, green shift.
constexpr int RADIATIONPAL = 13;

// Location of status bar
constexpr int ST_X = 0;

constexpr int ST_FX = 143;

// Number of status faces.
constexpr int ST_NUMPAINFACES = 5;
constexpr int ST_NUMSTRAIGHTFACES = 3;
constexpr int ST_NUMTURNFACES = 2;
constexpr int ST_NUMSPECIALFACES = 3;

constexpr int ST_FACESTRIDE =
    ST_NUMSTRAIGHTFACES + ST_NUMTURNFACES + ST_NUMSPECIALFACES;

constexpr int ST_TURNOFFSET = ST_NUMSTRAIGHTFACES;
constexpr int ST_OUCHOFFSET = ST_TURNOFFSET + ST_NUMTURNFACES;
constexpr int ST_EVILGRINOFFSET = ST_OUCHOFFSET + 1;
constexpr int ST_RAMPAGEOFFSET = ST_EVILGRINOFFSET + 1;
constexpr int ST_GODFACE = ST_NUMPAINFACES * ST_FACESTRIDE;
constexpr int ST_DEADFACE = ST_GODFACE + 1;

// ST_DEADFACE is the highest index the face animation ever writes, and
// StatusBarGraphics sizes its patch array with its own numFaces, spelled out as
// (3 + 2 + 3) * 5 + 2 because that header deliberately does not include this file's
// constants. Two spellings of one number across a subsystem boundary, so they are
// tied with a static_assert - the same answer SAVESTRINGSIZE == menuSaveStringSize
// gets. A dead ST_NUMFACES macro used to sit here holding the same arithmetic a
// third time; deleting it is what made this check the only one, and StatusBarGraphics.h
// had been claiming a compile-time check that a retired reference-to-array binding
// used to provide.
static_assert(StatusBarGraphics::numFaces == ST_DEADFACE + 1,
              "the face patch array must hold every index the animation writes");

constexpr int ST_FACESX = 143;
constexpr int ST_FACESY = 168;

constexpr int ST_EVILGRINCOUNT = 2 * TICRATE;
constexpr int ST_STRAIGHTFACECOUNT = TICRATE / 2;
constexpr int ST_TURNCOUNT = 1 * TICRATE;
constexpr int ST_RAMPAGEDELAY = 2 * TICRATE;

constexpr int ST_MUCHPAIN = 20;

// Location and size of statistics,
// justified according to widget type.
// Problem is, within which space? STbar? Screen?
// Note: this could be read in by a lump.
//       Problem is, is the stuff rendered
//       into a buffer,
//       or into the frame buffer?

// AMMO number pos.
constexpr int ST_AMMOWIDTH = 3;
constexpr int ST_AMMOX = 44;
constexpr int ST_AMMOY = 171;

// HEALTH number pos.
constexpr int ST_HEALTHX = 90;
constexpr int ST_HEALTHY = 171;

// Weapon pos.
constexpr int ST_ARMSX = 111;
constexpr int ST_ARMSY = 172;
constexpr int ST_ARMSBGX = 104;
constexpr int ST_ARMSBGY = 168;
constexpr int ST_ARMSXSPACE = 12;
constexpr int ST_ARMSYSPACE = 10;

// Frags pos.
constexpr int ST_FRAGSX = 138;
constexpr int ST_FRAGSY = 171;
constexpr int ST_FRAGSWIDTH = 2;

// ARMOR number pos.
constexpr int ST_ARMORX = 221;
constexpr int ST_ARMORY = 171;

// Key icon positions.
constexpr int ST_KEY0X = 239;
constexpr int ST_KEY0Y = 171;
constexpr int ST_KEY1X = 239;
constexpr int ST_KEY1Y = 181;
constexpr int ST_KEY2X = 239;
constexpr int ST_KEY2Y = 191;

// Ammunition counter.
constexpr int ST_AMMO0WIDTH = 3;
constexpr int ST_AMMO0X = 288;
constexpr int ST_AMMO0Y = 173;
constexpr int ST_AMMO1WIDTH = ST_AMMO0WIDTH;
constexpr int ST_AMMO1X = 288;
constexpr int ST_AMMO1Y = 179;
constexpr int ST_AMMO2WIDTH = ST_AMMO0WIDTH;
constexpr int ST_AMMO2X = 288;
constexpr int ST_AMMO2Y = 191;
constexpr int ST_AMMO3WIDTH = ST_AMMO0WIDTH;
constexpr int ST_AMMO3X = 288;
constexpr int ST_AMMO3Y = 185;

// Indicate maximum ammunition.
// Only needed because backpack exists.
constexpr int ST_MAXAMMO0WIDTH = 3;
constexpr int ST_MAXAMMO0X = 314;
constexpr int ST_MAXAMMO0Y = 173;
constexpr int ST_MAXAMMO1WIDTH = ST_MAXAMMO0WIDTH;
constexpr int ST_MAXAMMO1X = 314;
constexpr int ST_MAXAMMO1Y = 179;
constexpr int ST_MAXAMMO2WIDTH = ST_MAXAMMO0WIDTH;
constexpr int ST_MAXAMMO2X = 314;
constexpr int ST_MAXAMMO2Y = 191;
constexpr int ST_MAXAMMO3WIDTH = ST_MAXAMMO0WIDTH;
constexpr int ST_MAXAMMO3X = 314;
constexpr int ST_MAXAMMO3Y = 185;

// The status bar's residual runtime state is a StatusBarState owned by the Engine
// (StatusBarState.h); the loaded patches are a StatusBarGraphics (StatusBarGraphics.h,
// faces[ST_NUMFACES] sized off the same ST_NUMFACES macro); the STlib widgets are a
// StatusBarWidgets (StatusBarWidgets.h); and the animated face's selection state is a
// StatusBarFace (StatusBarFace.h). All four used to be reached through file-scope
// `static T& x = cluster().x;` reference aliases (moved in by the file-scope-statics sweep,
// REFACTOR.md Step 5); the file-local-alias sweep (REFACTOR.md, Step 9 strand (a)) retired them -
// every function below reaches its cluster(s) through a hoisted local instead, taken once per
// function and reused for however many of that cluster's members the function touches.

// Massive bunches of cheat shit
//  to keep it from being easy to figure them out.
// Yeah, right...
Array<unsigned char, 9> cheat_mus_seq = {
    0xb2, 0x26, 0xb6, 0xae, 0xea, 1, 0, 0, 0xff};

Array<unsigned char, 11> cheat_choppers_seq = {
    0xb2, 0x26, 0xe2, 0x32, 0xf6, 0x2a, 0x2a, 0xa6, 0x6a, 0xea, 0xff // id...
};

Array<unsigned char, 6> cheat_god_seq = {
    0xb2, 0x26, 0x26, 0xaa, 0x26, 0xff // iddqd
};

Array<unsigned char, 6> cheat_ammo_seq = {
    0xb2, 0x26, 0xf2, 0x66, 0xa2, 0xff // idkfa
};

Array<unsigned char, 5> cheat_ammonokey_seq = {
    0xb2, 0x26, 0x66, 0xa2, 0xff // idfa
};

// Smashing Pumpkins Into Samml Piles Of Putried Debris.
Array<unsigned char, 11> cheat_noclip_seq = {0xb2,
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
Array<unsigned char, 7> cheat_commercial_noclip_seq = {
    0xb2, 0x26, 0xe2, 0x36, 0xb2, 0x2a, 0xff // idclip
};

Array<Array<unsigned char, 10>, 7> cheat_powerup_seq = {
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x6e, 0xff}, // beholdv
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xea, 0xff}, // beholds
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xb2, 0xff}, // beholdi
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x6a, 0xff}, // beholdr
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xa2, 0xff}, // beholda
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x36, 0xff}, // beholdl
    {0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xff} // behold
};

Array<unsigned char, 10> cheat_clev_seq = {
    0xb2, 0x26, 0xe2, 0x36, 0xa6, 0x6e, 1, 0, 0, 0xff // idclev
};

// my position cheat
Array<unsigned char, 8> cheat_mypos_seq = {
    0xb2, 0x26, 0xb6, 0xba, 0x2a, 0xf6, 0xea, 0xff // idmypos
};

// Now what?
CheatSequence cheat_mus = {{cheat_mus_seq}};
CheatSequence cheat_god = {{cheat_god_seq}};
CheatSequence cheat_ammo = {{cheat_ammo_seq}};
CheatSequence cheat_ammonokey = {{cheat_ammonokey_seq}};
CheatSequence cheat_noclip = {{cheat_noclip_seq}};
CheatSequence cheat_commercial_noclip = {{cheat_commercial_noclip_seq}};

Array<CheatSequence, 7> cheat_powerup = {CheatSequence {{cheat_powerup_seq[0]}},
                                         CheatSequence {{cheat_powerup_seq[1]}},
                                         CheatSequence {{cheat_powerup_seq[2]}},
                                         CheatSequence {{cheat_powerup_seq[3]}},
                                         CheatSequence {{cheat_powerup_seq[4]}},
                                         CheatSequence {{cheat_powerup_seq[5]}},
                                         CheatSequence {{cheat_powerup_seq[6]}}};

CheatSequence cheat_choppers = {{cheat_choppers_seq}};
CheatSequence cheat_clev = {{cheat_clev_seq}};
CheatSequence cheat_mypos = {{cheat_mypos_seq}};

void stopStatusBar();

//
// STATUS BAR CODE
//

void refreshBackground()
{
    if (statusBarState().st_statusbaron)
    {
        auto& gfx = statusBarGraphics();

        drawPatch(ST_X, 0, STLIB_BG, gfx.sbar);

        if (gameSession().netgame)
            drawPatch(ST_FX, 0, STLIB_BG, gfx.faceback);

        copyRect(ST_X, 0, STLIB_BG, ST_WIDTH, ST_HEIGHT, ST_X, ST_Y, STLIB_FG);
    }
}

// Respond to keyboard input events,
//  intercept cheats.
bool statusBarResponder(Event& ev)
{
    auto& bar = statusBarState();

    // Filter automap on/off.
    if (ev.type == EventType::KeyUp && ((ev.data1 & 0xffff0000) == AM_MSGHEADER))
    {
        switch (ev.data1)
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
    else if (ev.type == EventType::KeyDown)
    {
        if (!gameSession().netgame)
        {
            // b. - enabled for more debug fun.
            // if (gameskill != Skill::Nightmare) {

            // 'dqd' cheat for toggleable god mode
            if (checkCheat(cheat_god, ev.data1))
            {
                bar.plyr->cheats =
                    toggledFlags(bar.plyr->cheats, CheatFlag::GodMode);
                if (hasFlag(bar.plyr->cheats, CheatFlag::GodMode))
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
            else if (checkCheat(cheat_ammonokey, ev.data1))
            {
                bar.plyr->armorpoints = 200;
                bar.plyr->armortype = 2;

                for (auto& i: bar.plyr->weaponowned)
                    i = true;

                for (int i = 0; i < numAmmo; i++)
                    bar.plyr->ammo[i] = bar.plyr->maxammo[i];

                bar.plyr->message = STSTR_FAADDED;
            }
            // 'kfa' cheat for key full ammo
            else if (checkCheat(cheat_ammo, ev.data1))
            {
                bar.plyr->armorpoints = 200;
                bar.plyr->armortype = 2;

                for (int i = 0; i < numWeapons; i++)
                    bar.plyr->weaponowned[i] = true;

                for (int i = 0; i < numAmmo; i++)
                    bar.plyr->ammo[i] = bar.plyr->maxammo[i];

                for (bool& card: bar.plyr->cards)
                    card = true;

                bar.plyr->message = STSTR_KFAADDED;
            }
            // 'mus' cheat for changing music
            else if (checkCheat(cheat_mus, ev.data1))
            {
                MusicEnum musnum;

                bar.plyr->message = STSTR_MUS;
                const auto buf = getParam(cheat_mus);

                if (gameVersion().gamemode == GameMode::Commercial)
                {
                    musnum = static_cast<MusicEnum>(toIndex(MusicEnum::Runnin)
                                                    + (buf[0] - '0') * 10 + buf[1]
                                                    - '0' - 1);

                    if (((buf[0] - '0') * 10 + buf[1] - '0') > 35)
                        bar.plyr->message = STSTR_NOMUS;
                    else
                        changeMusic(musnum, 1);
                }
                else
                {
                    musnum = static_cast<MusicEnum>(toIndex(MusicEnum::E1m1)
                                                    + (buf[0] - '1') * 9
                                                    + (buf[1] - '1'));

                    if (((buf[0] - '1') * 9 + buf[1] - '1') > 31)
                        bar.plyr->message = STSTR_NOMUS;
                    else
                        changeMusic(musnum, 1);
                }
            }
            // Simplified, accepting both "noclip" and "idspispopd".
            // no clipping mode cheat
            else if (checkCheat(cheat_noclip, ev.data1)
                     || checkCheat(cheat_commercial_noclip, ev.data1))
            {
                bar.plyr->cheats = toggledFlags(bar.plyr->cheats, CheatFlag::NoClip);

                if (hasFlag(bar.plyr->cheats, CheatFlag::NoClip))
                    bar.plyr->message = STSTR_NCON;
                else
                    bar.plyr->message = STSTR_NCOFF;
            }
            // 'behold?' power-up cheats
            for (int i = 0; i < 6; i++)
            {
                if (checkCheat(cheat_powerup[i], ev.data1))
                {
                    if (!bar.plyr->powers[i])
                        givePower(*bar.plyr, static_cast<PowerType>(i));
                    else if (i != toIndex(PowerType::Strength))
                        bar.plyr->powers[i] = 1;
                    else
                        bar.plyr->powers[i] = 0;

                    bar.plyr->message = STSTR_BEHOLDX;
                }
            }

            // 'behold' power-up menu
            if (checkCheat(cheat_powerup[6], ev.data1))
            {
                bar.plyr->message = STSTR_BEHOLD;
            }
            // 'choppers' invulnerability & chainsaw
            else if (checkCheat(cheat_choppers, ev.data1))
            {
                bar.plyr->weaponowned[toIndex(WeaponType::Chainsaw)] = true;
                bar.plyr->powers[toIndex(PowerType::Invulnerability)] = true;
                bar.plyr->message = STSTR_CHOPPERS;
            }
            // 'mypos' for player position
            else if (checkCheat(cheat_mypos, ev.data1))
            {
                static std::string buf;
                //doom_sprintf(buf, "ang=0x%x;x,y=(0x%x,0x%x)",
                //        players[consoleplayer].mo->angle,
                //        players[consoleplayer].mo->x,
                //        players[consoleplayer].mo->y);
                auto& players_ = playerState();
                const auto* mo = players_.players[players_.consoleplayer].mo;

                buf = concat("ang=0x",
                             hexString((int) mo->angle.raw),
                             ";x,y=(0x",
                             hexString(mo->x.raw),
                             ",0x",
                             hexString(mo->y.raw),
                             ")");
                bar.plyr->message = buf;
            }
        }

        // 'clev' change-level cheat
        if (checkCheat(cheat_clev, ev.data1))
        {
            int epsd;
            int map;

            const auto buf = getParam(cheat_clev);

            const auto& version = gameVersion();

            if (version.gamemode == GameMode::Commercial)
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
            if ((version.gamemode == GameMode::Retail) && ((epsd > 4) || (map > 9)))
                return false;

            if ((version.gamemode == GameMode::Registered)
                && ((epsd > 3) || (map > 9)))
                return false;

            if ((version.gamemode == GameMode::Shareware)
                && ((epsd > 1) || (map > 9)))
                return false;

            if ((version.gamemode == GameMode::Commercial)
                && ((epsd > 1) || (map > 34)))
                return false;

            // So be it.
            bar.plyr->message = STSTR_CLEV;
            deferInitNew(gameSession().gameskill, epsd, map);
        }
    }
    return false;
}

int calcPainOffset()
{
    // The pain-offset cache: StatusBarFace members (Engine) now, reached by a local
    // reference (formerly function-local statics, never reset - identical persistence).
    int& lastcalc = statusBarFace().lastcalc;
    int& oldhealth = statusBarFace().oldhealth;
    auto& bar = statusBarState();

    int health = bar.plyr->health > 100 ? 100 : bar.plyr->health;

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
    Angle diffang;
    // The face state machine's carry: StatusBarFace members (Engine) now, reached by a
    // hoisted local (formerly function-local statics, never reset - identical persistence).
    auto& face = statusBarFace();
    auto& bar = statusBarState();

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
            bool doevilgrin = false;

            for (i = 0; i < numWeapons; i++)
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
                Angle badguyangle = pointToAngle2(bar.plyr->mo->x,
                                                  bar.plyr->mo->y,
                                                  bar.plyr->attacker->x,
                                                  bar.plyr->attacker->y);

                if (badguyangle > bar.plyr->mo->angle)
                {
                    // whether right or left
                    diffang = badguyangle - bar.plyr->mo->angle;
                    i = diffang > ang180;
                }
                else
                {
                    // whether left or right
                    diffang = bar.plyr->mo->angle - badguyangle;
                    i = diffang <= ang180;
                } // confusing, aint it?

                face.st_facecount = ST_TURNCOUNT;
                face.st_faceindex = calcPainOffset();

                if (diffang < ang45)
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
        if ((hasFlag(bar.plyr->cheats, CheatFlag::GodMode))
            || bar.plyr->powers[toIndex(PowerType::Invulnerability)])
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
    // The "n/a" ammo sentinel: a StatusBarWidgets member (Engine) now, reached through the
    // hoisted cluster (w_ready.num takes its address, so the member's stable address is what it
    // needs).
    auto& widgets = statusBarWidgets();

    if (weaponinfo()[toIndex(bar.plyr->readyweapon)].ammo == AmmoType::NoAmmo)
        widgets.w_ready.num = &widgets.largeammo;
    else
        widgets.w_ready.num =
            &bar.plyr
                 ->ammo[toIndex(weaponinfo()[toIndex(bar.plyr->readyweapon)].ammo)];

    widgets.w_ready.data = toIndex(bar.plyr->readyweapon);

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

    face.st_randomnumber = randomness().forMenu();
    updateWidgets();
    face.st_oldhealth = bar.plyr->health;
}

void doPaletteStuff()
{
    auto& bar = statusBarState();

    int palette;

    int cnt = bar.plyr->damagecount;

    if (bar.plyr->powers[toIndex(PowerType::Strength)])
    {
        // slowly fade the berzerk out
        int bzc = 12 - (bar.plyr->powers[toIndex(PowerType::Strength)] >> 6);

        cnt = std::max(cnt, bzc);
    }

    if (cnt)
    {
        palette = (cnt + 7) >> 3;

        palette = std::min(palette, NUMREDPALS - 1);

        palette += STARTREDPALS;
    }

    else if (bar.plyr->bonuscount)
    {
        palette = (bar.plyr->bonuscount + 7) >> 3;

        palette = std::min(palette, NUMBONUSPALS - 1);

        palette += STARTBONUSPALS;
    }

    else if (bar.plyr->powers[toIndex(PowerType::IronFeet)] > 4 * 32
             || bar.plyr->powers[toIndex(PowerType::IronFeet)] & 8)
        palette = RADIATIONPAL;
    else
        palette = 0;

    if (palette != bar.st_palette)
    {
        bar.st_palette = palette;
        byte* pal = static_cast<byte*>(cacheLumpNum(bar.lu_palette)) + palette * 768;
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

    updateNum(widgets.w_ready, refresh);

    for (int i = 0; i < 4; i++)
    {
        updateNum(widgets.w_ammo[i], refresh);
        updateNum(widgets.w_maxammo[i], refresh);
    }

    updatePercent(widgets.w_health, refresh);
    updatePercent(widgets.w_armor, refresh);

    updateBinIcon(widgets.w_armsbg, refresh);

    // The arms icons read w_armsindex, not the player directly - see StatusBarWidgets.h.
    // Refreshed here, immediately before the update, so each icon sees exactly the
    // weaponowned value the old (int*) pun would have read at this same point.
    for (int i = 0; i < 6; i++)
        widgets.w_armsindex[i] = bar.plyr->weaponowned[i + 1];

    for (auto& arm: widgets.w_arms)
        updateMultIcon(arm, refresh);

    updateMultIcon(widgets.w_faces, refresh);

    for (auto& keybox: widgets.w_keyboxes)
        updateMultIcon(keybox, refresh);

    updateNum(widgets.w_frags, refresh);
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
    if (host().flags & DOOM_FLAG_MENU_DARKEN_BG)
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
    auto& gfx = statusBarGraphics();

    // Load the numbers, tall and short
    for (int i = 0; i < 10; i++)
    {
        gfx.tallnum[i] = static_cast<Patch*>(cacheLumpName(concat("STTNUM", i)));
        gfx.shortnum[i] = static_cast<Patch*>(cacheLumpName(concat("STYSNUM", i)));
    }

    // Load percent key.
    //Note: why not load STMINUS here, too?
    gfx.tallpercent = static_cast<Patch*>(cacheLumpName("STTPRCNT"));

    // key cards
    for (int i = 0; i < numCards; i++)
    {
        gfx.keys[i] = static_cast<Patch*>(cacheLumpName(concat("STKEYS", i)));
    }

    // arms background
    gfx.armsbg = static_cast<Patch*>(cacheLumpName("STARMS"));

    // arms ownership widgets
    for (int i = 0; i < 6; i++)
    {
        // gray #
        gfx.arms[i][0] = static_cast<Patch*>(cacheLumpName(concat("STGNUM", i + 2)));

        // yellow #
        gfx.arms[i][1] = gfx.shortnum[i + 2];
    }

    // face backgrounds for different color players
    gfx.faceback = static_cast<Patch*>(
        cacheLumpName(concat("STFB", playerState().consoleplayer)));

    // status bar background bits
    gfx.sbar = static_cast<Patch*>(cacheLumpName("STBAR"));

    // face states
    int facenum = 0;
    for (int i = 0; i < ST_NUMPAINFACES; i++)
    {
        for (int j = 0; j < ST_NUMSTRAIGHTFACES; j++)
        {
            gfx.faces[facenum++] =
                static_cast<Patch*>(cacheLumpName(concat("STFST", i, j)));
        }
        // turn right
        gfx.faces[facenum++] =
            static_cast<Patch*>(cacheLumpName(concat("STFTR", i, "0")));
        // turn left
        gfx.faces[facenum++] =
            static_cast<Patch*>(cacheLumpName(concat("STFTL", i, "0")));
        // ouch!
        gfx.faces[facenum++] =
            static_cast<Patch*>(cacheLumpName(concat("STFOUCH", i)));
        // evil grin ;)
        gfx.faces[facenum++] =
            static_cast<Patch*>(cacheLumpName(concat("STFEVL", i)));
        // pissed off
        gfx.faces[facenum++] =
            static_cast<Patch*>(cacheLumpName(concat("STFKILL", i)));
    }
    gfx.faces[facenum++] = static_cast<Patch*>(cacheLumpName("STFGOD0"));
    gfx.faces[facenum++] = static_cast<Patch*>(cacheLumpName("STFDEAD0"));
}

void loadData()
{
    statusBarState().lu_palette = wad().number("PLAYPAL");
    loadGraphics();
}

void unloadGraphics()
{
    // Nothing to unload any more: WadFile owns the lumps and they are
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

    std::copy_n(bar.plyr->weaponowned, numWeapons, face.oldweaponsowned.begin());

    bar.keyboxes.fill(-1);

    initStatusWidgets();
}

void createWidgets()
{
    auto& bar = statusBarState();
    auto& widgets = statusBarWidgets();
    auto& gfx = statusBarGraphics();

    // ready weapon ammo
    initNum(
        widgets.w_ready,
        ST_AMMOX,
        ST_AMMOY,
        gfx.tallnum.data(),
        &bar.plyr->ammo[toIndex(weaponinfo()[toIndex(bar.plyr->readyweapon)].ammo)],
        &bar.st_statusbaron,
        ST_AMMOWIDTH);

    // the last weapon type
    widgets.w_ready.data = toIndex(bar.plyr->readyweapon);

    // health percentage
    initPercent(widgets.w_health,
                ST_HEALTHX,
                ST_HEALTHY,
                gfx.tallnum.data(),
                &bar.plyr->health,
                &bar.st_statusbaron,
                gfx.tallpercent);

    // arms background
    initBinIcon(widgets.w_armsbg,
                ST_ARMSBGX,
                ST_ARMSBGY,
                gfx.armsbg,
                &bar.st_notdeathmatch,
                &bar.st_statusbaron);

    // weapons owned
    for (int i = 0; i < 6; i++)
    {
        initMultIcon(widgets.w_arms[i],
                     ST_ARMSX + (i % 3) * ST_ARMSXSPACE,
                     ST_ARMSY + (i / 3) * ST_ARMSYSPACE,
                     gfx.arms[i].data(),
                     &widgets.w_armsindex[i],
                     &bar.st_armson);
    }

    // frags sum
    initNum(widgets.w_frags,
            ST_FRAGSX,
            ST_FRAGSY,
            gfx.tallnum.data(),
            &bar.st_fragscount,
            &bar.st_fragson,
            ST_FRAGSWIDTH);

    // faces
    initMultIcon(widgets.w_faces,
                 ST_FACESX,
                 ST_FACESY,
                 gfx.faces.data(),
                 &statusBarFace().st_faceindex,
                 &bar.st_statusbaron);

    // armor percentage - should be colored later
    initPercent(widgets.w_armor,
                ST_ARMORX,
                ST_ARMORY,
                gfx.tallnum.data(),
                &bar.plyr->armorpoints,
                &bar.st_statusbaron,
                gfx.tallpercent);

    // keyboxes 0-2
    initMultIcon(widgets.w_keyboxes[0],
                 ST_KEY0X,
                 ST_KEY0Y,
                 gfx.keys.data(),
                 &bar.keyboxes[0],
                 &bar.st_statusbaron);

    initMultIcon(widgets.w_keyboxes[1],
                 ST_KEY1X,
                 ST_KEY1Y,
                 gfx.keys.data(),
                 &bar.keyboxes[1],
                 &bar.st_statusbaron);

    initMultIcon(widgets.w_keyboxes[2],
                 ST_KEY2X,
                 ST_KEY2Y,
                 gfx.keys.data(),
                 &bar.keyboxes[2],
                 &bar.st_statusbaron);

    // ammo count (all four kinds)
    initNum(widgets.w_ammo[0],
            ST_AMMO0X,
            ST_AMMO0Y,
            gfx.shortnum.data(),
            &bar.plyr->ammo[0],
            &bar.st_statusbaron,
            ST_AMMO0WIDTH);

    initNum(widgets.w_ammo[1],
            ST_AMMO1X,
            ST_AMMO1Y,
            gfx.shortnum.data(),
            &bar.plyr->ammo[1],
            &bar.st_statusbaron,
            ST_AMMO1WIDTH);

    initNum(widgets.w_ammo[2],
            ST_AMMO2X,
            ST_AMMO2Y,
            gfx.shortnum.data(),
            &bar.plyr->ammo[2],
            &bar.st_statusbaron,
            ST_AMMO2WIDTH);

    initNum(widgets.w_ammo[3],
            ST_AMMO3X,
            ST_AMMO3Y,
            gfx.shortnum.data(),
            &bar.plyr->ammo[3],
            &bar.st_statusbaron,
            ST_AMMO3WIDTH);

    // max ammo count (all four kinds)
    initNum(widgets.w_maxammo[0],
            ST_MAXAMMO0X,
            ST_MAXAMMO0Y,
            gfx.shortnum.data(),
            &bar.plyr->maxammo[0],
            &bar.st_statusbaron,
            ST_MAXAMMO0WIDTH);

    initNum(widgets.w_maxammo[1],
            ST_MAXAMMO1X,
            ST_MAXAMMO1Y,
            gfx.shortnum.data(),
            &bar.plyr->maxammo[1],
            &bar.st_statusbaron,
            ST_MAXAMMO1WIDTH);

    initNum(widgets.w_maxammo[2],
            ST_MAXAMMO2X,
            ST_MAXAMMO2Y,
            gfx.shortnum.data(),
            &bar.plyr->maxammo[2],
            &bar.st_statusbaron,
            ST_MAXAMMO2WIDTH);

    initNum(widgets.w_maxammo[3],
            ST_MAXAMMO3X,
            ST_MAXAMMO3Y,
            gfx.shortnum.data(),
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

    setPalette(static_cast<byte*>(cacheLumpNum(bar.lu_palette)));

    bar.st_stopped = true;
}

void initStatusBar()
{
    loadData();
    // RAII now (Step 9): the status bar back-buffer is a VideoState-owned vector;
    // screens[4] is the raw view onto its data(). initStatusBar runs once at boot.
    auto& statusBar = videoState().statusBar;
    statusBar.resize(ST_WIDTH * ST_HEIGHT);
    videoState().screens[4] = statusBar.data();
}

} // namespace Doom
