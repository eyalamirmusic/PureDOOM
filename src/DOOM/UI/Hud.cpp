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
// DESCRIPTION:  Heads-up displays
//
//-----------------------------------------------------------------------------

// Rewritten out of vanilla hu_stuff into namespace Doom.
//
// The heads-up display: the message list, the chat input line, and the level
// title. hu_stuff.cpp shims the HU_ names and owns the globals other files read
// (the font, the chat macros, the player names, the level-name tables, the two
// message flags); everything else is file-local. Covered by the frame goldens
// (messages and the level name land in screens[0]).

#include "../Host/Platform.h"
#include "../Sim/MapTypes.h"
#include "../Render/RenderTypes.h"

#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h"
#include "../Game/Strings.h" // Data.
#include "HudWidgetTypes.h"
#include "Hud.h"
#include "../Math/Swap.h"
#include "../Game/SoundData.h"
#include "../Wad/WadFile.h"

#include <ea_data_structures/Structures/Array.h>

#include "../Game/GameSession.h"
#include "../Game/GameVersion.h"
#include "../Game/OverlayState.h"
#include "../Game/PlayerState.h"
#include "Hud.h"
#include "MenuSettings.h"
#include "HudChat.h"
#include "HudFlags.h"
#include "HudFont.h"
#include "HudMessage.h"
#include "HudState.h"
#include "HudWidgets.h"

#include "../Game/Sound.h"

// Globals owned by the hu_stuff.cpp shim (read by other files through their own
// externs): the config-persisted chat macros, the player names and the level-name
// tables (st_stuff reads mapnames).
extern EA::Array<const char*, 10> chat_macros;
extern EA::Array<const char*, 4> player_names;
extern EA::Array<const char*, 45> mapnames;

//
// Locally used constants, shortcuts.
//
// Every one of these stays a macro on purpose, for one of two reasons.
// HU_TITLE/2/P/T and HU_TITLEY/HU_INPUTY have bodies that call a runtime accessor
// (gameSession(), hudFont()), so no constexpr is available to them at all.
// HU_TITLEHEIGHT/HU_INPUTWIDTH/HU_INPUTHEIGHT are dead, and dead in 1993 too, and
// belong to the ~55 REFACTOR.md item 6 deliberately leaves alone.
#define HU_TITLE                                                                    \
    (mapnames[(gameSession().gameepisode - 1) * 9 + gameSession().gamemap - 1])
#define HU_TITLE2 (mapnames2[gameSession().gamemap - 1])
#define HU_TITLEP (mapnamesp[gameSession().gamemap - 1])
#define HU_TITLET (mapnamest[gameSession().gamemap - 1])
#define HU_TITLEHEIGHT 1
#define HU_TITLEY (167 - Doom::littleEndian(Doom::hudFont().hu_font[0]->height))
#define HU_INPUTY                                                                   \
    (HU_MSGY                                                                        \
     + HU_MSGHEIGHT * (Doom::littleEndian(Doom::hudFont().hu_font[0]->height) + 1))
#define HU_INPUTWIDTH 64
#define HU_INPUTHEIGHT 1

namespace Doom
{

constexpr int HU_TITLEX = 0;
constexpr char HU_INPUTTOGGLE = 't';
constexpr int HU_INPUTX = HU_MSGX;
constexpr int QUEUESIZE = 128;

// The HUD's residual state (the player, the level-title line, the active flag) is a Doom::HudState
// owned by the Engine (HudState.h); the heads-up chat state is a Doom::HudChat (HudChat.h); the
// message line is a Doom::HudMessage (HudMessage.h). All three used to be reached through
// file-scope `static T& x = cluster().x;` reference aliases (moved in by the file-scope-statics
// sweep, REFACTOR.md Step 5); the file-local-alias sweep (REFACTOR.md, Step 9 strand (a)) retired
// them - every function below reaches its cluster(s) through a hoisted local instead.

const char* shiftxform;

const EA::Array<char, 128> french_shiftxform = {
    0,    1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,
    16,   17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,
    ' ',  '!', '"', '#', '$', '%', '&',
    '"', // shift-'
    '(',  ')', '*', '+',
    '?', // shift-,
    '_', // shift--
    '>', // shift-.
    '?', // shift-/
    '0', // shift-0
    '1', // shift-1
    '2', // shift-2
    '3', // shift-3
    '4', // shift-4
    '5', // shift-5
    '6', // shift-6
    '7', // shift-7
    '8', // shift-8
    '9', // shift-9
    '/',
    '.', // shift-;
    '<',
    '+', // shift-=
    '>',  '?', '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N',  'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '[', // shift-[
    '!', // shift-backslash - OH MY GOD DOES WATCOM SUCK
    ']', // shift-]
    '"',  '_',
    '\'', // shift-`
    'A',  'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q',  'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '{', '|', '}', '~', 127

};

const EA::Array<char, 128> english_shiftxform = {
    0,    1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,
    16,   17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,
    ' ',  '!', '"', '#', '$', '%', '&',
    '"', // shift-'
    '(',  ')', '*', '+',
    '<', // shift-,
    '_', // shift--
    '>', // shift-.
    '?', // shift-/
    ')', // shift-0
    '!', // shift-1
    '@', // shift-2
    '#', // shift-3
    '$', // shift-4
    '%', // shift-5
    '^', // shift-6
    '&', // shift-7
    '*', // shift-8
    '(', // shift-9
    ':',
    ':', // shift-;
    '<',
    '+', // shift-=
    '>',  '?', '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N',  'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '[', // shift-[
    '!', // shift-backslash - OH MY GOD DOES WATCOM SUCK
    ']', // shift-]
    '"',  '_',
    '\'', // shift-`
    'A',  'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q',  'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '{', '|', '}', '~', 127};

EA::Array<char, 128> frenchKeyMap = {
    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,   13,  14,  15,
    16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,   29,  30,  31,
    ' ', '!', '"', '#', '$', '%', '&', '%', '(', ')', '*', '+', ';',  '-', ':', '!',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', 'M', '<',  '=', '>', '?',
    '@', 'Q', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',  ',', 'N', 'O',
    'P', 'A', 'R', 'S', 'T', 'U', 'V', 'Z', 'X', 'Y', 'W', '^', '\\', '$', '^', '_',
    '@', 'Q', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',  ',', 'N', 'O',
    'P', 'A', 'R', 'S', 'T', 'U', 'V', 'Z', 'X', 'Y', 'W', '^', '\\', '$', '^', 127};

//
// Builtin map names.
// The actual names can be found in DStrings.h.
//

EA::Array<const char*, 32> mapnames2 = // DOOM 2 map names.
    {Doom::HUSTR_1,  Doom::HUSTR_2,  Doom::HUSTR_3,  Doom::HUSTR_4,  Doom::HUSTR_5,
     Doom::HUSTR_6,  Doom::HUSTR_7,  Doom::HUSTR_8,  Doom::HUSTR_9,  Doom::HUSTR_10,
     Doom::HUSTR_11,

     Doom::HUSTR_12, Doom::HUSTR_13, Doom::HUSTR_14, Doom::HUSTR_15, Doom::HUSTR_16,
     Doom::HUSTR_17, Doom::HUSTR_18, Doom::HUSTR_19, Doom::HUSTR_20,

     Doom::HUSTR_21, Doom::HUSTR_22, Doom::HUSTR_23, Doom::HUSTR_24, Doom::HUSTR_25,
     Doom::HUSTR_26, Doom::HUSTR_27, Doom::HUSTR_28, Doom::HUSTR_29, Doom::HUSTR_30,
     Doom::HUSTR_31, Doom::HUSTR_32};

EA::Array<const char*, 32> mapnamesp = // Plutonia WAD map names.
    {Doom::PHUSTR_1,  Doom::PHUSTR_2,  Doom::PHUSTR_3,  Doom::PHUSTR_4,
     Doom::PHUSTR_5,  Doom::PHUSTR_6,  Doom::PHUSTR_7,  Doom::PHUSTR_8,
     Doom::PHUSTR_9,  Doom::PHUSTR_10, Doom::PHUSTR_11,

     Doom::PHUSTR_12, Doom::PHUSTR_13, Doom::PHUSTR_14, Doom::PHUSTR_15,
     Doom::PHUSTR_16, Doom::PHUSTR_17, Doom::PHUSTR_18, Doom::PHUSTR_19,
     Doom::PHUSTR_20,

     Doom::PHUSTR_21, Doom::PHUSTR_22, Doom::PHUSTR_23, Doom::PHUSTR_24,
     Doom::PHUSTR_25, Doom::PHUSTR_26, Doom::PHUSTR_27, Doom::PHUSTR_28,
     Doom::PHUSTR_29, Doom::PHUSTR_30, Doom::PHUSTR_31, Doom::PHUSTR_32};

EA::Array<const char*, 32> mapnamest = // TNT WAD map names.
    {Doom::THUSTR_1,  Doom::THUSTR_2,  Doom::THUSTR_3,  Doom::THUSTR_4,
     Doom::THUSTR_5,  Doom::THUSTR_6,  Doom::THUSTR_7,  Doom::THUSTR_8,
     Doom::THUSTR_9,  Doom::THUSTR_10, Doom::THUSTR_11,

     Doom::THUSTR_12, Doom::THUSTR_13, Doom::THUSTR_14, Doom::THUSTR_15,
     Doom::THUSTR_16, Doom::THUSTR_17, Doom::THUSTR_18, Doom::THUSTR_19,
     Doom::THUSTR_20,

     Doom::THUSTR_21, Doom::THUSTR_22, Doom::THUSTR_23, Doom::THUSTR_24,
     Doom::THUSTR_25, Doom::THUSTR_26, Doom::THUSTR_27, Doom::THUSTR_28,
     Doom::THUSTR_29, Doom::THUSTR_30, Doom::THUSTR_31, Doom::THUSTR_32};

char foreignTranslation(unsigned char ch)
{
    return ch < 128 ? frenchKeyMap[ch] : ch;
}

void initHud()
{
    auto& font = hudFont();

    int j;
    EA::Array<char, 9> buffer;

    if (french)
        shiftxform = french_shiftxform.data();
    else
        shiftxform = english_shiftxform.data();

    // load the heads-up font
    j = HU_FONTSTART;
    for (int i = 0; i < HU_FONTSIZE; i++)
    {
        //if (j == 40) __debugbreak();
        //doom_sprintf(buffer, "STCFN%.3d", j++);
        doom_strcpy(buffer.data(), "STCFN");
        if (j < 100)
            doom_concat(buffer.data(), "0");
        if (j < 10)
            doom_concat(buffer.data(), "0");
        doom_concat(buffer.data(), doom_itoa(j++, 10));
        font.hu_font[i] = static_cast<Patch*>(Doom::cacheLumpName(buffer.data()));
    }
}

void stopHud()
{
    hudState().headsupactive = false;
}

void startHud()
{
    auto& font = hudFont();
    auto& hud = hudFlags();
    auto& state = hudState();
    auto& chat = hudChat();
    auto& msg = hudMessage();

    const char* s;

    if (state.headsupactive)
        stopHud();

    auto& players_ = playerState();

    state.plr = &players_.players[players_.consoleplayer];
    msg.message_on = false;
    hud.message_dontfuckwithme = false;
    msg.message_nottobefuckedwith = false;
    hud.chat_on = false;

    // create the message widget
    Doom::initSText(msg.w_message,
                    HU_MSGX,
                    HU_MSGY,
                    HU_MSGHEIGHT,
                    font.hu_font.data(),
                    HU_FONTSTART,
                    &msg.message_on);

    // create the map title widget
    Doom::initTextLine(
        state.w_title, HU_TITLEX, HU_TITLEY, font.hu_font.data(), HU_FONTSTART);

    switch (gameVersion().gamemode)
    {
        case shareware:
        case registered:
        case retail:
            s = HU_TITLE;
            break;

            /* FIXME
                  case pack_plut:
                    s = HU_TITLEP;
                    break;
                  case pack_tnt:
                    s = HU_TITLET;
                    break;
            */

        case commercial:
        default:
            s = HU_TITLE2;
            break;
    }

    while (*s)
        Doom::addCharToTextLine(state.w_title, *(s++));

    // create the chat widget
    Doom::initIText(chat.w_chat,
                    HU_INPUTX,
                    HU_INPUTY,
                    font.hu_font.data(),
                    HU_FONTSTART,
                    &hud.chat_on);

    // create the inputbuffer widgets
    for (int i = 0; i < MAXPLAYERS; i++)
        Doom::initIText(chat.w_inputbuffer[i], 0, 0, 0, 0, &chat.always_off);

    state.headsupactive = true;
}

void drawHud()
{
    Doom::drawSText(hudMessage().w_message);
    Doom::drawIText(hudChat().w_chat);
    if (overlayState().automapactive)
        Doom::drawTextLine(hudState().w_title, false);
}

void eraseHud()
{
    Doom::eraseSText(hudMessage().w_message);
    Doom::eraseIText(hudChat().w_chat);
    Doom::eraseTextLine(hudState().w_title);
}

void hudTicker()
{
    auto& hud = hudFlags();
    auto& msg = hudMessage();
    auto& chat = hudChat();
    auto& plr = *hudState().plr;

    int rc;
    char c;

    // tick down message counter if message is up
    if (msg.message_counter && !--msg.message_counter)
    {
        msg.message_on = false;
        msg.message_nottobefuckedwith = false;
    }

    if (menuSettings().showMessages || hud.message_dontfuckwithme)
    {
        // display message if necessary
        if ((plr.message && !msg.message_nottobefuckedwith)
            || (plr.message && hud.message_dontfuckwithme))
        {
            Doom::addMessageToSText(msg.w_message, 0, plr.message);
            plr.message = nullptr;
            msg.message_on = true;
            msg.message_counter = HU_MSGTIMEOUT;
            msg.message_nottobefuckedwith = hud.message_dontfuckwithme;
            hud.message_dontfuckwithme = 0;
        }

    } // else message_on = false;

    auto& players_ = playerState();

    // check for incoming chat characters
    if (gameSession().netgame)
    {
        for (int i = 0; i < MAXPLAYERS; i++)
        {
            if (!players_.playeringame[i])
                continue;
            if (i != players_.consoleplayer
                && (c = players_.players[i].cmd.chatchar))
            {
                if (c <= HU_BROADCAST)
                    chat.chat_dest[i] = c;
                else
                {
                    if (c >= 'a' && c <= 'z')
                        c = static_cast<char>(
                            shiftxform[static_cast<unsigned char>(c)]);
                    rc = Doom::keyInIText(chat.w_inputbuffer[i], c);
                    if (rc && c == KEY_ENTER)
                    {
                        if (chat.w_inputbuffer[i].l.len
                            && (chat.chat_dest[i] == players_.consoleplayer + 1
                                || chat.chat_dest[i] == HU_BROADCAST))
                        {
                            Doom::addMessageToSText(
                                msg.w_message,
                                player_names[i],
                                chat.w_inputbuffer[i].l.l.data());

                            msg.message_nottobefuckedwith = true;
                            msg.message_on = true;
                            msg.message_counter = HU_MSGTIMEOUT;
                            if (gameVersion().gamemode == commercial)
                                Doom::startSound(0, sfx_radio);
                            else
                                Doom::startSound(0, sfx_tink);
                        }
                        Doom::resetIText(chat.w_inputbuffer[i]);
                    }
                }
                players_.players[i].cmd.chatchar = 0;
            }
        }
    }
}

void queueChatChar(char c)
{
    auto& chat = hudChat();

    if (((chat.head + 1) & (QUEUESIZE - 1)) == chat.tail)
    {
        hudState().plr->message = Doom::HUSTR_MSGU;
    }
    else
    {
        chat.chatchars[chat.head] = c;
        chat.head = (chat.head + 1) & (QUEUESIZE - 1);
    }
}

char dequeueChatChar()
{
    auto& chat = hudChat();

    char c;

    if (chat.head != chat.tail)
    {
        c = chat.chatchars[chat.tail];
        chat.tail = (chat.tail + 1) & (QUEUESIZE - 1);
    }
    else
    {
        c = 0;
    }

    return c;
}

bool hudResponder(Event* ev)
{
    auto& hud = hudFlags();
    auto& chat = hudChat();
    auto& msg = hudMessage();
    auto& state = hudState();

    const char* macromessage;
    bool eatkey = false;
    unsigned char c;
    int numplayers;

    static EA::Array<char, MAXPLAYERS> destination_keys = {Doom::HUSTR_KEYGREEN,
                                                           Doom::HUSTR_KEYINDIGO,
                                                           Doom::HUSTR_KEYBROWN,
                                                           Doom::HUSTR_KEYRED};

    auto& players_ = playerState();

    numplayers = 0;
    for (int i = 0; i < MAXPLAYERS; i++)
        numplayers += players_.playeringame[i];

    if (ev->data1 == KEY_RSHIFT)
    {
        chat.shiftdown = ev->type == ev_keydown;
        return false;
    }
    else if (ev->data1 == KEY_RALT || ev->data1 == KEY_LALT)
    {
        chat.altdown = ev->type == ev_keydown;
        return false;
    }

    if (ev->type != ev_keydown)
        return false;

    if (!hud.chat_on)
    {
        if (ev->data1 == HU_MSGREFRESH)
        {
            msg.message_on = true;
            msg.message_counter = HU_MSGTIMEOUT;
            eatkey = true;
        }
        else if (gameSession().netgame && ev->data1 == HU_INPUTTOGGLE)
        {
            eatkey = hud.chat_on = true;
            Doom::resetIText(chat.w_chat);
            queueChatChar(HU_BROADCAST);
        }
        else if (gameSession().netgame && numplayers > 2)
        {
            for (int i = 0; i < MAXPLAYERS; i++)
            {
                if (ev->data1 == destination_keys[i])
                {
                    if (players_.playeringame[i] && i != players_.consoleplayer)
                    {
                        eatkey = hud.chat_on = true;
                        Doom::resetIText(chat.w_chat);
                        queueChatChar(i + 1);
                        break;
                    }
                    else if (i == players_.consoleplayer)
                    {
                        chat.num_nobrainers++;
                        if (chat.num_nobrainers < 3)
                            state.plr->message = Doom::HUSTR_TALKTOSELF1;
                        else if (chat.num_nobrainers < 6)
                            state.plr->message = Doom::HUSTR_TALKTOSELF2;
                        else if (chat.num_nobrainers < 9)
                            state.plr->message = Doom::HUSTR_TALKTOSELF3;
                        else if (chat.num_nobrainers < 32)
                            state.plr->message = Doom::HUSTR_TALKTOSELF4;
                        else
                            state.plr->message = Doom::HUSTR_TALKTOSELF5;
                    }
                }
            }
        }
    }
    else
    {
        c = ev->data1;
        // send a macro
        if (chat.altdown)
        {
            c = c - '0';
            if (c > 9)
                return false;
            macromessage = chat_macros[c];

            // kill last message with a '\n'
            queueChatChar(KEY_ENTER); // DEBUG!!!

            // send the macro message
            while (*macromessage)
                queueChatChar(*macromessage++);
            queueChatChar(KEY_ENTER);

            // leave chat mode and notify that it was sent
            hud.chat_on = false;
            doom_strcpy(chat.lastmessage.data(), chat_macros[c]);
            state.plr->message = chat.lastmessage.data();
            eatkey = true;
        }
        else
        {
            if (french)
                c = foreignTranslation(c);
            if (chat.shiftdown || (c >= 'a' && c <= 'z'))
                c = shiftxform[c];
            eatkey = Doom::keyInIText(chat.w_chat, c);
            if (eatkey)
            {
                queueChatChar(c);
            }
            if (c == KEY_ENTER)
            {
                hud.chat_on = false;
                if (chat.w_chat.l.len)
                {
                    doom_strcpy(chat.lastmessage.data(), chat.w_chat.l.l.data());
                    state.plr->message = chat.lastmessage.data();
                }
            }
            else if (c == KEY_ESCAPE)
                hud.chat_on = false;
        }
    }

    return eatkey;
}

} // namespace Doom

// ---------------------------------------------------------------------------
// Global-scope data that was hu_stuff.cpp. It stays at :: scope because these are the
// vanilla names other translation units (and the eacp port) still link against.
// ---------------------------------------------------------------------------
// The heads-up font (read by f_finale, m_menu, m_misc). A Doom::HudFont owned by the Engine now;
// this is a reference onto its member. HU_Start writes the array, so every reader's extern is a
// reference-to-array too (a plain array extern would read the reference's hidden pointer as the
// first glyph and fault). The reference-to-array binding self-checks HU_FONTSIZE against the
// cluster's fontSize - a drift won't compile.

// The two cross-read HUD flags (chat input open; a forced message locks the line)
// are a Doom::HudFlags owned by the Engine now; these are references onto its members.

// The chat macros (m_misc persists them in the config).
EA::Array<const char*, 10> chat_macros = {Doom::HUSTR_CHATMACRO0,
                                          Doom::HUSTR_CHATMACRO1,
                                          Doom::HUSTR_CHATMACRO2,
                                          Doom::HUSTR_CHATMACRO3,
                                          Doom::HUSTR_CHATMACRO4,
                                          Doom::HUSTR_CHATMACRO5,
                                          Doom::HUSTR_CHATMACRO6,
                                          Doom::HUSTR_CHATMACRO7,
                                          Doom::HUSTR_CHATMACRO8,
                                          Doom::HUSTR_CHATMACRO9};

// The player colour names (g_game uses them for obituary messages).
EA::Array<const char*, 4> player_names = {Doom::HUSTR_PLRGREEN,
                                          Doom::HUSTR_PLRINDIGO,
                                          Doom::HUSTR_PLRBROWN,
                                          Doom::HUSTR_PLRRED};

//
// Builtin map names. The actual names can be found in dstrings.h. st_stuff reads
// mapnames for the deathmatch/coop level title.
//
EA::Array<const char*, 45>
    mapnames = // DOOM shareware/registered/retail (Ultimate) names.
    {Doom::HUSTR_E1M1, Doom::HUSTR_E1M2, Doom::HUSTR_E1M3,
     Doom::HUSTR_E1M4, Doom::HUSTR_E1M5, Doom::HUSTR_E1M6,
     Doom::HUSTR_E1M7, Doom::HUSTR_E1M8, Doom::HUSTR_E1M9,

     Doom::HUSTR_E2M1, Doom::HUSTR_E2M2, Doom::HUSTR_E2M3,
     Doom::HUSTR_E2M4, Doom::HUSTR_E2M5, Doom::HUSTR_E2M6,
     Doom::HUSTR_E2M7, Doom::HUSTR_E2M8, Doom::HUSTR_E2M9,

     Doom::HUSTR_E3M1, Doom::HUSTR_E3M2, Doom::HUSTR_E3M3,
     Doom::HUSTR_E3M4, Doom::HUSTR_E3M5, Doom::HUSTR_E3M6,
     Doom::HUSTR_E3M7, Doom::HUSTR_E3M8, Doom::HUSTR_E3M9,

     Doom::HUSTR_E4M1, Doom::HUSTR_E4M2, Doom::HUSTR_E4M3,
     Doom::HUSTR_E4M4, Doom::HUSTR_E4M5, Doom::HUSTR_E4M6,
     Doom::HUSTR_E4M7, Doom::HUSTR_E4M8, Doom::HUSTR_E4M9,

     "NEWLEVEL",       "NEWLEVEL",       "NEWLEVEL",
     "NEWLEVEL",       "NEWLEVEL",       "NEWLEVEL",
     "NEWLEVEL",       "NEWLEVEL",       "NEWLEVEL"};
