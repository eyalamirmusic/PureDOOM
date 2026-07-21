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
#include "../Host/Text.h"
#include "../Wad/WadFile.h"

#include "../Containers.h"

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

#include <algorithm>

// Globals owned by the hu_stuff.cpp shim (read by other files through their own
// externs): the config-persisted chat macros, the player names and the level-name
// tables (st_stuff reads mapnames).
extern Doom::Array<std::string_view, 10> chat_macros;
extern Doom::Array<std::string_view, 4> player_names;
extern Doom::Array<std::string_view, 45> mapnames;

//
// Locally used constants, shortcuts.
//
// The level-title and input-row positions that used to live here as macros are
// functions now (hudTitle/hudTitle2/hudTitleY/hudInputY, below the name tables
// they read) - their bodies call runtime accessors, so no constexpr was ever
// available to them, but nothing about them wanted the preprocessor either.
// HU_TITLEHEIGHT/HU_INPUTWIDTH/HU_INPUTHEIGHT stay macros: they are dead, and dead
// in 1993 too, and belong to the ~55 REFACTOR.md item 6 deliberately leaves alone.
#define HU_TITLEHEIGHT 1
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

const Array<char, 128> french_shiftxform = {
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

const Array<char, 128> english_shiftxform = {
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

Array<char, 128> frenchKeyMap = {
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

Array<std::string_view, 32> mapnames2 = // DOOM 2 map names.
    {HUSTR_1,  HUSTR_2,  HUSTR_3,  HUSTR_4,  HUSTR_5,  HUSTR_6,
     HUSTR_7,  HUSTR_8,  HUSTR_9,  HUSTR_10, HUSTR_11,

     HUSTR_12, HUSTR_13, HUSTR_14, HUSTR_15, HUSTR_16, HUSTR_17,
     HUSTR_18, HUSTR_19, HUSTR_20,

     HUSTR_21, HUSTR_22, HUSTR_23, HUSTR_24, HUSTR_25, HUSTR_26,
     HUSTR_27, HUSTR_28, HUSTR_29, HUSTR_30, HUSTR_31, HUSTR_32};

Array<std::string_view, 32> mapnamesp = // Plutonia WAD map names.
    {PHUSTR_1,  PHUSTR_2,  PHUSTR_3,  PHUSTR_4,  PHUSTR_5,  PHUSTR_6,
     PHUSTR_7,  PHUSTR_8,  PHUSTR_9,  PHUSTR_10, PHUSTR_11,

     PHUSTR_12, PHUSTR_13, PHUSTR_14, PHUSTR_15, PHUSTR_16, PHUSTR_17,
     PHUSTR_18, PHUSTR_19, PHUSTR_20,

     PHUSTR_21, PHUSTR_22, PHUSTR_23, PHUSTR_24, PHUSTR_25, PHUSTR_26,
     PHUSTR_27, PHUSTR_28, PHUSTR_29, PHUSTR_30, PHUSTR_31, PHUSTR_32};

Array<std::string_view, 32> mapnamest = // TNT WAD map names.
    {THUSTR_1,  THUSTR_2,  THUSTR_3,  THUSTR_4,  THUSTR_5,  THUSTR_6,
     THUSTR_7,  THUSTR_8,  THUSTR_9,  THUSTR_10, THUSTR_11,

     THUSTR_12, THUSTR_13, THUSTR_14, THUSTR_15, THUSTR_16, THUSTR_17,
     THUSTR_18, THUSTR_19, THUSTR_20,

     THUSTR_21, THUSTR_22, THUSTR_23, THUSTR_24, THUSTR_25, THUSTR_26,
     THUSTR_27, THUSTR_28, THUSTR_29, THUSTR_30, THUSTR_31, THUSTR_32};

// The level title for the current map, and the two positions derived from the HUD
// font's height. These were macros until Step 9's last pass - not for any reason
// of the preprocessor's, but because their bodies call accessors and so could not
// become constexpr. They sit here rather than beside the other constants because
// they read the name tables defined just above.
//
// The Plutonia and TNT variants (mapnamesp/mapnamest) have no function: their only
// callers are inside a commented-out FIXME in initHud, in both eras.
std::string_view hudTitle()
{
    const auto& session = gameSession();
    return mapnames[(session.gameepisode - 1) * 9 + session.gamemap - 1];
}

std::string_view hudTitle2()
{
    return mapnames2[gameSession().gamemap - 1];
}

int hudFontHeight()
{
    return littleEndian(hudFont().hu_font[0]->height);
}

int hudTitleY()
{
    return 167 - hudFontHeight();
}

int hudInputY()
{
    return HU_MSGY + HU_MSGHEIGHT * (hudFontHeight() + 1);
}

char foreignTranslation(unsigned char ch)
{
    return ch < 128 ? frenchKeyMap[ch] : ch;
}

void initHud()
{
    auto& font = hudFont();

    if (french)
        shiftxform = french_shiftxform.data();
    else
        shiftxform = english_shiftxform.data();

    // load the heads-up font
    int j = HU_FONTSTART;
    for (auto*& glyph: font.hu_font)
    {
        auto name = std::string {"STCFN"};

        if (j < 100)
            name += "0";

        if (j < 10)
            name += "0";

        name += std::to_string(j++);
        glyph = static_cast<Patch*>(cacheLumpName(name));
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

    std::string_view s;

    if (state.headsupactive)
        stopHud();

    auto& players_ = playerState();

    state.plr = &players_.players[players_.consoleplayer];
    msg.message_on = false;
    hud.message_dontfuckwithme = false;
    msg.message_nottobefuckedwith = false;
    hud.chat_on = false;

    // create the message widget
    initSText(msg.w_message,
              HU_MSGX,
              HU_MSGY,
              HU_MSGHEIGHT,
              font.hu_font.data(),
              HU_FONTSTART,
              &msg.message_on);

    // create the map title widget
    initTextLine(
        state.w_title, HU_TITLEX, hudTitleY(), font.hu_font.data(), HU_FONTSTART);

    switch (gameVersion().gamemode)
    {
        case shareware:
        case registered:
        case retail:
            s = hudTitle();
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
            s = hudTitle2();
            break;
    }

    for (auto character: s)
        addCharToTextLine(state.w_title, character);

    // create the chat widget
    initIText(chat.w_chat,
              HU_INPUTX,
              hudInputY(),
              font.hu_font.data(),
              HU_FONTSTART,
              &hud.chat_on);

    // create the inputbuffer widgets
    for (auto& buffer: chat.w_inputbuffer)
        initIText(buffer, 0, 0, nullptr, 0, &chat.always_off);

    state.headsupactive = true;
}

void drawHud()
{
    drawSText(hudMessage().w_message);
    drawIText(hudChat().w_chat);
    if (overlayState().automapactive)
        drawTextLine(hudState().w_title, false);
}

void eraseHud()
{
    eraseSText(hudMessage().w_message);
    eraseIText(hudChat().w_chat);
    eraseTextLine(hudState().w_title);
}

void hudTicker()
{
    auto& hud = hudFlags();
    auto& msg = hudMessage();
    auto& chat = hudChat();
    auto& plr = *hudState().plr;

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
        if ((!plr.message.empty() && !msg.message_nottobefuckedwith)
            || (!plr.message.empty() && hud.message_dontfuckwithme))
        {
            addMessageToSText(msg.w_message, "", plr.message);
            plr.message = {};
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
                    int rc = keyInIText(chat.w_inputbuffer[i], c);
                    if (rc && c == KEY_ENTER)
                    {
                        if (!chat.w_inputbuffer[i].l.l.empty()
                            && (chat.chat_dest[i] == players_.consoleplayer + 1
                                || chat.chat_dest[i] == HU_BROADCAST))
                        {
                            addMessageToSText(msg.w_message,
                                              player_names[i],
                                              chat.w_inputbuffer[i].l.l);

                            msg.message_nottobefuckedwith = true;
                            msg.message_on = true;
                            msg.message_counter = HU_MSGTIMEOUT;
                            if (gameVersion().gamemode == commercial)
                                startSound(nullptr, sfx_radio);
                            else
                                startSound(nullptr, sfx_tink);
                        }
                        resetIText(chat.w_inputbuffer[i]);
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
        hudState().plr->message = HUSTR_MSGU;
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

bool hudResponder(Event& ev)
{
    auto& hud = hudFlags();
    auto& chat = hudChat();
    auto& msg = hudMessage();
    auto& state = hudState();

    bool eatkey = false;

    static Array<char, MAXPLAYERS> destination_keys = {
        HUSTR_KEYGREEN, HUSTR_KEYINDIGO, HUSTR_KEYBROWN, HUSTR_KEYRED};

    auto& players_ = playerState();

    const auto numplayers = static_cast<int>(std::count(
        players_.playeringame.begin(), players_.playeringame.end(), true));

    if (ev.data1 == KEY_RSHIFT)
    {
        chat.shiftdown = ev.type == ev_keydown;
        return false;
    }
    else if (ev.data1 == KEY_RALT || ev.data1 == KEY_LALT)
    {
        chat.altdown = ev.type == ev_keydown;
        return false;
    }

    if (ev.type != ev_keydown)
        return false;

    if (!hud.chat_on)
    {
        if (ev.data1 == HU_MSGREFRESH)
        {
            msg.message_on = true;
            msg.message_counter = HU_MSGTIMEOUT;
            eatkey = true;
        }
        else if (gameSession().netgame && ev.data1 == HU_INPUTTOGGLE)
        {
            eatkey = hud.chat_on = true;
            resetIText(chat.w_chat);
            queueChatChar(HU_BROADCAST);
        }
        else if (gameSession().netgame && numplayers > 2)
        {
            for (int i = 0; i < MAXPLAYERS; i++)
            {
                if (ev.data1 == destination_keys[i])
                {
                    if (players_.playeringame[i] && i != players_.consoleplayer)
                    {
                        eatkey = hud.chat_on = true;
                        resetIText(chat.w_chat);
                        queueChatChar(i + 1);
                        break;
                    }
                    else if (i == players_.consoleplayer)
                    {
                        chat.num_nobrainers++;
                        if (chat.num_nobrainers < 3)
                            state.plr->message = HUSTR_TALKTOSELF1;
                        else if (chat.num_nobrainers < 6)
                            state.plr->message = HUSTR_TALKTOSELF2;
                        else if (chat.num_nobrainers < 9)
                            state.plr->message = HUSTR_TALKTOSELF3;
                        else if (chat.num_nobrainers < 32)
                            state.plr->message = HUSTR_TALKTOSELF4;
                        else
                            state.plr->message = HUSTR_TALKTOSELF5;
                    }
                }
            }
        }
    }
    else
    {
        unsigned char c = ev.data1;
        // send a macro
        if (chat.altdown)
        {
            c = c - '0';
            if (c > 9)
                return false;
            auto macromessage = chat_macros[c];

            // kill last message with a '\n'
            queueChatChar(KEY_ENTER); // DEBUG!!!

            // send the macro message
            for (auto character: macromessage)
                queueChatChar(character);
            queueChatChar(KEY_ENTER);

            // leave chat mode and notify that it was sent
            hud.chat_on = false;
            chat.lastmessage = chat_macros[c];
            state.plr->message = chat.lastmessage;
            eatkey = true;
        }
        else
        {
            if (french)
                c = foreignTranslation(c);
            if (chat.shiftdown || (c >= 'a' && c <= 'z'))
                c = shiftxform[c];
            eatkey = keyInIText(chat.w_chat, c);
            if (eatkey)
            {
                queueChatChar(c);
            }
            if (c == KEY_ENTER)
            {
                hud.chat_on = false;
                if (!chat.w_chat.l.l.empty())
                {
                    chat.lastmessage = chat.w_chat.l.l;
                    state.plr->message = chat.lastmessage;
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
Doom::Array<std::string_view, 10> chat_macros = {Doom::HUSTR_CHATMACRO0,
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
Doom::Array<std::string_view, 4> player_names = {Doom::HUSTR_PLRGREEN,
                                                 Doom::HUSTR_PLRINDIGO,
                                                 Doom::HUSTR_PLRBROWN,
                                                 Doom::HUSTR_PLRRED};

//
// Builtin map names. The actual names can be found in dstrings.h. st_stuff reads
// mapnames for the deathmatch/coop level title.
//
Doom::Array<std::string_view, 45>
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
