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
//        Game completion, final screen animation.
//
//-----------------------------------------------------------------------------

// Rewritten out of vanilla f_finale into namespace Doom.
//
// The end-of-episode finale: the scrolling text screen, the character cast call,
// and the DOOM II bunny scroll. The finale's runtime state is a Doom::FinaleState
// owned by the Engine (UI/FinaleState.h), reached through a hoisted local per
// function; the two externs below are other subsystems' globals. It has its own
// frame golden now (Tests/Goldens/finale.frames, via Tests/FinaleReplay.h - no
// demo reaches a finale, so nothing else covers this file).

#include "../Host/Platform.h"

#include "../Game/MapSpawns.h"
#include "../Game/Strings.h" // Data.
#include "Hud.h"
#include "../Math/Swap.h" // Functions.
#include "../Game/SoundData.h" // Data.
#include "../Game/GameFlow.h"
#include "../Game/GameSession.h"
#include "../Game/GameVersion.h"
#include "../Game/OverlayState.h"
#include "../Game/PlayerState.h"
#include "../Game/RefreshFlags.h"
#include "../Render/GraphicsData.h"
#include "../Wad/WadFile.h"

#include "Finale.h"
#include "FinaleState.h"
#include "HudFont.h"

#include "../Render/Video.h"
#include <ea_data_structures/Structures/Array.h>

// Other subsystems' globals/functions this file reads.
#include "../Game/Sound.h"
void Doom::drawPatchFlipped(int x, int y, int scrn, Patch* patch); // v_video

namespace Doom
{

constexpr int TEXTSPEED = 3;
constexpr int TEXTWAIT = 250;

struct CastInfo
{
    const char* name;
    MobjType type;
};

// The finale's runtime state lives on the Engine (UI/FinaleState.h, moved by the
// file-scope-statics sweep - REFACTOR.md, Step 5). It used to be reached through file-scope
// `static T& x = finaleState().x;` reference aliases until the file-local-alias sweep (REFACTOR.md,
// Step 9 strand (a)) retired them; every function below reaches finaleState() through a hoisted
// local instead (or inline where a function touches it exactly once). The immutable reference data -
// the per-ending text pointers and the castorder[] cast list - stays file-local: it is fixed
// constants, not per-run state, and never moved onto FinaleState.

const char* e1text = E1TEXT;
const char* e2text = E2TEXT;
const char* e3text = E3TEXT;
const char* e4text = E4TEXT;

const char* c1text = C1TEXT;
const char* c2text = C2TEXT;
const char* c3text = C3TEXT;
const char* c4text = C4TEXT;
const char* c5text = C5TEXT;
const char* c6text = C6TEXT;

const char* p1text = P1TEXT;
const char* p2text = P2TEXT;
const char* p3text = P3TEXT;
const char* p4text = P4TEXT;
const char* p5text = P5TEXT;
const char* p6text = P6TEXT;

const char* t1text = T1TEXT;
const char* t2text = T2TEXT;
const char* t3text = T3TEXT;
const char* t4text = T4TEXT;
const char* t5text = T5TEXT;
const char* t6text = T6TEXT;

EA::Array<CastInfo, 18> castorder = {{CC_ZOMBIE, MT_POSSESSED},
                                     {CC_SHOTGUN, MT_SHOTGUY},
                                     {CC_HEAVY, MT_CHAINGUY},
                                     {CC_IMP, MT_TROOP},
                                     {CC_DEMON, MT_SERGEANT},
                                     {CC_LOST, MT_SKULL},
                                     {CC_CACO, MT_HEAD},
                                     {CC_HELL, MT_KNIGHT},
                                     {CC_BARON, MT_BRUISER},
                                     {CC_ARACH, MT_BABY},
                                     {CC_PAIN, MT_PAIN},
                                     {CC_REVEN, MT_UNDEAD},
                                     {CC_MANCU, MT_FATSO},
                                     {CC_ARCH, MT_VILE},
                                     {CC_SPIDER, MT_SPIDER},
                                     {CC_CYBER, MT_CYBORG},
                                     {CC_HERO, MT_PLAYER},

                                     {0, static_cast<MobjType>(0)}};

//
// startCast
//

void startCast();
void castTicker();
bool castResponder(Event* ev);
void castDrawer();

//
// startFinale
//
void startFinale()
{
    auto& flow = gameFlow();
    auto& fin = finaleState();

    flow.gameaction = ga_nothing;
    flow.gamestate = GS_FINALE;
    refreshFlags().viewactive = false;
    overlayState().automapactive = false;

    // Okay - IWAD dependend stuff.
    // This has been changed severly, and
    //  some stuff might have changed in the process.
    switch (gameVersion().gamemode)
    {
        // DOOM 1 - E1, E3 or E4, but each nine missions
        case shareware:
        case registered:
        case retail:
        {
            changeMusic(mus_victor, true);

            switch (gameSession().gameepisode)
            {
                case 1:
                    fin.finaleflat = "FLOOR4_8";
                    fin.finaletext = e1text;
                    break;
                case 2:
                    fin.finaleflat = "SFLR6_1";
                    fin.finaletext = e2text;
                    break;
                case 3:
                    fin.finaleflat = "MFLR8_4";
                    fin.finaletext = e3text;
                    break;
                case 4:
                    fin.finaleflat = "MFLR8_3";
                    fin.finaletext = e4text;
                    break;
                default:
                    // Ouch.
                    break;
            }
            break;
        }

        // DOOM II and missions packs with E1, M34
        case commercial:
        {
            changeMusic(mus_read_m, true);

            switch (gameSession().gamemap)
            {
                case 6:
                    fin.finaleflat = "SLIME16";
                    fin.finaletext = c1text;
                    break;
                case 11:
                    fin.finaleflat = "RROCK14";
                    fin.finaletext = c2text;
                    break;
                case 20:
                    fin.finaleflat = "RROCK07";
                    fin.finaletext = c3text;
                    break;
                case 30:
                    fin.finaleflat = "RROCK17";
                    fin.finaletext = c4text;
                    break;
                case 15:
                    fin.finaleflat = "RROCK13";
                    fin.finaletext = c5text;
                    break;
                case 31:
                    fin.finaleflat = "RROCK19";
                    fin.finaletext = c6text;
                    break;
                default:
                    // Ouch.
                    break;
            }
            break;
        }

        // Indeterminate.
        default:
            changeMusic(mus_read_m, true);
            fin.finaleflat = "F_SKY1"; // Not used anywhere else.
            fin.finaletext = c1text; // FIXME - other text, music?
            break;
    }

    fin.finalestage = 0;
    fin.finalecount = 0;
}

bool finaleResponder(Event* event)
{
    if (finaleState().finalestage == 2)
        return castResponder(event);

    return false;
}

//
// finaleTicker
//
void finaleTicker()
{
    int i;

    const auto& version = gameVersion();
    const auto& players_ = playerState();
    auto& fin = finaleState();

    // check for skipping
    if ((version.gamemode == commercial) && (fin.finalecount > 50))
    {
        // go on to the next level
        for (i = 0; i < MAXPLAYERS; i++)
            if (players_.players[i].cmd.buttons)
                break;

        if (i < MAXPLAYERS)
        {
            if (gameSession().gamemap == 30)
                startCast();
            else
                gameFlow().gameaction = ga_worlddone;
        }
    }

    // advance animation
    fin.finalecount++;

    if (fin.finalestage == 2)
    {
        castTicker();
        return;
    }

    if (version.gamemode == commercial)
        return;

    if (!fin.finalestage
        && fin.finalecount > doom_strlen(fin.finaletext) * TEXTSPEED + TEXTWAIT)
    {
        fin.finalecount = 0;
        fin.finalestage = 1;
        gameFlow().wipegamestate = GS_FORCE_WIPE;
        if (gameSession().gameepisode == 3)
            startMusic(mus_bunny);
    }
}

//
// textWrite
//
void textWrite()
{
    auto& font = hudFont();
    auto& fin = finaleState();

    // erase the entire screen to a tiled background
    byte* src = static_cast<byte*>(cacheLumpName(fin.finaleflat));
    byte* dest = screens[0];

    for (int y = 0; y < SCREENHEIGHT; y++)
    {
        for (int x = 0; x < SCREENWIDTH / 64; x++)
        {
            doom_memcpy(dest, src + ((y & 63) << 6), 64);
            dest += 64;
        }
        if (SCREENWIDTH & 63)
        {
            doom_memcpy(dest, src + ((y & 63) << 6), SCREENWIDTH & 63);
            dest += (SCREENWIDTH & 63);
        }
    }

    markRect(0, 0, SCREENWIDTH, SCREENHEIGHT);

    // draw some of the text onto the screen
    int cx = 10;
    int cy = 10;
    const char* ch = fin.finaletext;

    int count = (fin.finalecount - 10) / TEXTSPEED;
    if (count < 0)
        count = 0;
    for (; count; count--)
    {
        int c = *ch++;
        if (!c)
            break;
        if (c == '\n')
        {
            cx = 10;
            cy += 11;
            continue;
        }

        c = doom_toupper(c) - HU_FONTSTART;
        if (c < 0 || c > HU_FONTSIZE)
        {
            cx += 4;
            continue;
        }

        int w = littleEndian(font.hu_font[c]->width);
        if (cx + w > SCREENWIDTH)
            break;
        drawPatch(cx, cy, 0, font.hu_font[c]);
        cx += w;
    }
}

//
// Final DOOM 2 animation
// Casting by id Software.
//   in order of appearance
//
void startCast()
{
    auto& fin = finaleState();

    gameFlow().wipegamestate = GS_FORCE_WIPE;
    fin.castnum = 0;
    fin.caststate = &states[mobjinfo[castorder[fin.castnum].type].seestate];
    fin.casttics = fin.caststate->tics;
    fin.castdeath = false;
    fin.finalestage = 2;
    fin.castframes = 0;
    fin.castonmelee = 0;
    fin.castattacking = false;
    changeMusic(mus_evil, true);
}

//
// castTicker
//
void castTicker()
{
    auto& fin = finaleState();

    int st;
    int sfx;

    if (--fin.casttics > 0)
        return; // not time to change state yet

    if (fin.caststate->tics == -1 || fin.caststate->nextstate == S_NULL)
    {
        // switch from deathstate to next monster
        fin.castnum++;
        fin.castdeath = false;
        if (castorder[fin.castnum].name == nullptr)
            fin.castnum = 0;
        if (mobjinfo[castorder[fin.castnum].type].seesound)
            startSound(0, mobjinfo[castorder[fin.castnum].type].seesound);
        fin.caststate = &states[mobjinfo[castorder[fin.castnum].type].seestate];
        fin.castframes = 0;
    }
    else
    {
        // just advance to next state in animation
        if (fin.caststate == &states[S_PLAY_ATK1])
            goto stopattack; // Oh, gross hack!
        st = fin.caststate->nextstate;
        fin.caststate = &states[st];
        fin.castframes++;

        // sound hacks....
        switch (st)
        {
            case S_PLAY_ATK1:
                sfx = sfx_dshtgn;
                break;
            case S_POSS_ATK2:
                sfx = sfx_pistol;
                break;
            case S_SPOS_ATK2:
                sfx = sfx_shotgn;
                break;
            case S_VILE_ATK2:
                sfx = sfx_vilatk;
                break;
            case S_SKEL_FIST2:
                sfx = sfx_skeswg;
                break;
            case S_SKEL_FIST4:
                sfx = sfx_skepch;
                break;
            case S_SKEL_MISS2:
                sfx = sfx_skeatk;
                break;
            case S_FATT_ATK8:
            case S_FATT_ATK5:
            case S_FATT_ATK2:
                sfx = sfx_firsht;
                break;
            case S_CPOS_ATK2:
            case S_CPOS_ATK3:
            case S_CPOS_ATK4:
                sfx = sfx_shotgn;
                break;
            case S_TROO_ATK3:
                sfx = sfx_claw;
                break;
            case S_SARG_ATK2:
                sfx = sfx_sgtatk;
                break;
            case S_BOSS_ATK2:
            case S_BOS2_ATK2:
            case S_HEAD_ATK2:
                sfx = sfx_firsht;
                break;
            case S_SKULL_ATK2:
                sfx = sfx_sklatk;
                break;
            case S_SPID_ATK2:
            case S_SPID_ATK3:
                sfx = sfx_shotgn;
                break;
            case S_BSPI_ATK2:
                sfx = sfx_plasma;
                break;
            case S_CYBER_ATK2:
            case S_CYBER_ATK4:
            case S_CYBER_ATK6:
                sfx = sfx_rlaunc;
                break;
            case S_PAIN_ATK3:
                sfx = sfx_sklatk;
                break;
            default:
                sfx = 0;
                break;
        }

        if (sfx)
            startSound(0, sfx);
    }

    if (fin.castframes == 12)
    {
        // go into attack frame
        fin.castattacking = true;
        if (fin.castonmelee)
            fin.caststate =
                &states[mobjinfo[castorder[fin.castnum].type].meleestate];
        else
            fin.caststate =
                &states[mobjinfo[castorder[fin.castnum].type].missilestate];
        fin.castonmelee ^= 1;
        if (fin.caststate == &states[S_NULL])
        {
            if (fin.castonmelee)
                fin.caststate =
                    &states[mobjinfo[castorder[fin.castnum].type].meleestate];
            else
                fin.caststate =
                    &states[mobjinfo[castorder[fin.castnum].type].missilestate];
        }
    }

    if (fin.castattacking)
    {
        if (fin.castframes == 24
            || fin.caststate
                   == &states[mobjinfo[castorder[fin.castnum].type].seestate])
        {
        stopattack:
            fin.castattacking = false;
            fin.castframes = 0;
            fin.caststate = &states[mobjinfo[castorder[fin.castnum].type].seestate];
        }
    }

    fin.casttics = fin.caststate->tics;
    if (fin.casttics == -1)
        fin.casttics = 15;
}

//
// castResponder
//
bool castResponder(Event* ev)
{
    auto& fin = finaleState();

    if (ev->type != ev_keydown)
        return false;

    if (fin.castdeath)
        return true; // already in dying frames

    // go into death frame
    fin.castdeath = true;
    fin.caststate = &states[mobjinfo[castorder[fin.castnum].type].deathstate];
    fin.casttics = fin.caststate->tics;
    fin.castframes = 0;
    fin.castattacking = false;
    if (mobjinfo[castorder[fin.castnum].type].deathsound)
        startSound(0, mobjinfo[castorder[fin.castnum].type].deathsound);

    return true;
}

void castPrint(const char* text)
{
    auto& font = hudFont();

    int c;
    int w;

    // find width
    const char* ch = text;
    int width = 0;

    while (ch)
    {
        c = *ch++;
        if (!c)
            break;
        c = doom_toupper(c) - HU_FONTSTART;
        if (c < 0 || c > HU_FONTSIZE)
        {
            width += 4;
            continue;
        }

        w = littleEndian(font.hu_font[c]->width);
        width += w;
    }

    // draw it
    int cx = 160 - width / 2;
    ch = text;
    while (ch)
    {
        c = *ch++;
        if (!c)
            break;
        c = doom_toupper(c) - HU_FONTSTART;
        if (c < 0 || c > HU_FONTSIZE)
        {
            cx += 4;
            continue;
        }

        w = littleEndian(font.hu_font[c]->width);
        drawPatch(cx, 180, 0, font.hu_font[c]);
        cx += w;
    }
}

//
// castDrawer
//
void castDrawer()
{
    auto& fin = finaleState();

    // erase the entire screen to a background
    drawPatch(0, 0, 0, static_cast<Patch*>(cacheLumpName("BOSSBACK")));

    castPrint(castorder[fin.castnum].name);

    // draw the current frame in the middle of the screen
    SpriteDef* sprdef = &sprites[fin.caststate->sprite];
    SpriteFrame* sprframe =
        &sprdef->spriteframes[fin.caststate->frame & FF_FRAMEMASK];
    int lump = sprframe->lump[0];
    bool flip = static_cast<bool>(sprframe->flip[0]);

    Patch* patch =
        static_cast<Patch*>(cacheLumpNum(lump + graphicsData().firstspritelump));
    if (flip)
        drawPatchFlipped(160, 170, 0, patch);
    else
        drawPatch(160, 170, 0, patch);
}

//
// drawPatchCol
//
void drawPatchCol(int x, Patch* patch, int col)
{
    Column* column = reinterpret_cast<Column*>(
        reinterpret_cast<byte*>(patch) + littleEndian(patch->columnofs[col]));
    byte* desttop = screens[0] + x;

    // step through the posts in a column
    while (column->topdelta != 0xff)
    {
        byte* source = reinterpret_cast<byte*>(column) + 3;
        byte* dest = desttop + column->topdelta * SCREENWIDTH;
        int count = column->length;

        while (count--)
        {
            *dest = *source++;
            dest += SCREENWIDTH;
        }
        column = reinterpret_cast<Column*>(reinterpret_cast<byte*>(column)
                                           + column->length + 4);
    }
}

//
// bunnyScroll
//
void bunnyScroll()
{
    auto& fin = finaleState();

    EA::Array<char, 10> name;

    Patch* p1 = static_cast<Patch*>(cacheLumpName("PFUB2"));
    Patch* p2 = static_cast<Patch*>(cacheLumpName("PFUB1"));

    markRect(0, 0, SCREENWIDTH, SCREENHEIGHT);

    int scrolled = 320 - (fin.finalecount - 230) / 2;
    if (scrolled > 320)
        scrolled = 320;
    if (scrolled < 0)
        scrolled = 0;

    for (int x = 0; x < SCREENWIDTH; x++)
    {
        if (x + scrolled < 320)
            drawPatchCol(x, p1, x + scrolled);
        else
            drawPatchCol(x, p2, x + scrolled - 320);
    }

    if (fin.finalecount < 1130)
        return;
    if (fin.finalecount < 1180)
    {
        drawPatch((SCREENWIDTH - 13 * 8) / 2,
                  (SCREENHEIGHT - 8 * 8) / 2,
                  0,
                  static_cast<Patch*>(cacheLumpName("END0")));
        fin.laststage = 0;
        return;
    }

    int stage = (fin.finalecount - 1180) / 5;
    if (stage > 6)
        stage = 6;
    if (stage > fin.laststage)
    {
        startSound(0, sfx_pistol);
        fin.laststage = stage;
    }

    //doom_sprintf(name, "END%i", stage);
    doom_strcpy(name.data(), "END");
    doom_concat(name.data(), doom_itoa(stage, 10));
    drawPatch((SCREENWIDTH - 13 * 8) / 2,
              (SCREENHEIGHT - 8 * 8) / 2,
              0,
              static_cast<Patch*>(cacheLumpName(name.data())));
}

//
// drawFinale
//
void drawFinale()
{
    auto& fin = finaleState();

    if (fin.finalestage == 2)
    {
        castDrawer();
        return;
    }

    if (!fin.finalestage)
        textWrite();
    else
    {
        switch (gameSession().gameepisode)
        {
            case 1:
                if (gameVersion().gamemode == retail)
                    drawPatch(0, 0, 0, static_cast<Patch*>(cacheLumpName("CREDIT")));
                else
                    drawPatch(0, 0, 0, static_cast<Patch*>(cacheLumpName("HELP2")));
                break;
            case 2:
                drawPatch(0, 0, 0, static_cast<Patch*>(cacheLumpName("VICTORY2")));
                break;
            case 3:
                bunnyScroll();
                break;
            case 4:
                drawPatch(0, 0, 0, static_cast<Patch*>(cacheLumpName("ENDPIC")));
                break;
        }
    }
}

} // namespace Doom
