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
// and the DOOM II bunny scroll. f_finale.cpp shims the four F_ names. All the
// finale state is file-local here (nothing outside reads it); the two externs are
// other subsystems' globals. Not covered by any golden (no demo reaches a
// finale), so this is a faithful transcription.

#include "../doom_config.h"

#include "../doomstat.h"
#include "../dstrings.h" // Data.
#include "../hu_stuff.h"
#include "../i_system.h" // Functions.
#include "../m_swap.h" // Functions.
#include "../r_state.h"
#include "../s_sound.h" // Functions.
#include "../sounds.h" // Data.
#include "../v_video.h" // Functions.
#include "../w_wad.h" // Functions.

#include "Finale.h"
#include "FinaleState.h"

#include "../Render/Video.h"
#include <ea_data_structures/Structures/Array.h>

// Other subsystems' globals/functions this file reads.
extern gamestate_t& wipegamestate; // Doom::GameFlow (Engine member)
extern patch_t* (
    &hu_font)[HU_FONTSIZE]; // Doom::HudFont (Engine member); reference-to-array
void Doom::drawPatchFlipped(int x, int y, int scrn, patch_t* patch); // v_video

namespace Doom
{

#define TEXTSPEED 3
#define TEXTWAIT 250

struct castinfo_t
{
    const char* name;
    mobjtype_t type;
};

// The finale's runtime state now lives on the Engine (UI/FinaleState.h, moved by the
// file-scope-statics sweep - REFACTOR.md, Step 5). The vanilla names below are references onto that
// member, so every use is unchanged. The immutable reference data - the per-ending text pointers
// and the castorder[] cast list - stays file-local: it is fixed constants, not per-run state.

// Stage of animation:
//  0 = text, 1 = art screen, 2 = character cast
static int& finalestage = finaleState().finalestage;

static int& finalecount = finaleState().finalecount;

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

static const char*& finaletext = finaleState().finaletext;
static const char*& finaleflat = finaleState().finaleflat;

EA::Array<castinfo_t, 18> castorder = {{CC_ZOMBIE, MT_POSSESSED},
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

                                       {0, static_cast<mobjtype_t>(0)}};

static int& castnum = finaleState().castnum;
static int& casttics = finaleState().casttics;
static state_t*& caststate = finaleState().caststate;
static doom_boolean& castdeath = finaleState().castdeath;
static int& castframes = finaleState().castframes;
static int& castonmelee = finaleState().castonmelee;
static doom_boolean& castattacking = finaleState().castattacking;

//
// fStartCast
//

void fStartCast();
void fCastTicker();
doom_boolean fCastResponder(event_t* ev);
void fCastDrawer();

//
// startFinale
//
void startFinale()
{
    gameaction = ga_nothing;
    gamestate = GS_FINALE;
    viewactive = false;
    automapactive = false;

    // Okay - IWAD dependend stuff.
    // This has been changed severly, and
    //  some stuff might have changed in the process.
    switch (gamemode)
    {
        // DOOM 1 - E1, E3 or E4, but each nine missions
        case shareware:
        case registered:
        case retail:
        {
            S_ChangeMusic(mus_victor, true);

            switch (gameepisode)
            {
                case 1:
                    finaleflat = "FLOOR4_8";
                    finaletext = e1text;
                    break;
                case 2:
                    finaleflat = "SFLR6_1";
                    finaletext = e2text;
                    break;
                case 3:
                    finaleflat = "MFLR8_4";
                    finaletext = e3text;
                    break;
                case 4:
                    finaleflat = "MFLR8_3";
                    finaletext = e4text;
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
            S_ChangeMusic(mus_read_m, true);

            switch (gamemap)
            {
                case 6:
                    finaleflat = "SLIME16";
                    finaletext = c1text;
                    break;
                case 11:
                    finaleflat = "RROCK14";
                    finaletext = c2text;
                    break;
                case 20:
                    finaleflat = "RROCK07";
                    finaletext = c3text;
                    break;
                case 30:
                    finaleflat = "RROCK17";
                    finaletext = c4text;
                    break;
                case 15:
                    finaleflat = "RROCK13";
                    finaletext = c5text;
                    break;
                case 31:
                    finaleflat = "RROCK19";
                    finaletext = c6text;
                    break;
                default:
                    // Ouch.
                    break;
            }
            break;
        }

        // Indeterminate.
        default:
            S_ChangeMusic(mus_read_m, true);
            finaleflat = "F_SKY1"; // Not used anywhere else.
            finaletext = c1text; // FIXME - other text, music?
            break;
    }

    finalestage = 0;
    finalecount = 0;
}

doom_boolean finaleResponder(event_t* event)
{
    if (finalestage == 2)
        return fCastResponder(event);

    return false;
}

//
// finaleTicker
//
void finaleTicker()
{
    int i;

    // check for skipping
    if ((gamemode == commercial) && (finalecount > 50))
    {
        // go on to the next level
        for (i = 0; i < MAXPLAYERS; i++)
            if (players[i].cmd.buttons)
                break;

        if (i < MAXPLAYERS)
        {
            if (gamemap == 30)
                fStartCast();
            else
                gameaction = ga_worlddone;
        }
    }

    // advance animation
    finalecount++;

    if (finalestage == 2)
    {
        fCastTicker();
        return;
    }

    if (gamemode == commercial)
        return;

    if (!finalestage && finalecount > doom_strlen(finaletext) * TEXTSPEED + TEXTWAIT)
    {
        finalecount = 0;
        finalestage = 1;
        wipegamestate = static_cast<gamestate_t>(-1); // force a wipe
        if (gameepisode == 3)
            S_StartMusic(mus_bunny);
    }
}

//
// fTextWrite
//
void fTextWrite()
{
    byte* src;
    byte* dest;

    int w;
    int count;
    const char* ch;
    int c;
    int cx;
    int cy;

    // erase the entire screen to a tiled background
    src = static_cast<byte*>(W_CacheLumpName(finaleflat, PU_CACHE));
    dest = screens[0];

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

    Doom::markRect(0, 0, SCREENWIDTH, SCREENHEIGHT);

    // draw some of the text onto the screen
    cx = 10;
    cy = 10;
    ch = finaletext;

    count = (finalecount - 10) / TEXTSPEED;
    if (count < 0)
        count = 0;
    for (; count; count--)
    {
        c = *ch++;
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

        w = SHORT(hu_font[c]->width);
        if (cx + w > SCREENWIDTH)
            break;
        Doom::drawPatch(cx, cy, 0, hu_font[c]);
        cx += w;
    }
}

//
// Final DOOM 2 animation
// Casting by id Software.
//   in order of appearance
//
void fStartCast()
{
    wipegamestate = static_cast<gamestate_t>(-1); // force a screen wipe
    castnum = 0;
    caststate = &states[mobjinfo[castorder[castnum].type].seestate];
    casttics = caststate->tics;
    castdeath = false;
    finalestage = 2;
    castframes = 0;
    castonmelee = 0;
    castattacking = false;
    S_ChangeMusic(mus_evil, true);
}

//
// fCastTicker
//
void fCastTicker()
{
    int st;
    int sfx;

    if (--casttics > 0)
        return; // not time to change state yet

    if (caststate->tics == -1 || caststate->nextstate == S_NULL)
    {
        // switch from deathstate to next monster
        castnum++;
        castdeath = false;
        if (castorder[castnum].name == nullptr)
            castnum = 0;
        if (mobjinfo[castorder[castnum].type].seesound)
            S_StartSound(0, mobjinfo[castorder[castnum].type].seesound);
        caststate = &states[mobjinfo[castorder[castnum].type].seestate];
        castframes = 0;
    }
    else
    {
        // just advance to next state in animation
        if (caststate == &states[S_PLAY_ATK1])
            goto stopattack; // Oh, gross hack!
        st = caststate->nextstate;
        caststate = &states[st];
        castframes++;

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
            S_StartSound(0, sfx);
    }

    if (castframes == 12)
    {
        // go into attack frame
        castattacking = true;
        if (castonmelee)
            caststate = &states[mobjinfo[castorder[castnum].type].meleestate];
        else
            caststate = &states[mobjinfo[castorder[castnum].type].missilestate];
        castonmelee ^= 1;
        if (caststate == &states[S_NULL])
        {
            if (castonmelee)
                caststate = &states[mobjinfo[castorder[castnum].type].meleestate];
            else
                caststate = &states[mobjinfo[castorder[castnum].type].missilestate];
        }
    }

    if (castattacking)
    {
        if (castframes == 24
            || caststate == &states[mobjinfo[castorder[castnum].type].seestate])
        {
        stopattack:
            castattacking = false;
            castframes = 0;
            caststate = &states[mobjinfo[castorder[castnum].type].seestate];
        }
    }

    casttics = caststate->tics;
    if (casttics == -1)
        casttics = 15;
}

//
// fCastResponder
//
doom_boolean fCastResponder(event_t* ev)
{
    if (ev->type != ev_keydown)
        return false;

    if (castdeath)
        return true; // already in dying frames

    // go into death frame
    castdeath = true;
    caststate = &states[mobjinfo[castorder[castnum].type].deathstate];
    casttics = caststate->tics;
    castframes = 0;
    castattacking = false;
    if (mobjinfo[castorder[castnum].type].deathsound)
        S_StartSound(0, mobjinfo[castorder[castnum].type].deathsound);

    return true;
}

void fCastPrint(const char* text)
{
    const char* ch;
    int c;
    int cx;
    int w;
    int width;

    // find width
    ch = text;
    width = 0;

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

        w = SHORT(hu_font[c]->width);
        width += w;
    }

    // draw it
    cx = 160 - width / 2;
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

        w = SHORT(hu_font[c]->width);
        Doom::drawPatch(cx, 180, 0, hu_font[c]);
        cx += w;
    }
}

//
// fCastDrawer
//
void fCastDrawer()
{
    spritedef_t* sprdef;
    spriteframe_t* sprframe;
    int lump;
    doom_boolean flip;
    patch_t* patch;

    // erase the entire screen to a background
    Doom::drawPatch(
        0, 0, 0, static_cast<patch_t*>(W_CacheLumpName("BOSSBACK", PU_CACHE)));

    fCastPrint(castorder[castnum].name);

    // draw the current frame in the middle of the screen
    sprdef = &sprites[caststate->sprite];
    sprframe = &sprdef->spriteframes[caststate->frame & FF_FRAMEMASK];
    lump = sprframe->lump[0];
    flip = static_cast<doom_boolean>(sprframe->flip[0]);

    patch = static_cast<patch_t*>(W_CacheLumpNum(lump + firstspritelump, PU_CACHE));
    if (flip)
        Doom::drawPatchFlipped(160, 170, 0, patch);
    else
        Doom::drawPatch(160, 170, 0, patch);
}

//
// fDrawPatchCol
//
void fDrawPatchCol(int x, patch_t* patch, int col)
{
    column_t* column;
    byte* source;
    byte* dest;
    byte* desttop;
    int count;

    column = reinterpret_cast<column_t*>(reinterpret_cast<byte*>(patch)
                                         + LONG(patch->columnofs[col]));
    desttop = screens[0] + x;

    // step through the posts in a column
    while (column->topdelta != 0xff)
    {
        source = reinterpret_cast<byte*>(column) + 3;
        dest = desttop + column->topdelta * SCREENWIDTH;
        count = column->length;

        while (count--)
        {
            *dest = *source++;
            dest += SCREENWIDTH;
        }
        column = reinterpret_cast<column_t*>(reinterpret_cast<byte*>(column)
                                             + column->length + 4);
    }
}

//
// fBunnyScroll
//
void fBunnyScroll()
{
    int scrolled;
    patch_t* p1;
    patch_t* p2;
    EA::Array<char, 10> name;
    int stage;
    int& laststage = finaleState().laststage;

    p1 = static_cast<patch_t*>(W_CacheLumpName("PFUB2", PU_LEVEL));
    p2 = static_cast<patch_t*>(W_CacheLumpName("PFUB1", PU_LEVEL));

    Doom::markRect(0, 0, SCREENWIDTH, SCREENHEIGHT);

    scrolled = 320 - (finalecount - 230) / 2;
    if (scrolled > 320)
        scrolled = 320;
    if (scrolled < 0)
        scrolled = 0;

    for (int x = 0; x < SCREENWIDTH; x++)
    {
        if (x + scrolled < 320)
            fDrawPatchCol(x, p1, x + scrolled);
        else
            fDrawPatchCol(x, p2, x + scrolled - 320);
    }

    if (finalecount < 1130)
        return;
    if (finalecount < 1180)
    {
        Doom::drawPatch((SCREENWIDTH - 13 * 8) / 2,
                    (SCREENHEIGHT - 8 * 8) / 2,
                    0,
                    static_cast<patch_t*>(W_CacheLumpName("END0", PU_CACHE)));
        laststage = 0;
        return;
    }

    stage = (finalecount - 1180) / 5;
    if (stage > 6)
        stage = 6;
    if (stage > laststage)
    {
        S_StartSound(0, sfx_pistol);
        laststage = stage;
    }

    //doom_sprintf(name, "END%i", stage);
    doom_strcpy(name.data(), "END");
    doom_concat(name.data(), doom_itoa(stage, 10));
    Doom::drawPatch((SCREENWIDTH - 13 * 8) / 2,
                (SCREENHEIGHT - 8 * 8) / 2,
                0,
                static_cast<patch_t*>(W_CacheLumpName(name.data(), PU_CACHE)));
}

//
// drawFinale
//
void drawFinale()
{
    if (finalestage == 2)
    {
        fCastDrawer();
        return;
    }

    if (!finalestage)
        fTextWrite();
    else
    {
        switch (gameepisode)
        {
            case 1:
                if (gamemode == retail)
                    Doom::drawPatch(
                        0,
                        0,
                        0,
                        static_cast<patch_t*>(W_CacheLumpName("CREDIT", PU_CACHE)));
                else
                    Doom::drawPatch(
                        0,
                        0,
                        0,
                        static_cast<patch_t*>(W_CacheLumpName("HELP2", PU_CACHE)));
                break;
            case 2:
                Doom::drawPatch(
                    0,
                    0,
                    0,
                    static_cast<patch_t*>(W_CacheLumpName("VICTORY2", PU_CACHE)));
                break;
            case 3:
                fBunnyScroll();
                break;
            case 4:
                Doom::drawPatch(
                    0,
                    0,
                    0,
                    static_cast<patch_t*>(W_CacheLumpName("ENDPIC", PU_CACHE)));
                break;
        }
    }
}

} // namespace Doom
