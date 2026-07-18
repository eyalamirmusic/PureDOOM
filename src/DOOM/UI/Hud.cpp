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

#include "../doom_config.h"

#include "../doomdef.h"
#include "../doomstat.h"
#include "../dstrings.h" // Data.
#include "../hu_lib.h"
#include "../hu_stuff.h"
#include "../m_swap.h"
#include "../s_sound.h"
#include "../sounds.h"
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
extern char* chat_macros[];
extern char* player_names[];
extern char* mapnames[];

//
// Locally used constants, shortcuts.
//
#define HU_TITLE                                                                    \
    (mapnames[(gameSession().gameepisode - 1) * 9 + gameSession().gamemap - 1])
#define HU_TITLE2 (mapnames2[gameSession().gamemap - 1])
#define HU_TITLEP (mapnamesp[gameSession().gamemap - 1])
#define HU_TITLET (mapnamest[gameSession().gamemap - 1])
#define HU_TITLEHEIGHT 1
#define HU_TITLEX 0
#define HU_TITLEY (167 - SHORT(Doom::hudFont().hu_font[0]->height))
#define HU_INPUTTOGGLE 't'
#define HU_INPUTX HU_MSGX
#define HU_INPUTY                                                                   \
    (HU_MSGY + HU_MSGHEIGHT * (SHORT(Doom::hudFont().hu_font[0]->height) + 1))
#define HU_INPUTWIDTH 64
#define HU_INPUTHEIGHT 1
#define QUEUESIZE 128

namespace Doom
{

// The HUD's residual state (the player, the level-title line, the active flag) is a Doom::HudState
// owned by the Engine now, moved by the file-scope-statics sweep; these names are references onto
// the members (headsupactive follows below, at its own site) (REFACTOR.md, Step 5).
static Player*& plr = hudState().plr;
static HudTextLine& w_title = hudState().w_title;
// The heads-up chat state is a Doom::HudChat owned by the Engine now, moved by the
// file-scope-statics sweep; these names are references onto the members (REFACTOR.md, Step 5).
static HudInputText& w_chat = hudChat().w_chat;
static doom_boolean& always_off = hudChat().always_off;
static char (&chat_dest)[MAXPLAYERS] = hudChat().chat_dest;
static HudInputText (&w_inputbuffer)[MAXPLAYERS] = hudChat().w_inputbuffer;
// The HUD message line is a Doom::HudMessage owned by the Engine now, moved by the
// file-scope-statics sweep; these names are references onto the members (REFACTOR.md, Step 5).
static doom_boolean& message_on = hudMessage().message_on;
static doom_boolean& message_nottobefuckedwith =
    hudMessage().message_nottobefuckedwith;
static HudScrollingText& w_message = hudMessage().w_message;
static int& message_counter = hudMessage().message_counter;
static doom_boolean& headsupactive = hudState().headsupactive;
static char (&chatchars)[QUEUESIZE] =
    hudChat().chatchars; // outgoing local keystrokes
static int& head = hudChat().head;
static int& tail = hudChat().tail;

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
    {HUSTR_1,  HUSTR_2,  HUSTR_3,  HUSTR_4,  HUSTR_5,  HUSTR_6,
     HUSTR_7,  HUSTR_8,  HUSTR_9,  HUSTR_10, HUSTR_11,

     HUSTR_12, HUSTR_13, HUSTR_14, HUSTR_15, HUSTR_16, HUSTR_17,
     HUSTR_18, HUSTR_19, HUSTR_20,

     HUSTR_21, HUSTR_22, HUSTR_23, HUSTR_24, HUSTR_25, HUSTR_26,
     HUSTR_27, HUSTR_28, HUSTR_29, HUSTR_30, HUSTR_31, HUSTR_32};

EA::Array<const char*, 32> mapnamesp = // Plutonia WAD map names.
    {PHUSTR_1,  PHUSTR_2,  PHUSTR_3,  PHUSTR_4,  PHUSTR_5,  PHUSTR_6,
     PHUSTR_7,  PHUSTR_8,  PHUSTR_9,  PHUSTR_10, PHUSTR_11,

     PHUSTR_12, PHUSTR_13, PHUSTR_14, PHUSTR_15, PHUSTR_16, PHUSTR_17,
     PHUSTR_18, PHUSTR_19, PHUSTR_20,

     PHUSTR_21, PHUSTR_22, PHUSTR_23, PHUSTR_24, PHUSTR_25, PHUSTR_26,
     PHUSTR_27, PHUSTR_28, PHUSTR_29, PHUSTR_30, PHUSTR_31, PHUSTR_32};

EA::Array<const char*, 32> mapnamest = // TNT WAD map names.
    {THUSTR_1,  THUSTR_2,  THUSTR_3,  THUSTR_4,  THUSTR_5,  THUSTR_6,
     THUSTR_7,  THUSTR_8,  THUSTR_9,  THUSTR_10, THUSTR_11,

     THUSTR_12, THUSTR_13, THUSTR_14, THUSTR_15, THUSTR_16, THUSTR_17,
     THUSTR_18, THUSTR_19, THUSTR_20,

     THUSTR_21, THUSTR_22, THUSTR_23, THUSTR_24, THUSTR_25, THUSTR_26,
     THUSTR_27, THUSTR_28, THUSTR_29, THUSTR_30, THUSTR_31, THUSTR_32};

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
    headsupactive = false;
}

void startHud()
{
    auto& font = hudFont();
    auto& hud = hudFlags();

    const char* s;

    if (headsupactive)
        stopHud();

    auto& players_ = playerState();

    plr = &players_.players[players_.consoleplayer];
    message_on = false;
    hud.message_dontfuckwithme = false;
    message_nottobefuckedwith = false;
    hud.chat_on = false;

    // create the message widget
    Doom::initSText(w_message,
                    HU_MSGX,
                    HU_MSGY,
                    HU_MSGHEIGHT,
                    font.hu_font,
                    HU_FONTSTART,
                    &message_on);

    // create the map title widget
    Doom::initTextLine(w_title, HU_TITLEX, HU_TITLEY, font.hu_font, HU_FONTSTART);

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
        Doom::addCharToTextLine(w_title, *(s++));

    // create the chat widget
    Doom::initIText(
        w_chat, HU_INPUTX, HU_INPUTY, font.hu_font, HU_FONTSTART, &hud.chat_on);

    // create the inputbuffer widgets
    for (int i = 0; i < MAXPLAYERS; i++)
        Doom::initIText(w_inputbuffer[i], 0, 0, 0, 0, &always_off);

    headsupactive = true;
}

void drawHud()
{
    Doom::drawSText(w_message);
    Doom::drawIText(w_chat);
    if (overlayState().automapactive)
        Doom::drawTextLine(w_title, false);
}

void eraseHud()
{
    Doom::eraseSText(w_message);
    Doom::eraseIText(w_chat);
    Doom::eraseTextLine(w_title);
}

void hudTicker()
{
    auto& hud = hudFlags();

    int rc;
    char c;

    // tick down message counter if message is up
    if (message_counter && !--message_counter)
    {
        message_on = false;
        message_nottobefuckedwith = false;
    }

    if (menuSettings().showMessages || hud.message_dontfuckwithme)
    {
        // display message if necessary
        if ((plr->message && !message_nottobefuckedwith)
            || (plr->message && hud.message_dontfuckwithme))
        {
            Doom::addMessageToSText(w_message, 0, plr->message);
            plr->message = nullptr;
            message_on = true;
            message_counter = HU_MSGTIMEOUT;
            message_nottobefuckedwith = hud.message_dontfuckwithme;
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
                    chat_dest[i] = c;
                else
                {
                    if (c >= 'a' && c <= 'z')
                        c = static_cast<char>(
                            shiftxform[static_cast<unsigned char>(c)]);
                    rc = Doom::keyInIText(w_inputbuffer[i], c);
                    if (rc && c == KEY_ENTER)
                    {
                        if (w_inputbuffer[i].l.len
                            && (chat_dest[i] == players_.consoleplayer + 1
                                || chat_dest[i] == HU_BROADCAST))
                        {
                            Doom::addMessageToSText(
                                w_message, player_names[i], w_inputbuffer[i].l.l);

                            message_nottobefuckedwith = true;
                            message_on = true;
                            message_counter = HU_MSGTIMEOUT;
                            if (gameVersion().gamemode == commercial)
                                Doom::startSound(0, sfx_radio);
                            else
                                Doom::startSound(0, sfx_tink);
                        }
                        Doom::resetIText(w_inputbuffer[i]);
                    }
                }
                players_.players[i].cmd.chatchar = 0;
            }
        }
    }
}

void queueChatChar(char c)
{
    if (((head + 1) & (QUEUESIZE - 1)) == tail)
    {
        plr->message = HUSTR_MSGU;
    }
    else
    {
        chatchars[head] = c;
        head = (head + 1) & (QUEUESIZE - 1);
    }
}

char dequeueChatChar()
{
    char c;

    if (head != tail)
    {
        c = chatchars[tail];
        tail = (tail + 1) & (QUEUESIZE - 1);
    }
    else
    {
        c = 0;
    }

    return c;
}

doom_boolean hudResponder(Event* ev)
{
    auto& hud = hudFlags();

    char (&lastmessage)[HU_MAXLINELENGTH + 1] =
        hudChat().lastmessage; // ref-to-array onto member
    char* macromessage;
    doom_boolean eatkey = false;
    doom_boolean& shiftdown = hudChat().shiftdown;
    doom_boolean& altdown = hudChat().altdown;
    unsigned char c;
    int numplayers;

    static EA::Array<char, MAXPLAYERS> destination_keys = {
        HUSTR_KEYGREEN, HUSTR_KEYINDIGO, HUSTR_KEYBROWN, HUSTR_KEYRED};

    int& num_nobrainers = hudChat().num_nobrainers;

    auto& players_ = playerState();

    numplayers = 0;
    for (int i = 0; i < MAXPLAYERS; i++)
        numplayers += players_.playeringame[i];

    if (ev->data1 == KEY_RSHIFT)
    {
        shiftdown = ev->type == ev_keydown;
        return false;
    }
    else if (ev->data1 == KEY_RALT || ev->data1 == KEY_LALT)
    {
        altdown = ev->type == ev_keydown;
        return false;
    }

    if (ev->type != ev_keydown)
        return false;

    if (!hud.chat_on)
    {
        if (ev->data1 == HU_MSGREFRESH)
        {
            message_on = true;
            message_counter = HU_MSGTIMEOUT;
            eatkey = true;
        }
        else if (gameSession().netgame && ev->data1 == HU_INPUTTOGGLE)
        {
            eatkey = hud.chat_on = true;
            Doom::resetIText(w_chat);
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
                        Doom::resetIText(w_chat);
                        queueChatChar(i + 1);
                        break;
                    }
                    else if (i == players_.consoleplayer)
                    {
                        num_nobrainers++;
                        if (num_nobrainers < 3)
                            plr->message = HUSTR_TALKTOSELF1;
                        else if (num_nobrainers < 6)
                            plr->message = HUSTR_TALKTOSELF2;
                        else if (num_nobrainers < 9)
                            plr->message = HUSTR_TALKTOSELF3;
                        else if (num_nobrainers < 32)
                            plr->message = HUSTR_TALKTOSELF4;
                        else
                            plr->message = HUSTR_TALKTOSELF5;
                    }
                }
            }
        }
    }
    else
    {
        c = ev->data1;
        // send a macro
        if (altdown)
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
            doom_strcpy(lastmessage, chat_macros[c]);
            plr->message = lastmessage;
            eatkey = true;
        }
        else
        {
            if (french)
                c = foreignTranslation(c);
            if (shiftdown || (c >= 'a' && c <= 'z'))
                c = shiftxform[c];
            eatkey = Doom::keyInIText(w_chat, c);
            if (eatkey)
            {
                queueChatChar(c);
            }
            if (c == KEY_ENTER)
            {
                hud.chat_on = false;
                if (w_chat.l.len)
                {
                    doom_strcpy(lastmessage, w_chat.l.l);
                    plr->message = lastmessage;
                }
            }
            else if (c == KEY_ESCAPE)
                hud.chat_on = false;
        }
    }

    return eatkey;
}

} // namespace Doom
