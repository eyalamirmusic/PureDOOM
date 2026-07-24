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
//        DOOM selection menu, options, episode etc.
//        Sliders and icons. Kinda widget stuff.
//
//-----------------------------------------------------------------------------

#include "../Host/Diagnostics.h"
#include "../Host/Platform.h"
#include "../Render/GraphicsData.h"

#include "../Game/GameDefs.h"
#include "../Game/Strings.h"
#include "../Game/DoomMain.h"
#include "../Wad/WadFile.h"
#include "Hud.h"
#include "../Game/Args.h"
#include "../Math/Swap.h"
#include "../Game/MapSpawns.h"
#include "../Game/SoundData.h" // Data.
#include "Menu.h"

#include "../Game/OverlayState.h"

#include "../Game/DemoState.h"
#include "../Game/GameClock.h"
#include "../Game/GameFlow.h"
#include "../Game/GameSession.h"
#include "../Game/GameVersion.h"
#include "../Game/InputConfig.h"
#include "../Game/LaunchOptions.h"
#include "../Game/OverlayState.h"
#include "../Game/PlayerState.h"
#include "../Game/SoundSettings.h"
#include "HudFlags.h"
#include "HudFont.h"
#include "Menu.h"
#include "MenuSettings.h"
#include "MenuState.h"

#include "../Game/Args.h"
#include "../Game/DoomMain.h"
#include "../Render/Video.h"
#include "../Containers.h"

#include "../Host/Video.h"
#include "../Game/Game.h"
#include "../Game/Sound.h"
#include "../Host/System.h"
#include "../Render/Main.h"
#include <functional>

// --- Globals other subsystems read ------------------------------------------
//
// These are the only menu globals anything outside the menu touches, so they
// stay at file scope rather than moving into namespace Doom below. mouseSensitivity
// is declared in doomstat.h. (messageToPrint is a MenuState member now, read by the
// eacp overlay through menuState() like the rest of the cluster.)
// menuactive, automapactive and inhelpscreens (which gates Doom::displayFrame's border
// redraw) are a Doom::OverlayState owned by the Engine now; these are references
// onto it (REFACTOR.md, Step 5).
// The config-backed settings (mouse sensitivity, message toggle, detail, view
// size) are Engine members now (UI/MenuSettings.h); these are references onto
// them. Config.cpp binds its defaults[] entries to the members at runtime rather
// than capturing their addresses at static-init, which is what unblocked the move.

// The quit-screen taunts, drawn by quitDOOM. Declared in dstrings.h, read only
// here, so their definition moved out of dstrings.cpp to sit with their one
// reader. ::-scoped for the extern; const so the literals stay off -Wwritable.
std::string_view endmsgData[Doom::NUM_QUITMESSAGES + 1] = {
    // DOOM1
    Doom::QUITMSG,
    "please don't leave, there's more\ndemons to toast!",
    "let's beat it -- this is turning\ninto a bloodbath!",
    "i wouldn't leave if i were you.\ndos is much worse.",
    "you're trying to say you like dos\nbetter than me, right?",
    "don't leave yet -- there's a\ndemon around that corner!",
    "ya know, next time you come in here\ni'm gonna toast ya.",
    "go ahead and leave. see if i care.",

    // QuitDOOM II messages
    "you want to quit?\nthen, thou hast lost an eighth!",
    "don't go now, there's a \ndimensional shambler waiting\nat the dos prompt!",
    "get outta here and go back\nto your boring programs.",
    "if i were your boss, i'd \n deathmatch ya in a minute!",
    "look, bud. you leave now\nand you forfeit your body count!",
    "just leave. when you come\nback, i'll be waiting with a bat.",
    "you're lucky i don't smack\nyou for thinking about leaving.",

    // FinalDOOM?
    "fuck you, pussy!\nget the fuck out!",
    "you quit and i'll jizz\nin your cystholes!",
    "if you leave, i'll make\nthe lord drink my jizz.",
    "hey, ron! can we say\n'fuck' in the game?",
    "i'd leave: this is just\nmore monsters and levels.\nwhat a load.",
    "suck it down, asshole!\nyou're a fucking wimp!",
    "don't quit now! we're \nstill spending your money!",

    // Internal debug. Different style, too.
    "THIS IS NO MESSAGE!\nPage intentionally left blank."};

// Hands out the file-local quit-taunt table above (was the `extern endmsg[]`).
const std::string_view* endmsg()
{
    return endmsgData;
}

namespace Doom
{

constexpr int SKULLXOFF = -32;
constexpr int LINEHEIGHT = 16;

//
// MENU TYPEDEFS
//
struct MenuItem
{
    // 0 = no cursor here, 1 = ok, 2 = arrows ok
    short status;

    std::string_view name;

    // choice = menu item #.
    // if status = 2,
    //   choice=0:leftarrow,1:rightarrow
    void (*routine)(int choice);

    // hotkey in menu
    char alphaKey;
};

struct MenuDef
{
    short numitems; // # of menu items
    MenuDef* prevMenu; // previous menu
    MenuItem* menuitems; // menu items
    void (*routine)(); // draw routine
    short x;
    short y; // x,y of menu
    short lastOn; // last item user was on in menu
};

struct MenuCustomTextSeg
{
    std::string_view lump;
    int x, w;
    int offx;
    int offy;
};

struct MenuCustomText
{
    std::string_view name;
    MenuCustomTextSeg segs[16];
};

// The menu's transient interaction state lives on the Engine (UI/MenuState.h). Every function
// below reaches it through a hoisted local instead of a file-scope alias (REFACTOR.md, Step 9
// strand (a)). The immutable reference-data tables interspersed among them (gammamsg / skullName /
// detailNames / msgNames / quitsounds / menu_custom_texts) and the self-referential menu-definition
// apparatus further down stay file-local.

// Was Array<Array<char, 26>, 5>, a fixed 26-byte buffer per message that
// only existed because these were string-literal macros. Views now, so the 26
// cannot silently truncate and no length is recomputed at each use.
Array<std::string_view, 5> gammamsg = {
    GAMMALVL0, GAMMALVL1, GAMMALVL2, GAMMALVL3, GAMMALVL4};

// graphic name of skulls
Array<std::string_view, 2> skullName = {"M_SKULL1", "M_SKULL2"};

// The menu-definition tables below lean on partial aggregate initializers on
// purpose: a {0} terminates a custom-text segment list, and a {-1,"",0} marks a
// non-selectable spacer row. Both leave trailing fields defaulted, which is what
// they mean - so the warning about it is silenced just over the data.
DOOM_DIAGNOSTIC_PUSH
DOOM_IGNORE_MISSING_FIELD_INITIALIZERS

// We create new menu text by cutting into existing graphics and pasting them to create the new text.
// This way we don't ship code with embeded graphics that come from WAD files.
Array<MenuCustomText, 4> menu_custom_texts = {
    {"TXT_MMOV",
     {{"M_MSENS", 0, 74, 0, 0}, // Mouse
      {"M_MSENS", 0, 31, 83, 0}, // Mo
      {"M_MSENS", 160, 14, 83 + 31, 0}, // v
      {"M_MSENS", 60, 14, 83 + 31 + 14, 0}, // e
      {"M_DETAIL", 169, 5, 83 + 31 + 14 + 14, 0}, // :
      {}}},
    {"TXT_MOPT",
     {{"M_MSENS", 0, 74, 0, 0}, // Mouse
      {"M_OPTION", 0, 92, 74 + 9, 0}, // Options
      {}}},
    {"TXT_CROS",
     {{"M_SKILL", 0, 16, 0, 0}, // C
      {"M_DETAIL", 14, 15, 16, 0}, // r
      {"M_SKILL", 46, 30, 16 + 15, 0}, // os
      {"M_SKILL", 62, 14, 16 + 15 + 30, 0}, // s
      {"M_SKILL", 16, 15, 16 + 15 + 30 + 14, 0}, // h
      {"M_DETAIL", 140, 19, 16 + 15 + 30 + 14 + 15, 0}, // ai
      {"M_DETAIL", 14, 15, 16 + 15 + 30 + 14 + 15 + 19, 0}, // r
      {"M_DETAIL", 169, 5, 16 + 15 + 30 + 14 + 15 + 19 + 15, 0}, // :
      {}}},
    {"TXT_ARUN",
     {{"M_SGTTL", 90, 17, 0, 0}, // A
      {"M_GDLOW", 0, 10, 17, 3}, // l
      {"M_GDLOW", 26, 16, 17 + 10, 3}, //
      {"M_DISP", 57, 30, 17 + 10 + 16, 0}, // ay
      {"M_RDTHIS", 99, 14, 17 + 10 + 16 + 30, 0}, // s
      {"M_RDTHIS", 0, 16, 17 + 10 + 16 + 30 + 14 + 7, 0}, // R
      {"M_SFXVOL", 90, 15, 17 + 10 + 16 + 30 + 14 + 7 + 16, 0}, // u
      {"M_OPTION", 62, 15, 17 + 10 + 16 + 30 + 14 + 7 + 16 + 15, 0}, // n
      {"M_DETAIL", 169, 5, 17 + 10 + 16 + 30 + 14 + 7 + 16 + 15 + 15, 0}, // :
      {}}},
};

Array<std::string_view, 2> detailNames = {"M_GDHIGH", "M_GDLOW"};
Array<std::string_view, 2> msgNames = {"M_MSGOFF", "M_MSGON"};

Array<SfxEnum, 8> quitsounds = {SfxEnum::Pldeth,
                                SfxEnum::Dmpain,
                                SfxEnum::Popain,
                                SfxEnum::Slop,
                                SfxEnum::Telept,
                                SfxEnum::Posit1,
                                SfxEnum::Posit3,
                                SfxEnum::Sgtatk};

Array<SfxEnum, 8> quitsounds2 = {SfxEnum::Vilact,
                                 SfxEnum::Getpow,
                                 SfxEnum::Boscub,
                                 SfxEnum::Slop,
                                 SfxEnum::Skeswg,
                                 SfxEnum::Kntdth,
                                 SfxEnum::Bspact,
                                 SfxEnum::Sgtatk};

//
// PROTOTYPES
//
void newGame(int choice);
void episode(int choice);
void chooseSkill(int choice);
void loadGameMenu(int choice);
void saveGameMenu(int choice);
void optionsMenu(int choice);
void endGame(int choice);
void readThis(int choice);
void readThis2(int choice);
void quitDOOM(int choice);

void changeMessages(int choice);
void sfxVol(int choice);
void musicVol(int choice);
void changeDetail(int choice);
void mouseOptions(int choice);
void sizeDisplay(int choice);
void startGame(int choice);
void sound(int choice);
void changeCrosshair(int choice);
void changeAlwaysRun(int choice);

void mouseMove(int choice);
void changeSensitivity(int choice);

void finishReadThis(int choice);
void loadSelect(int choice);
void saveSelect(int choice);
void readSaveStrings();
void quickSave();
void quickLoad();

void drawMainMenu();
void drawReadThis1();
void drawReadThis2();
void drawNewGame();
void drawEpisode();
void drawOptions();
void drawSound();
void drawLoad();
void drawSave();

void drawSaveLoadBorder(int x, int y);
void setupNextMenu(MenuDef* menudef);
void drawThermo(int x, int y, int thermWidth, int thermDot);
void drawEmptyCell(MenuDef* menu, int item);
void drawSelCell(MenuDef* menu, int item);
void writeText(int x, int y, std::string_view string);
int stringWidth(std::string_view string);
int stringHeight(std::string_view string);
void startControlPanel();
void startMessage(std::string_view string,
                  std::function<void(int)> routine,
                  bool input);
void stopMessage();
void clearMenus();
void drawMouseOptions();

//
// DOOM MENU
//
enum class MainItem
{
    NewGame = 0,
    Options,
    LoadGame,
    SaveGame,
    ReadThis,
    QuitDoom,
    End
};

Array<MenuItem, 6> MainMenu = {{1, "M_NGAME", newGame, 'n'},
                               {1, "M_OPTION", optionsMenu, 'o'},
                               {1, "M_LOADG", loadGameMenu, 'l'},
                               {1, "M_SAVEG", saveGameMenu, 's'},
                               // Another hickup with Special edition.
                               {1, "M_RDTHIS", readThis, 'r'},
                               {1, "M_QUITG", quitDOOM, 'q'}};

MenuDef MainDef = {
    toIndex(MainItem::End), nullptr, MainMenu.data(), drawMainMenu, 97, 64, 0};

//
// EPISODE SELECT
//
enum class EpisodeItem
{
    Ep1,
    Ep2,
    Ep3,
    Ep4,
    End
};

Array<MenuItem, 4> EpisodeMenu = {{1, "M_EPI1", episode, 'k'},
                                  {1, "M_EPI2", episode, 't'},
                                  {1, "M_EPI3", episode, 'i'},
                                  {1, "M_EPI4", episode, 't'}};

MenuDef EpiDef = {
    toIndex(EpisodeItem::End), // # of menu items
    &MainDef, // previous menu
    EpisodeMenu.data(), // MenuItem ->
    drawEpisode, // drawing routine ->
    48,
    63, // x,y
    toIndex(EpisodeItem::Ep1) // lastOn
};

//
// NEW GAME
//
enum class SkillItem
{
    KillThings,
    TooRough,
    HurtMe,
    Violence,
    Nightmare,
    End
};

Array<MenuItem, 5> NewGameMenu = {{1, "M_JKILL", chooseSkill, 'i'},
                                  {1, "M_ROUGH", chooseSkill, 'h'},
                                  {1, "M_HURT", chooseSkill, 'h'},
                                  {1, "M_ULTRA", chooseSkill, 'u'},
                                  {1, "M_NMARE", chooseSkill, 'n'}};

MenuDef NewDef = {
    toIndex(SkillItem::End), // # of menu items
    &EpiDef, // previous menu
    NewGameMenu.data(), // MenuItem ->
    drawNewGame, // drawing routine ->
    48,
    63, // x,y
    toIndex(SkillItem::HurtMe) // lastOn
};

//
// OPTIONS MENU
//
MenuItem* OptionsMenu;

enum class OptionsItem
{
    EndGame,
    Messages,
    CrosshairOpt,
    AlwaysRunOpt,
    //detail, // Details do nothing?
    ScrnSize,
    OptionEmpty1,
    Mouseoptions,
    SoundVol,
    End
};

Array<MenuItem, 8> OptionsMenuFull = {
    {1, "M_ENDGAM", endGame, 'e'},
    {1, "M_MESSG", changeMessages, 'm'},
    {1, "TXT_CROS", changeCrosshair, 'c'},
    {1, "TXT_ARUN", changeAlwaysRun, 'r'},
    //{1,"M_DETAIL", changeDetail,'g'},  // Details do nothing?
    {2, "M_SCRNSZ", sizeDisplay, 's'},
    {-1, "", nullptr},
    {1, "TXT_MOPT", mouseOptions, 'f'},
    {1, "M_SVOL", sound, 's'}};

MenuDef OptionsDef = {toIndex(OptionsItem::End),
                      &MainDef,
                      OptionsMenuFull.data(),
                      drawOptions,
                      60,
                      37,
                      0};

enum class OptionsNoMouseItem
{
    EndGameNoMouse,
    MessagesNoMouse,
    CrosshairOptNoMouse,
    AlwaysRunOptNoMouse,
    //detail_no_mouse, // Details do nothing?
    ScrnSizeNoMouse,
    OptionEmpty1NoMouse,
    SoundVolNoMouse,
    OptEndNoMouse
};

Array<MenuItem, 7> OptionsMenuNoMouse = {
    {1, "M_ENDGAM", endGame, 'e'},
    {1, "M_MESSG", changeMessages, 'm'},
    {1, "TXT_CROS", changeCrosshair, 'c'},
    {1, "TXT_ARUN", changeAlwaysRun, 'r'},
    //{1,"M_DETAIL",  changeDetail,'g'}, // Details do nothing?
    {2, "M_SCRNSZ", sizeDisplay, 's'},
    {-1, "", nullptr},
    {1, "M_SVOL", sound, 's'}};

MenuDef OptionsNoMouseDef = {toIndex(OptionsNoMouseItem::OptEndNoMouse),
                             &MainDef,
                             OptionsMenuNoMouse.data(),
                             drawOptions,
                             60,
                             37,
                             0};

enum class OptionsNoSoundItem
{
    EndGameNoSound,
    MessagesNoSound,
    CrosshairOptNoSound,
    AlwaysRunOptNoSound,
    //detail_no_sound, // Details do nothing?
    ScrnSizeNoSound,
    OptionEmpty1NoSound,
    MouseoptionsNoSound,
    OptEndNoSound
};

Array<MenuItem, 7> OptionsMenuNoSound = {
    {1, "M_ENDGAM", endGame, 'e'},
    {1, "M_MESSG", changeMessages, 'm'},
    {1, "TXT_CROS", changeCrosshair, 'c'},
    {1, "TXT_ARUN", changeAlwaysRun, 'r'},
    //{1,"M_DETAIL",  changeDetail,'g'}, // Details do nothing?
    {2, "M_SCRNSZ", sizeDisplay, 's'},
    {-1, "", nullptr},
    {1, "TXT_MOPT", mouseOptions, 'f'}};

MenuDef OptionsNoSoundDef = {toIndex(OptionsNoSoundItem::OptEndNoSound),
                             &MainDef,
                             OptionsMenuNoSound.data(),
                             drawOptions,
                             60,
                             37,
                             0};

enum class OptionsNoSoundNoMouseItem
{
    EndGameNoSoundNoMouse,
    MessagesNoSoundNoMouse,
    CrosshairOptNoSoundNoMouse,
    AlwaysRunTopNoSoundNoMouse,
    //detail_no_sound_no_mouse, // Details do nothing?
    ScrnSizeNoSoundNoMouse,
    OptionEmpty1NoSoundNoMouse,
    OptEndNoSoundNoMouse
};

Array<MenuItem, 6> OptionsMenuNoSoundNoMouse = {
    {1, "M_ENDGAM", endGame, 'e'},
    {1, "M_MESSG", changeMessages, 'm'},
    {1, "TXT_CROS", changeCrosshair, 'c'},
    {1, "TXT_ARUN", changeAlwaysRun, 'r'},
    //{1,"M_DETAIL",  changeDetail,'g'}, // Details do nothing?
    {2, "M_SCRNSZ", sizeDisplay, 's'},
    {-1, "", nullptr}};

MenuDef OptionsNoSoundNoMouseDef = {
    toIndex(OptionsNoSoundNoMouseItem::OptEndNoSoundNoMouse),
    &MainDef,
    OptionsMenuNoSoundNoMouse.data(),
    drawOptions,
    60,
    37,
    0};

//
// MOUSE OPTIONS
//
enum class MouseItem
{
    MouseMov,
    MouseSens,
    MouseOptionEmpty1,
    End
};

Array<MenuItem, 3> MouseOptionsMenu = {
    {1, "TXT_MMOV", mouseMove, 'f'},
    {2, "M_MSENS", changeSensitivity, 'm'},
    {-1, "", nullptr},
};

MenuDef MouseOptionsDef = {toIndex(MouseItem::End),
                           &OptionsDef,
                           MouseOptionsMenu.data(),
                           drawMouseOptions,
                           60,
                           70,
                           0};

//
// Read This! MENU 1 & 2
//
enum class ReadThis1Item
{
    Empty1,
    End
};

Array<MenuItem, 1> ReadMenu1 = {{1, "", readThis2, 0}};

MenuDef ReadDef1 = {toIndex(ReadThis1Item::End),
                    &MainDef,
                    ReadMenu1.data(),
                    drawReadThis1,
                    280,
                    185,
                    0};

enum class ReadThis2Item
{
    Empty2,
    End
};

Array<MenuItem, 1> ReadMenu2 = {{1, "", finishReadThis, 0}};

MenuDef ReadDef2 = {toIndex(ReadThis2Item::End),
                    &ReadDef1,
                    ReadMenu2.data(),
                    drawReadThis2,
                    330,
                    175,
                    0};

//
// SOUND VOLUME MENU
//
MenuItem* SoundMenu;

enum class SoundItem
{
    SfxVol,
    SfxEmpty1,
    MusicVol,
    SfxEmpty2,
    End
};

Array<MenuItem, 4> SoundMenuFull = {{2, "M_SFXVOL", sfxVol, 's'},
                                    {-1, "", nullptr},
                                    {2, "M_MUSVOL", musicVol, 'm'},
                                    {-1, "", nullptr}};

MenuDef SoundDef = {toIndex(SoundItem::End),
                    &OptionsDef,
                    SoundMenuFull.data(),
                    drawSound,
                    80,
                    64,
                    0};

enum class SoundNoSfxItem
{
    MusicVolNoSfx,
    SfxEmpty2NoSfx,
    SoundEndNoSfx
};

Array<MenuItem, 2> SoundMenuNoSFX = {{2, "M_MUSVOL", musicVol, 'm'},
                                     {-1, "", nullptr}};

MenuDef SoundNoSFXDef = {toIndex(SoundNoSfxItem::SoundEndNoSfx),
                         &OptionsDef,
                         SoundMenuNoSFX.data(),
                         drawSound,
                         80,
                         64,
                         0};

enum class SoundNoMusicItem
{
    SfxVolNoMusic,
    SfxEmpty1NoMusic,
    SoundEndNoMusic
};

Array<MenuItem, 2> SoundMenuNoMusic = {{2, "M_SFXVOL", sfxVol, 's'},
                                       {-1, "", nullptr}};

MenuDef SoundNoMusicDef = {toIndex(SoundNoMusicItem::SoundEndNoMusic),
                           &OptionsDef,
                           SoundMenuNoMusic.data(),
                           drawSound,
                           80,
                           64,
                           0};

//
// LOAD GAME MENU
//
enum class LoadItem
{
    Load1,
    Load2,
    Load3,
    Load4,
    Load5,
    Load6,
    End
};

Array<MenuItem, 6> DOOM_LoadMenu = {{1, "", loadSelect, '1'},
                                    {1, "", loadSelect, '2'},
                                    {1, "", loadSelect, '3'},
                                    {1, "", loadSelect, '4'},
                                    {1, "", loadSelect, '5'},
                                    {1, "", loadSelect, '6'}};

MenuDef LoadDef = {
    toIndex(LoadItem::End), &MainDef, DOOM_LoadMenu.data(), drawLoad, 80, 54, 0};

//
// SAVE GAME MENU
//
Array<MenuItem, 6> SaveMenu = {{1, "", saveSelect, '1'},
                               {1, "", saveSelect, '2'},
                               {1, "", saveSelect, '3'},
                               {1, "", saveSelect, '4'},
                               {1, "", saveSelect, '5'},
                               {1, "", saveSelect, '6'}};

MenuDef SaveDef = {
    toIndex(LoadItem::End), &MainDef, SaveMenu.data(), drawSave, 80, 54, 0};

DOOM_DIAGNOSTIC_POP

//
// drawCustomMenuText
//  Draw several segments of patches to make up new text
//
void drawCustomMenuText(std::string_view name, int x, int y)
{
    for (auto& custom_text: menu_custom_texts)
    {
        if (custom_text.name == name)
        {
            MenuCustomTextSeg* seg = custom_text.segs;
            while (!seg->lump.empty())
            {
                void* lump = cacheLumpName(seg->lump);
                drawPatchRectDirect(
                    x + seg->offx, y, 0, static_cast<Patch*>(lump), seg->x, seg->w);
                ++seg;
            }
            break;
        }
    }
}

//
// readSaveStrings
//  read the strings from the savegame files
//
void readSaveStrings()
{
    auto& state = menuState();

    void* handle;

    for (int i = 0; i < toIndex(LoadItem::End); i++)
    {
        //doom_sprintf(name, SAVEGAMENAME"%d.dsg", i);
        auto name = concat(SAVEGAMENAME, i, ".dsg");

        handle = host().open(name.c_str(), "r");
        if (handle == nullptr)
        {
            state.savegamestrings[i] = EMPTYSTRING;
            DOOM_LoadMenu[i].status = 0;
            continue;
        }

        // The description is a fixed menuSaveStringSize-byte field on disk,
        // zero-padded; keep the text up to its first NUL.
        Array<char, menuSaveStringSize> field = {};
        host().read(handle, field.data(), menuSaveStringSize);
        state.savegamestrings[i] = nameView(field.data(), menuSaveStringSize);
        host().close(handle);
        DOOM_LoadMenu[i].status = 1;
    }
}

//
// loadGameMenu & Cie.
//
void drawLoad()
{
    auto& state = menuState();

    drawPatchDirect(72, 28, 0, static_cast<Patch*>(cacheLumpName("M_LOADG")));
    for (int i = 0; i < toIndex(LoadItem::End); i++)
    {
        drawSaveLoadBorder(LoadDef.x, LoadDef.y + LINEHEIGHT * i);
        writeText(
            LoadDef.x, LoadDef.y + LINEHEIGHT * i, state.savegamestrings[i].data());
    }
}

//
// Draw border for the savegame description
//
void drawSaveLoadBorder(int x, int y)
{
    drawPatchDirect(x - 8, y + 7, 0, static_cast<Patch*>(cacheLumpName("M_LSLEFT")));

    for (int i = 0; i < 24; i++)
    {
        drawPatchDirect(x, y + 7, 0, static_cast<Patch*>(cacheLumpName("M_LSCNTR")));
        x += 8;
    }

    drawPatchDirect(x, y + 7, 0, static_cast<Patch*>(cacheLumpName("M_LSRGHT")));
}

//
// User wants to load this game
//
void loadSelect(int choice)
{
    //doom_sprintf(name, SAVEGAMENAME"%d.dsg", choice);
    loadGame(concat(SAVEGAMENAME, choice, ".dsg"));
    clearMenus();
}

//
// Selected from DOOM menu
//
void loadGameMenu(int)
{
    if (gameSession().netgame)
    {
        startMessage(LOADNET, {}, false);
        return;
    }

    setupNextMenu(&LoadDef);
    readSaveStrings();
}

//
//  saveGameMenu & Cie.
//
void drawSave()
{
    auto& state = menuState();

    int i;

    drawPatchDirect(72, 28, 0, static_cast<Patch*>(cacheLumpName("M_SAVEG")));
    for (i = 0; i < toIndex(LoadItem::End); i++)
    {
        drawSaveLoadBorder(LoadDef.x, LoadDef.y + LINEHEIGHT * i);
        writeText(
            LoadDef.x, LoadDef.y + LINEHEIGHT * i, state.savegamestrings[i].c_str());
    }

    if (state.saveStringEnter)
    {
        i = stringWidth(state.savegamestrings[state.saveSlot]);
        writeText(LoadDef.x + i, LoadDef.y + LINEHEIGHT * state.saveSlot, "_");
    }
}

//
// menuResponder calls this when user is finished
//
void doSave(int slot)
{
    auto& state = menuState();

    saveGame(slot, state.savegamestrings[slot]);
    clearMenus();

    // PICK QUICKSAVE SLOT YET?
    if (state.quickSaveSlot == -2)
        state.quickSaveSlot = slot;
}

//
// User wants to save. Start string input for menuResponder
//
void saveSelect(int choice)
{
    auto& state = menuState();

    // we are going to be intercepting all chars
    state.saveStringEnter = 1;

    state.saveSlot = choice;
    state.saveOldString = state.savegamestrings[choice];
    if (state.savegamestrings[choice] == EMPTYSTRING)
        state.savegamestrings[choice].clear();
}

//
// Selected from DOOM menu
//
void saveGameMenu(int)
{
    if (!demoState().usergame)
    {
        startMessage(SAVEDEAD, {}, false);
        return;
    }

    if (gameFlow().gamestate != GameState::Level)
        return;

    setupNextMenu(&SaveDef);
    readSaveStrings();
}

//
// quickSave
//
void quickSaveResponse(int ch)
{
    if (ch == 'y')
    {
        doSave(menuState().quickSaveSlot);
        startSound(nullptr, SfxEnum::Swtchx);
    }
}

void quickSave()
{
    auto& state = menuState();

    if (!demoState().usergame)
    {
        startSound(nullptr, SfxEnum::Oof);
        return;
    }

    if (gameFlow().gamestate != GameState::Level)
        return;

    if (state.quickSaveSlot < 0)
    {
        startControlPanel();
        readSaveStrings();
        setupNextMenu(&SaveDef);
        state.quickSaveSlot = -2; // means to pick a slot now
        return;
    }
    //doom_sprintf(tempstring, QSPROMPT, savegamestrings[quickSaveSlot]);
    // The lineage's last call was doom_strcpy where concat was meant, so the
    // prompt it shows is QSPROMPT_2 alone and the savegame name never appears.
    // Preserved, not fixed - restoring the name is a behaviour change.
    state.tempstring =
        concat(QSPROMPT_1, state.savegamestrings[state.quickSaveSlot]);
    state.tempstring = QSPROMPT_2;
    startMessage(state.tempstring, quickSaveResponse, true);
}

//
// quickLoad
//
void quickLoadResponse(int ch)
{
    if (ch == 'y')
    {
        loadSelect(menuState().quickSaveSlot);
        startSound(nullptr, SfxEnum::Swtchx);
    }
}

void quickLoad()
{
    auto& state = menuState();

    if (gameSession().netgame)
    {
        startMessage(QLOADNET, {}, false);
        return;
    }

    if (state.quickSaveSlot < 0)
    {
        startMessage(QSAVESPOT, {}, false);
        return;
    }
    //doom_sprintf(tempstring, QLPROMPT, savegamestrings[quickSaveSlot]);
    // Same lineage bug as quickSave above: the last doom_strcpy overwrote the
    // name it just built. Preserved, not fixed.
    state.tempstring =
        concat(QLPROMPT_1, state.savegamestrings[state.quickSaveSlot]);
    state.tempstring = QLPROMPT_2;
    startMessage(state.tempstring, quickLoadResponse, true);
}

//
// Read This Menus
// Had a "quick hack to fix romero bug"
//
void drawReadThis1()
{
    overlayState().inhelpscreens = true;
    switch (gameVersion().gamemode)
    {
        case GameMode::Commercial:
            drawPatchDirect(0, 0, 0, static_cast<Patch*>(cacheLumpName("HELP")));
            break;
        case GameMode::Shareware:
        case GameMode::Registered:
        case GameMode::Retail:
            drawPatchDirect(0, 0, 0, static_cast<Patch*>(cacheLumpName("HELP1")));
            break;
        case GameMode::Indetermined:
            break;
    }
    return;
}

//
// Read This Menus - optional second page.
//
void drawReadThis2()
{
    overlayState().inhelpscreens = true;
    switch (gameVersion().gamemode)
    {
        case GameMode::Retail:
        case GameMode::Commercial:
            // This hack keeps us from having to change menus.
            drawPatchDirect(0, 0, 0, static_cast<Patch*>(cacheLumpName("CREDIT")));
            break;
        case GameMode::Shareware:
        case GameMode::Registered:
            drawPatchDirect(0, 0, 0, static_cast<Patch*>(cacheLumpName("HELP2")));
            break;
        case GameMode::Indetermined:
            break;
    }
    return;
}

//
// Change Sfx & Music volumes
//
void drawSound()
{
    auto& sndset = soundSettings();

    drawPatchDirect(60, 38, 0, static_cast<Patch*>(cacheLumpName("M_SVOL")));

    if (!(host().flags & DOOM_FLAG_HIDE_SOUND_OPTIONS))
    {
        int offset = (host().flags & DOOM_FLAG_HIDE_MUSIC_OPTIONS)
                         ? static_cast<int>(toIndex(SoundNoMusicItem::SfxVolNoMusic))
                         : static_cast<int>(toIndex(SoundItem::SfxVol));
        drawThermo(SoundDef.x,
                   SoundDef.y + LINEHEIGHT * (offset + 1),
                   16,
                   sndset.sfxVolume);
    }

    if (!(host().flags & DOOM_FLAG_HIDE_MUSIC_OPTIONS))
    {
        int offset = (host().flags & DOOM_FLAG_HIDE_SOUND_OPTIONS)
                         ? static_cast<int>(toIndex(SoundNoSfxItem::MusicVolNoSfx))
                         : static_cast<int>(toIndex(SoundItem::MusicVol));
        drawThermo(SoundDef.x,
                   SoundDef.y + LINEHEIGHT * (offset + 1),
                   16,
                   sndset.musicVolume);
    }
}

void sound(int)
{
    setupNextMenu(&SoundDef);
}

void mouseOptions(int)
{
    setupNextMenu(&MouseOptionsDef);
}

void sfxVol(int choice)
{
    auto& sndset = soundSettings();

    switch (choice)
    {
        case 0:
            if (sndset.sfxVolume)
                sndset.sfxVolume--;
            break;
        case 1:
            if (sndset.sfxVolume < 15)
                sndset.sfxVolume++;
            break;
    }

    setSfxVolume(sndset.sfxVolume /* *8 */);
}

void musicVol(int choice)
{
    auto& sndset = soundSettings();

    switch (choice)
    {
        case 0:
            if (sndset.musicVolume)
                sndset.musicVolume--;
            break;
        case 1:
            if (sndset.musicVolume < 15)
                sndset.musicVolume++;
            break;
    }

    setMusicVolumeLevel(sndset.musicVolume /* *8 */);
}

//
// drawMainMenu
//
void drawMainMenu()
{
    drawPatchDirect(94, 2, 0, static_cast<Patch*>(cacheLumpName("M_DOOM")));
}

//
// newGame
//
void drawNewGame()
{
    drawPatchDirect(96, 14, 0, static_cast<Patch*>(cacheLumpName("M_NEWG")));
    drawPatchDirect(54, 38, 0, static_cast<Patch*>(cacheLumpName("M_SKILL")));
}

void newGame(int)
{
    if (gameSession().netgame && !demoState().demoplayback)
    {
        startMessage(NEWGAME, {}, false);
        return;
    }

    if (gameVersion().gamemode == GameMode::Commercial)
        setupNextMenu(&NewDef);
    else
        setupNextMenu(&EpiDef);
}

//
// episode
//
void drawEpisode()
{
    drawPatchDirect(54, 38, 0, static_cast<Patch*>(cacheLumpName("M_EPISOD")));
}

void verifyNightmare(int ch)
{
    if (ch != 'y')
        return;

    deferInitNew(
        static_cast<Skill>(toIndex(SkillItem::Nightmare)), menuState().epi + 1, 1);
    clearMenus();
}

void chooseSkill(int choice)
{
    if (choice == toIndex(SkillItem::Nightmare))
    {
        startMessage(NIGHTMARE, verifyNightmare, true);
        return;
    }

    deferInitNew(static_cast<Skill>(choice), menuState().epi + 1, 1);
    clearMenus();
}

void episode(int choice)
{
    auto& version = gameVersion();

    if ((version.gamemode == GameMode::Shareware) && choice)
    {
        startMessage(SWSTRING, {}, false);
        setupNextMenu(&ReadDef1);
        return;
    }

    // Yet another hack...
    if ((version.gamemode == GameMode::Registered) && (choice > 2))
    {
        print("episode: 4th episode requires UltimateDOOM\n");
        choice = 0;
    }

    menuState().epi = choice;
    setupNextMenu(&NewDef);
}

//
// optionsMenu
//
void drawOptions()
{
    auto& input = inputConfig();

    drawPatchDirect(108, 15, 0, static_cast<Patch*>(cacheLumpName("M_OPTTTL")));

    //drawPatchDirect (OptionsDef.x + 175,OptionsDef.y+LINEHEIGHT*detail,0,
    //                cacheLumpName(detailNames[detailLevel])); // Details do nothing?

    drawPatchDirect(
        OptionsDef.x + 120,
        OptionsDef.y + LINEHEIGHT * toIndex(OptionsItem::Messages),
        0,
        static_cast<Patch*>(cacheLumpName(msgNames[menuSettings().showMessages])));

    drawPatchDirect(OptionsDef.x + 131,
                    OptionsDef.y + LINEHEIGHT * toIndex(OptionsItem::CrosshairOpt),
                    0,
                    static_cast<Patch*>(cacheLumpName(msgNames[input.crosshair])));

    drawPatchDirect(OptionsDef.x + 147,
                    OptionsDef.y + LINEHEIGHT * toIndex(OptionsItem::AlwaysRunOpt),
                    0,
                    static_cast<Patch*>(cacheLumpName(msgNames[input.always_run])));

    drawThermo(OptionsDef.x,
               OptionsDef.y + LINEHEIGHT * (toIndex(OptionsItem::ScrnSize) + 1),
               9,
               menuState().screenSize);
}

void drawMouseOptions()
{
    drawCustomMenuText("TXT_MOPT", 74, 45);

    drawPatchDirect(
        MouseOptionsDef.x + 149,
        MouseOptionsDef.y + LINEHEIGHT * toIndex(MouseItem::MouseMov),
        0,
        static_cast<Patch*>(cacheLumpName(msgNames[inputConfig().mousemove])));

    drawThermo(MouseOptionsDef.x,
               MouseOptionsDef.y + LINEHEIGHT * (toIndex(MouseItem::MouseSens) + 1),
               10,
               menuSettings().mouseSensitivity);
}

void optionsMenu(int)
{
    setupNextMenu(&OptionsDef);
}

//
// Toggle messages on/off
//
void changeMessages(int)
{
    auto& players_ = playerState();
    auto& settings = menuSettings();

    settings.showMessages = 1 - settings.showMessages;

    if (!settings.showMessages)
        players_.players[players_.consoleplayer].message = MSGOFF;
    else
        players_.players[players_.consoleplayer].message = MSGON;

    hudFlags().message_dontfuckwithme = true;
}

//
// Toggle crosshair on/off
//
void changeCrosshair(int)
{
    auto& input = inputConfig();
    auto& players_ = playerState();

    input.crosshair = 1 - input.crosshair;

    if (!input.crosshair)
        players_.players[players_.consoleplayer].message = CROSSOFF;
    else
        players_.players[players_.consoleplayer].message = CROSSON;

    hudFlags().message_dontfuckwithme = true;
}

//
// Toggle always-run on/off
//
void changeAlwaysRun(int)
{
    auto& input = inputConfig();
    auto& players_ = playerState();

    input.always_run = 1 - input.always_run;

    if (!input.always_run)
        players_.players[players_.consoleplayer].message = ALWAYSRUNOFF;
    else
        players_.players[players_.consoleplayer].message = ALWAYSRUNON;

    hudFlags().message_dontfuckwithme = true;
}

//
// endGame
//
void endGameResponse(int ch)
{
    if (ch != 'y')
        return;

    auto& state = menuState();

    state.currentMenu->lastOn = state.itemOn;
    clearMenus();
    startTitle();
}

void endGame(int)
{
    if (!demoState().usergame)
    {
        startSound(nullptr, SfxEnum::Oof);
        return;
    }

    if (gameSession().netgame)
    {
        startMessage(NETEND, {}, false);
        return;
    }

    startMessage(ENDGAME, endGameResponse, true);
}

//
// readThis
//
void readThis(int)
{
    setupNextMenu(&ReadDef1);
}

void readThis2(int)
{
    setupNextMenu(&ReadDef2);
}

void finishReadThis(int)
{
    setupNextMenu(&MainDef);
}

//
// quitDOOM
//
void quitResponse(int ch)
{
    auto& clock = gameClock();

    if (ch != 'y')
        return;
    if (!gameSession().netgame)
    {
        if (gameVersion().gamemode == GameMode::Commercial)
            startSound(nullptr, quitsounds2[(clock.gametic >> 2) & 7]);
        else
            startSound(nullptr, quitsounds[(clock.gametic >> 2) & 7]);
        waitVBlank(105);
    }
    quitGame();
}

void quitDOOM(int)
{
    auto& state = menuState();

    // We pick index 0 which is language sensitive,
    //  or one at random, between 1 and maximum number.
    if (gameVersion().language != Language::English)
    {
        //doom_sprintf(endstring, "%s\n\n"DOSY, endmsg[0]);
        state.endstring = concat(endmsg()[0], "\n\n" DOSY);
    }
    else
    {
        //doom_sprintf(endstring, "%s\n\n" DOSY, endmsg[gametic % (NUM_QUITMESSAGES - 2) + 1]);
        state.endstring = concat(
            endmsg()[gameClock().gametic % (NUM_QUITMESSAGES - 2) + 1], "\n\n" DOSY);
    }

    startMessage(state.endstring, quitResponse, true);
}

void changeSensitivity(int choice)
{
    auto& settings = menuSettings();

    switch (choice)
    {
        case 0:
            if (settings.mouseSensitivity)
                settings.mouseSensitivity--;
            break;
        case 1:
            if (settings.mouseSensitivity < 9)
                settings.mouseSensitivity++;
            break;
    }
}

void mouseMove(int)
{
    auto& input = inputConfig();

    input.mousemove = 1 - input.mousemove;

    return;
}

void changeDetail(int)
{
    auto& settings = menuSettings();

    settings.detailLevel = 1 - settings.detailLevel;

    // FIXME - does not work. Remove anyway?
    print("changeDetail: low detail mode n.a.\n");
}

void sizeDisplay(int choice)
{
    auto& settings = menuSettings();
    auto& state = menuState();

    switch (choice)
    {
        case 0:
            if (state.screenSize > 0)
            {
                settings.screenblocks--;
                state.screenSize--;
            }
            break;
        case 1:
            if (state.screenSize < 8)
            {
                settings.screenblocks++;
                state.screenSize++;
            }
            break;
    }

    setViewSize(settings.screenblocks, settings.detailLevel);
}

//
// Menu Functions
//
void drawThermo(int x, int y, int thermWidth, int thermDot)
{
    int xx = x;
    drawPatchDirect(xx, y, 0, static_cast<Patch*>(cacheLumpName("M_THERML")));
    xx += 8;
    for (int i = 0; i < thermWidth; i++)
    {
        drawPatchDirect(xx, y, 0, static_cast<Patch*>(cacheLumpName("M_THERMM")));
        xx += 8;
    }
    drawPatchDirect(xx, y, 0, static_cast<Patch*>(cacheLumpName("M_THERMR")));

    drawPatchDirect((x + 8) + thermDot * 8,
                    y,
                    0,
                    static_cast<Patch*>(cacheLumpName("M_THERMO")));
}

void drawEmptyCell(MenuDef* menu, int item)
{
    drawPatchDirect(menu->x - 10,
                    menu->y + item * LINEHEIGHT - 1,
                    0,
                    static_cast<Patch*>(cacheLumpName("M_CELL1")));
}

void drawSelCell(MenuDef* menu, int item)
{
    drawPatchDirect(menu->x - 10,
                    menu->y + item * LINEHEIGHT - 1,
                    0,
                    static_cast<Patch*>(cacheLumpName("M_CELL2")));
}

void startMessage(std::string_view string,
                  std::function<void(int)> routine,
                  bool input)
{
    auto& overlay = overlayState();
    auto& state = menuState();

    state.messageLastMenuActive = overlay.menuactive;
    state.messageToPrint = 1;
    state.messageString = string;
    // An empty routine means "no answer needed"; keep the member callable so the
    // responder never has to test it.
    state.messageRoutine =
        routine ? std::move(routine) : std::function<void(int)> {[](int) {}};
    state.messageNeedsInput = input;
    overlay.menuactive = true;
}

void stopMessage()
{
    overlayState().menuactive = menuState().messageLastMenuActive;
    menuState().messageToPrint = 0;
}

//
// Find string width from hu_font chars
//
int stringWidth(std::string_view string)
{
    int w = 0;

    for (auto character: string)
    {
        int c = toUpper(character) - HU_FONTSTART;
        if (c < 0 || c >= HU_FONTSIZE)
            w += 4;
        else
            w += littleEndian(hudFont().hu_font[c]->width);
    }

    return w;
}

//
// Find string height from hu_font chars
//
int stringHeight(std::string_view string)
{
    int height = littleEndian(hudFont().hu_font[0]->height);

    int h = height;
    for (auto character: string)
        if (character == '\n')
            h += height;

    return h;
}

//
// Write a string using the hu_font
//
void writeText(int x, int y, std::string_view string)
{
    auto& font = hudFont();

    int cx = x;
    int cy = y;

    for (auto character: string)
    {
        if (character == '\n')
        {
            cx = x;
            cy += 12;
            continue;
        }

        int c = toUpper(character) - HU_FONTSTART;
        if (c < 0 || c >= HU_FONTSIZE)
        {
            cx += 4;
            continue;
        }

        int w = littleEndian(font.hu_font[c]->width);
        if (cx + w > SCREENWIDTH)
            break;
        drawPatchDirect(cx, cy, 0, font.hu_font[c]);
        cx += w;
    }
}

//
// CONTROL PANEL
//

//
// menuResponder
//
bool menuResponder(Event& ev)
{
    auto& overlay = overlayState();
    auto& players_ = playerState();
    auto& settings = menuSettings();

    auto& state = menuState();

    int ch = -1;

    if (ev.type == EventType::Joystick && state.joywait < currentTic())
    {
        if (ev.data3 == -1)
        {
            ch = KEY_UPARROW;
            state.joywait = currentTic() + 5;
        }
        else if (ev.data3 == 1)
        {
            ch = KEY_DOWNARROW;
            state.joywait = currentTic() + 5;
        }

        if (ev.data2 == -1)
        {
            ch = KEY_LEFTARROW;
            state.joywait = currentTic() + 2;
        }
        else if (ev.data2 == 1)
        {
            ch = KEY_RIGHTARROW;
            state.joywait = currentTic() + 2;
        }

        if (ev.data1 & 1)
        {
            ch = KEY_ENTER;
            state.joywait = currentTic() + 5;
        }
        if (ev.data1 & 2)
        {
            ch = KEY_BACKSPACE;
            state.joywait = currentTic() + 5;
        }
    }
    else
    {
        if (ev.type == EventType::Mouse && state.mousewait < currentTic())
        {
            state.mousey += ev.data3;
            if (state.mousey < state.lasty - 30)
            {
                ch = KEY_DOWNARROW;
                state.mousewait = currentTic() + 5;
                state.mousey = state.lasty -= 30;
            }
            else if (state.mousey > state.lasty + 30)
            {
                ch = KEY_UPARROW;
                state.mousewait = currentTic() + 5;
                state.mousey = state.lasty += 30;
            }

            state.mousex += ev.data2;
            if (state.mousex < state.lastx - 30)
            {
                ch = KEY_LEFTARROW;
                state.mousewait = currentTic() + 5;
                state.mousex = state.lastx -= 30;
            }
            else if (state.mousex > state.lastx + 30)
            {
                ch = KEY_RIGHTARROW;
                state.mousewait = currentTic() + 5;
                state.mousex = state.lastx += 30;
            }

            if (ev.data1 & 1)
            {
                ch = KEY_ENTER;
                state.mousewait = currentTic() + 15;
            }

            if (ev.data1 & 2)
            {
                ch = KEY_BACKSPACE;
                state.mousewait = currentTic() + 15;
            }
        }
        else if (ev.type == EventType::KeyDown)
        {
            ch = ev.data1;
        }
    }

    if (ch == -1)
        return false;

    // Save Game string input
    if (state.saveStringEnter)
    {
        auto& entry = state.savegamestrings[state.saveSlot];

        switch (ch)
        {
            case KEY_BACKSPACE:
                if (!entry.empty())
                    entry.pop_back();
                break;

            case KEY_ESCAPE:
                state.saveStringEnter = 0;
                entry = state.saveOldString;
                break;

            case KEY_ENTER:
                state.saveStringEnter = 0;
                if (!entry.empty())
                    doSave(state.saveSlot);
                break;

            default:
                ch = toUpper(ch);
                if (ch != 32)
                    if (ch - HU_FONTSTART < 0 || ch - HU_FONTSTART >= HU_FONTSIZE)
                        break;
                if (ch >= 32 && ch <= 127
                    && static_cast<int>(entry.size()) < menuSaveStringSize - 1
                    && stringWidth(entry) < (menuSaveStringSize - 2) * 8)
                {
                    entry += static_cast<char>(ch);
                }
                break;
        }
        return true;
    }

    // Take care of any messages that need input
    if (state.messageToPrint)
    {
        if (state.messageNeedsInput == true
            && !(ch == ' ' || ch == 'n' || ch == 'y' || ch == KEY_ESCAPE))
            return false;

        overlay.menuactive = state.messageLastMenuActive;
        state.messageToPrint = 0;
        state.messageRoutine(ch);

        overlay.menuactive = false;
        startSound(nullptr, SfxEnum::Swtchx);
        return true;
    }

    if (launchOptions().devparm && ch == KEY_F1)
    {
        takeScreenshot();
        return true;
    }

    // F-Keys
    if (!overlay.menuactive)
        switch (ch)
        {
            case KEY_MINUS: // Screen size down
                if (overlayState().automapactive || hudFlags().chat_on)
                    return false;
                sizeDisplay(0);
                startSound(nullptr, SfxEnum::Stnmov);
                return true;

            case KEY_EQUALS: // Screen size up
                if (overlayState().automapactive || hudFlags().chat_on)
                    return false;
                sizeDisplay(1);
                startSound(nullptr, SfxEnum::Stnmov);
                return true;

            case KEY_F1: // Help key
                startControlPanel();

                if (gameVersion().gamemode == GameMode::Retail)
                    state.currentMenu = &ReadDef2;
                else
                    state.currentMenu = &ReadDef1;

                state.itemOn = 0;
                startSound(nullptr, SfxEnum::Swtchn);
                return true;

            case KEY_F2: // Save
                startControlPanel();
                startSound(nullptr, SfxEnum::Swtchn);
                saveGameMenu(0);
                return true;

            case KEY_F3: // Load
                startControlPanel();
                startSound(nullptr, SfxEnum::Swtchn);
                loadGameMenu(0);
                return true;

            case KEY_F4: // Sound Volume
                startControlPanel();
                state.currentMenu = &SoundDef;
                state.itemOn = toIndex(SoundItem::SfxVol);
                startSound(nullptr, SfxEnum::Swtchn);
                return true;

                // case KEY_F5:            // Detail toggle
                //     changeDetail(0);
                //     startSound(0, SfxEnum::Swtchn);
                //     return true;

            case KEY_F5: // Crosshair toggle
                changeCrosshair(0);
                startSound(nullptr, SfxEnum::Swtchn);
                return true;

            case KEY_F6: // Quicksave
                startSound(nullptr, SfxEnum::Swtchn);
                quickSave();
                return true;

            case KEY_F7: // End game
                startSound(nullptr, SfxEnum::Swtchn);
                endGame(0);
                return true;

            case KEY_F8: // Toggle messages
                changeMessages(0);
                startSound(nullptr, SfxEnum::Swtchn);
                return true;

            case KEY_F9: // Quickload
                startSound(nullptr, SfxEnum::Swtchn);
                quickLoad();
                return true;

            case KEY_F10: // Quit DOOM
                startSound(nullptr, SfxEnum::Swtchn);
                quitDOOM(0);
                return true;

            case KEY_F11: // gamma toggle
                settings.usegamma++;
                if (settings.usegamma > 4)
                    settings.usegamma = 0;
                players_.players[players_.consoleplayer].message =
                    gammamsg[settings.usegamma];
                setPalette(static_cast<byte*>(cacheLumpName("PLAYPAL")));
                return true;
        }

    // Pop-up menu?
    if (!overlay.menuactive)
    {
        if (ch == KEY_ESCAPE)
        {
            startControlPanel();
            startSound(nullptr, SfxEnum::Swtchn);
            return true;
        }
        return false;
    }

    // Keys usable within menu
    switch (ch)
    {
        case KEY_DOWNARROW:
            do
            {
                if (state.itemOn + 1 > state.currentMenu->numitems - 1)
                    state.itemOn = 0;
                else
                    state.itemOn++;
                startSound(nullptr, SfxEnum::Pstop);
            } while (state.currentMenu->menuitems[state.itemOn].status == -1);
            return true;

        case KEY_UPARROW:
            do
            {
                if (!state.itemOn)
                    state.itemOn = state.currentMenu->numitems - 1;
                else
                    state.itemOn--;
                startSound(nullptr, SfxEnum::Pstop);
            } while (state.currentMenu->menuitems[state.itemOn].status == -1);
            return true;

        case KEY_LEFTARROW:
            if (state.currentMenu->menuitems[state.itemOn].routine
                && state.currentMenu->menuitems[state.itemOn].status == 2)
            {
                startSound(nullptr, SfxEnum::Stnmov);
                state.currentMenu->menuitems[state.itemOn].routine(0);
            }
            return true;

        case KEY_RIGHTARROW:
            if (state.currentMenu->menuitems[state.itemOn].routine
                && state.currentMenu->menuitems[state.itemOn].status == 2)
            {
                startSound(nullptr, SfxEnum::Stnmov);
                state.currentMenu->menuitems[state.itemOn].routine(1);
            }
            return true;

        case KEY_ENTER:
            if (state.currentMenu->menuitems[state.itemOn].routine
                && state.currentMenu->menuitems[state.itemOn].status)
            {
                state.currentMenu->lastOn = state.itemOn;
                if (state.currentMenu->menuitems[state.itemOn].status == 2)
                {
                    state.currentMenu->menuitems[state.itemOn].routine(
                        1); // right arrow
                    startSound(nullptr, SfxEnum::Stnmov);
                }
                else
                {
                    state.currentMenu->menuitems[state.itemOn].routine(state.itemOn);
                    startSound(nullptr, SfxEnum::Pistol);
                }
            }
            return true;

        case KEY_ESCAPE:
            state.currentMenu->lastOn = state.itemOn;
            clearMenus();
            startSound(nullptr, SfxEnum::Swtchx);
            return true;

        case KEY_BACKSPACE:
            state.currentMenu->lastOn = state.itemOn;
            if (state.currentMenu->prevMenu)
            {
                state.currentMenu = state.currentMenu->prevMenu;
                state.itemOn = state.currentMenu->lastOn;
                startSound(nullptr, SfxEnum::Swtchn);
            }
            return true;

        default:
            for (int i = state.itemOn + 1; i < state.currentMenu->numitems; i++)
                if (state.currentMenu->menuitems[i].alphaKey == ch)
                {
                    state.itemOn = i;
                    startSound(nullptr, SfxEnum::Pstop);
                    return true;
                }
            for (int i = 0; i <= state.itemOn; i++)
                if (state.currentMenu->menuitems[i].alphaKey == ch)
                {
                    state.itemOn = i;
                    startSound(nullptr, SfxEnum::Pstop);
                    return true;
                }
            break;
    }

    return false;
}

//
// startControlPanel
//
void startControlPanel()
{
    auto& overlay = overlayState();
    auto& state = menuState();

    // intro might call this repeatedly
    if (overlay.menuactive)
        return;

    overlay.menuactive = 1;
    state.currentMenu = &MainDef; // JDC
    state.itemOn = state.currentMenu->lastOn; // JDC
}

//
// drawMenu
// Called after the view has been rendered,
// but before it has been blitted.
//
void drawMenu()
{
    auto& overlay = overlayState();
    auto& state = menuState();

    static short x;
    static short y;
    short i;

    overlay.inhelpscreens = false;

    // Horiz. & Vertically center string and print it.
    if (state.messageToPrint)
    {
        auto message = state.messageString;
        y = 100 - stringHeight(state.messageString) / 2;
        while (!message.empty())
        {
            auto lineEnd = message.find('\n');
            auto line = message.substr(0, lineEnd);
            message.remove_prefix(lineEnd == std::string_view::npos ? message.size()
                                                                    : lineEnd + 1);

            x = 160 - stringWidth(line) / 2;
            writeText(x, y, line);
            y += littleEndian(hudFont().hu_font[0]->height);
        }
        return;
    }

    if (!overlay.menuactive)
        return;

    // Darken background so the menu is more readable.
    if (host().flags & DOOM_FLAG_MENU_DARKEN_BG)
    {
        for (int j = 0, len = SCREENWIDTH * SCREENHEIGHT; j < len; ++j)
        {
            byte color = videoState().screens[0][j];
            color = graphicsData().colormaps[color + (20 * 256)];
            videoState().screens[0][j] = color;
        }
    }

    if (state.currentMenu->routine)
        state.currentMenu->routine(); // call Draw routine

    // DRAW MENU
    x = state.currentMenu->x;
    y = state.currentMenu->y;
    short max = state.currentMenu->numitems;

    for (i = 0; i < max; i++)
    {
        MenuItem* menuitem = state.currentMenu->menuitems + i;
        if (!menuitem->name.empty())
        {
            if (menuitem->name.starts_with("TXT_"))
            {
                drawCustomMenuText(menuitem->name, x, y);
            }
            else
            {
                drawPatchDirect(
                    x, y, 0, static_cast<Patch*>(cacheLumpName(menuitem->name)));
            }
        }
        y += LINEHEIGHT;
    }

    // DRAW SKULL
    drawPatchDirect(x + SKULLXOFF,
                    state.currentMenu->y - 5 + state.itemOn * LINEHEIGHT,
                    0,
                    static_cast<Patch*>(cacheLumpName(skullName[state.whichSkull])));
}

//
// clearMenus
//
void clearMenus()
{
    overlayState().menuactive = 0;
}

//
// setupNextMenu
//
void setupNextMenu(MenuDef* menudef)
{
    auto& state = menuState();

    state.currentMenu = menudef;
    state.itemOn = state.currentMenu->lastOn;
}

//
// menuTicker
//
void menuTicker()
{
    auto& state = menuState();

    if (--state.skullAnimCounter <= 0)
    {
        state.whichSkull ^= 1;
        state.skullAnimCounter = 8;
    }
}

//
// initMenu
//
void initMenu()
{
    auto& overlay = overlayState();
    auto& state = menuState();

    bool hide_mouse = (host().flags & DOOM_FLAG_HIDE_MOUSE_OPTIONS) ? true : false;
    bool hide_sound = ((host().flags & DOOM_FLAG_HIDE_MUSIC_OPTIONS)
                       && (host().flags & DOOM_FLAG_HIDE_SOUND_OPTIONS))
                          ? true
                          : false;

    OptionsMenu = OptionsMenuFull.data();
    if (hide_mouse && !hide_sound)
    {
        OptionsMenu = OptionsMenuNoMouse.data();
        doom_memcpy(&OptionsDef, &OptionsNoMouseDef, sizeof(OptionsDef));
    }
    else if (!hide_mouse && hide_sound)
    {
        OptionsMenu = OptionsMenuNoSound.data();
        doom_memcpy(&OptionsDef, &OptionsNoSoundDef, sizeof(OptionsDef));
    }
    else if (hide_mouse && hide_sound)
    {
        OptionsMenu = OptionsMenuNoSoundNoMouse.data();
        doom_memcpy(&OptionsDef, &OptionsNoSoundNoMouseDef, sizeof(OptionsDef));
    }

    SoundMenu = SoundMenuFull.data();
    if (host().flags & DOOM_FLAG_HIDE_MUSIC_OPTIONS)
    {
        SoundMenu = SoundMenuNoMusic.data();
        doom_memcpy(&SoundDef, &SoundNoMusicDef, sizeof(SoundDef));
    }
    else if (host().flags & DOOM_FLAG_HIDE_SOUND_OPTIONS)
    {
        SoundMenu = SoundMenuNoSFX.data();
        doom_memcpy(&SoundDef, &SoundNoSFXDef, sizeof(SoundDef));
    }

    state.currentMenu = &MainDef;
    overlay.menuactive = 0;
    state.itemOn = state.currentMenu->lastOn;
    state.whichSkull = 0;
    state.skullAnimCounter = 10;
    state.screenSize = menuSettings().screenblocks - 3;
    state.messageToPrint = 0;
    state.messageString = {};
    state.messageLastMenuActive = overlay.menuactive;
    state.quickSaveSlot = -1;

    // Here we could catch other version dependencies,
    //  like HELP1/2, and four episodes.

    switch (gameVersion().gamemode)
    {
        case GameMode::Commercial:
            // This is used because DOOM 2 had only one HELP
            //  page. I use CREDIT as second page now, but
            //  kept this hack for educational purposes.
            MainMenu[toIndex(MainItem::ReadThis)] =
                MainMenu[toIndex(MainItem::QuitDoom)];
            MainDef.numitems--;
            MainDef.y += 8;
            NewDef.prevMenu = &MainDef;
            ReadDef1.routine = drawReadThis1;
            ReadDef1.x = 330;
            ReadDef1.y = 165;
            ReadMenu1[0].routine = finishReadThis;
            break;
        case GameMode::Shareware:
            // Episode 2 and 3 are handled,
            //  branching to an ad screen.
        case GameMode::Registered:
            // We need to remove the fourth episode.
            EpiDef.numitems--;
            break;
        case GameMode::Retail:
            // We are fine.
        case GameMode::Indetermined:
            break;
    }
}
} // namespace Doom
