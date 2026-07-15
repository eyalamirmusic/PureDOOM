// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Heads-up displays. Rewritten in UI/Hud.{h,cpp}; this keeps the HU_ names
//        as shims and owns the globals other files read: the HUD font, the chat
//        macros (persisted in the config), the player names, the level-name
//        tables, and the two message flags.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "doomdef.h"
#include "dstrings.h" // Data.
#include "hu_stuff.h"
#include "r_defs.h" // patch_t

#include "UI/Hud.h"

// The heads-up font (read by f_finale, m_menu, m_misc).
patch_t* hu_font[HU_FONTSIZE];

// Chat state read elsewhere: whether chat input is open (m_menu) and whether a
// message must not be suppressed (m_menu).
doom_boolean chat_on;
doom_boolean message_dontfuckwithme;

// The chat macros (m_misc persists them in the config).
char* chat_macros[] = {
    HUSTR_CHATMACRO0, HUSTR_CHATMACRO1, HUSTR_CHATMACRO2, HUSTR_CHATMACRO3,
    HUSTR_CHATMACRO4, HUSTR_CHATMACRO5, HUSTR_CHATMACRO6, HUSTR_CHATMACRO7,
    HUSTR_CHATMACRO8, HUSTR_CHATMACRO9};

// The player colour names (g_game uses them for obituary messages).
char* player_names[] = {
    HUSTR_PLRGREEN, HUSTR_PLRINDIGO, HUSTR_PLRBROWN, HUSTR_PLRRED};

//
// Builtin map names. The actual names can be found in dstrings.h. st_stuff reads
// mapnames for the deathmatch/coop level title.
//
char* mapnames[] = // DOOM shareware/registered/retail (Ultimate) names.
    {HUSTR_E1M1, HUSTR_E1M2, HUSTR_E1M3, HUSTR_E1M4, HUSTR_E1M5, HUSTR_E1M6,
     HUSTR_E1M7, HUSTR_E1M8, HUSTR_E1M9,

     HUSTR_E2M1, HUSTR_E2M2, HUSTR_E2M3, HUSTR_E2M4, HUSTR_E2M5, HUSTR_E2M6,
     HUSTR_E2M7, HUSTR_E2M8, HUSTR_E2M9,

     HUSTR_E3M1, HUSTR_E3M2, HUSTR_E3M3, HUSTR_E3M4, HUSTR_E3M5, HUSTR_E3M6,
     HUSTR_E3M7, HUSTR_E3M8, HUSTR_E3M9,

     HUSTR_E4M1, HUSTR_E4M2, HUSTR_E4M3, HUSTR_E4M4, HUSTR_E4M5, HUSTR_E4M6,
     HUSTR_E4M7, HUSTR_E4M8, HUSTR_E4M9,

     "NEWLEVEL", "NEWLEVEL", "NEWLEVEL", "NEWLEVEL", "NEWLEVEL", "NEWLEVEL",
     "NEWLEVEL", "NEWLEVEL", "NEWLEVEL"};


void HU_Init(void)
{
    Doom::huInit();
}

void HU_Start(void)
{
    Doom::huStart();
}

doom_boolean HU_Responder(event_t* ev)
{
    return Doom::huResponder(ev);
}

void HU_Ticker(void)
{
    Doom::huTicker();
}

void HU_Drawer(void)
{
    Doom::huDrawer();
}

char HU_dequeueChatChar(void)
{
    return Doom::huDequeueChatChar();
}

void HU_Erase(void)
{
    Doom::huErase();
}
