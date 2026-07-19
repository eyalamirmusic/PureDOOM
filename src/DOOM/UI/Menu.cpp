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
#include <ea_data_structures/Structures/Array.h>

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
// is declared in doomstat.h; messageToPrint (m_menu.h) is read by the eacp overlay
// capture.
// menuactive, automapactive and inhelpscreens (which gates Doom::displayFrame's border
// redraw) are a Doom::OverlayState owned by the Engine now; these are references
// onto it (REFACTOR.md, Step 5).
// The config-backed settings (mouse sensitivity, message toggle, detail, view
// size) are Engine members now (UI/MenuSettings.h); these are references onto
// them. Config.cpp binds its defaults[] entries to the members at runtime rather
// than capturing their addresses at static-init, which is what unblocked the move.
int messageToPrint; // 1 = message to be printed

// Globals owned elsewhere that the menu reads or writes. They are declared here,
// at file scope, so the uses inside namespace Doom below resolve to these ::
// entities - a matching extern placed *inside* the namespace would instead be
// taken for a Doom:: member and fail to link (the trap the Step 8 notes call
// out). screens (v_video.h) and colormaps (r_state.h) already come in through
// headers; screen_palette has no header, so it is declared here.
extern int doom_flags;
extern unsigned char screen_palette[256 * 3]; // i_video, no header

// The quit-screen taunts, drawn by quitDOOM. Declared in dstrings.h, read only
// here, so their definition moved out of dstrings.cpp to sit with their one
// reader. ::-scoped for the extern; const so the literals stay off -Wwritable.
const char* endmsg[Doom::NUM_QUITMESSAGES + 1] = {
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

    char name[10];

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
    const char* lump;
    int x, w;
    int offx;
    int offy;
};

struct MenuCustomText
{
    const char* name;
    MenuCustomTextSeg segs[16];
};

// The menu's transient interaction state lives on the Engine (UI/MenuState.h). Every function
// below reaches it through a hoisted local instead of a file-scope alias (REFACTOR.md, Step 9
// strand (a)). The immutable reference-data tables interspersed among them (gammamsg / skullName /
// detailNames / msgNames / quitsounds / menu_custom_texts) and the self-referential menu-definition
// apparatus further down stay file-local.

// Was EA::Array<EA::Array<char, 26>, 5>, a fixed 26-byte buffer per message that
// only existed because these were string-literal macros. Its one reader wants a
// const char*, so it holds pointers now and the 26 cannot silently truncate.
EA::Array<const char*, 5> gammamsg = {GAMMALVL0,
                                      GAMMALVL1,
                                      GAMMALVL2,
                                      GAMMALVL3,
                                      GAMMALVL4};

// graphic name of skulls
// warning: initializer-string for array of chars is too long
EA::Array<EA::Array<char, /*8*/ 9>, 2> skullName = {
    EA::Array<char, 9> {{"M_SKULL1"}}, EA::Array<char, 9> {{"M_SKULL2"}}};

// The menu-definition tables below lean on partial aggregate initializers on
// purpose: a {0} terminates a custom-text segment list, and a {-1,"",0} marks a
// non-selectable spacer row. Both leave trailing fields defaulted, which is what
// they mean - so the warning about it is silenced just over the data.
DOOM_DIAGNOSTIC_PUSH
DOOM_IGNORE_MISSING_FIELD_INITIALIZERS

// We create new menu text by cutting into existing graphics and pasting them to create the new text.
// This way we don't ship code with embeded graphics that come from WAD files.
EA::Array<MenuCustomText, 4> menu_custom_texts = {
    {"TXT_MMOV",
     {{"M_MSENS", 0, 74, 0, 0}, // Mouse
      {"M_MSENS", 0, 31, 83, 0}, // Mo
      {"M_MSENS", 160, 14, 83 + 31, 0}, // v
      {"M_MSENS", 60, 14, 83 + 31 + 14, 0}, // e
      {"M_DETAIL", 169, 5, 83 + 31 + 14 + 14, 0}, // :
      {0}}},
    {"TXT_MOPT",
     {{"M_MSENS", 0, 74, 0, 0}, // Mouse
      {"M_OPTION", 0, 92, 74 + 9, 0}, // Options
      {0}}},
    {"TXT_CROS",
     {{"M_SKILL", 0, 16, 0, 0}, // C
      {"M_DETAIL", 14, 15, 16, 0}, // r
      {"M_SKILL", 46, 30, 16 + 15, 0}, // os
      {"M_SKILL", 62, 14, 16 + 15 + 30, 0}, // s
      {"M_SKILL", 16, 15, 16 + 15 + 30 + 14, 0}, // h
      {"M_DETAIL", 140, 19, 16 + 15 + 30 + 14 + 15, 0}, // ai
      {"M_DETAIL", 14, 15, 16 + 15 + 30 + 14 + 15 + 19, 0}, // r
      {"M_DETAIL", 169, 5, 16 + 15 + 30 + 14 + 15 + 19 + 15, 0}, // :
      {0}}},
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
      {0}}},
};

const int custom_texts_count = sizeof(menu_custom_texts) / sizeof(MenuCustomText);

EA::Array<EA::Array<char, 9>, 2> detailNames = {EA::Array<char, 9> {{"M_GDHIGH"}},
                                                EA::Array<char, 9> {{"M_GDLOW"}}};
EA::Array<EA::Array<char, 9>, 2> msgNames = {EA::Array<char, 9> {{"M_MSGOFF"}},
                                             EA::Array<char, 9> {{"M_MSGON"}}};

EA::Array<int, 8> quitsounds = {sfx_pldeth,
                                sfx_dmpain,
                                sfx_popain,
                                sfx_slop,
                                sfx_telept,
                                sfx_posit1,
                                sfx_posit3,
                                sfx_sgtatk};

EA::Array<int, 8> quitsounds2 = {sfx_vilact,
                                 sfx_getpow,
                                 sfx_boscub,
                                 sfx_slop,
                                 sfx_skeswg,
                                 sfx_kntdth,
                                 sfx_bspact,
                                 sfx_sgtatk};

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
void writeText(int x, int y, const char* string);
int stringWidth(const char* string);
int stringHeight(const char* string);
void startControlPanel();
void startMessage(const char* string, std::function<void(int)> routine, bool input);
void stopMessage();
void clearMenus();
void drawMouseOptions();

//
// DOOM MENU
//
enum
{
    newgame = 0,
    options,
    loadgame,
    savegame,
    readthis,
    quitdoom,
    main_end
};

EA::Array<MenuItem, 6> MainMenu = {{1, "M_NGAME", newGame, 'n'},
                                   {1, "M_OPTION", optionsMenu, 'o'},
                                   {1, "M_LOADG", loadGameMenu, 'l'},
                                   {1, "M_SAVEG", saveGameMenu, 's'},
                                   // Another hickup with Special edition.
                                   {1, "M_RDTHIS", readThis, 'r'},
                                   {1, "M_QUITG", quitDOOM, 'q'}};

MenuDef MainDef = {main_end, 0, MainMenu.data(), drawMainMenu, 97, 64, 0};

//
// EPISODE SELECT
//
enum
{
    ep1,
    ep2,
    ep3,
    ep4,
    ep_end
};

EA::Array<MenuItem, 4> EpisodeMenu = {{1, "M_EPI1", episode, 'k'},
                                      {1, "M_EPI2", episode, 't'},
                                      {1, "M_EPI3", episode, 'i'},
                                      {1, "M_EPI4", episode, 't'}};

MenuDef EpiDef = {
    ep_end, // # of menu items
    &MainDef, // previous menu
    EpisodeMenu.data(), // MenuItem ->
    drawEpisode, // drawing routine ->
    48,
    63, // x,y
    ep1 // lastOn
};

//
// NEW GAME
//
enum
{
    killthings,
    toorough,
    hurtme,
    violence,
    nightmare,
    newg_end
};

EA::Array<MenuItem, 5> NewGameMenu = {{1, "M_JKILL", chooseSkill, 'i'},
                                      {1, "M_ROUGH", chooseSkill, 'h'},
                                      {1, "M_HURT", chooseSkill, 'h'},
                                      {1, "M_ULTRA", chooseSkill, 'u'},
                                      {1, "M_NMARE", chooseSkill, 'n'}};

MenuDef NewDef = {
    newg_end, // # of menu items
    &EpiDef, // previous menu
    NewGameMenu.data(), // MenuItem ->
    drawNewGame, // drawing routine ->
    48,
    63, // x,y
    hurtme // lastOn
};

//
// OPTIONS MENU
//
MenuItem* OptionsMenu;

enum
{
    endgame,
    messages,
    crosshair_opt,
    always_run_opt,
    //detail, // Details do nothing?
    scrnsize,
    option_empty1,
    mouseoptions,
    soundvol,
    opt_end
};

EA::Array<MenuItem, 8> OptionsMenuFull = {
    {1, "M_ENDGAM", endGame, 'e'},
    {1, "M_MESSG", changeMessages, 'm'},
    {1, "TXT_CROS", changeCrosshair, 'c'},
    {1, "TXT_ARUN", changeAlwaysRun, 'r'},
    //{1,"M_DETAIL", changeDetail,'g'},  // Details do nothing?
    {2, "M_SCRNSZ", sizeDisplay, 's'},
    {-1, "", 0},
    {1, "TXT_MOPT", mouseOptions, 'f'},
    {1, "M_SVOL", sound, 's'}};

MenuDef OptionsDef = {
    opt_end, &MainDef, OptionsMenuFull.data(), drawOptions, 60, 37, 0};

enum
{
    endgame_no_mouse,
    messages_no_mouse,
    crosshair_opt_no_mouse,
    always_run_opt_no_mouse,
    //detail_no_mouse, // Details do nothing?
    scrnsize_no_mouse,
    option_empty1_no_mouse,
    soundvol_no_mouse,
    opt_end_no_mouse
};

EA::Array<MenuItem, 7> OptionsMenuNoMouse = {
    {1, "M_ENDGAM", endGame, 'e'},
    {1, "M_MESSG", changeMessages, 'm'},
    {1, "TXT_CROS", changeCrosshair, 'c'},
    {1, "TXT_ARUN", changeAlwaysRun, 'r'},
    //{1,"M_DETAIL",  changeDetail,'g'}, // Details do nothing?
    {2, "M_SCRNSZ", sizeDisplay, 's'},
    {-1, "", 0},
    {1, "M_SVOL", sound, 's'}};

MenuDef OptionsNoMouseDef = {
    opt_end_no_mouse, &MainDef, OptionsMenuNoMouse.data(), drawOptions, 60, 37, 0};

enum
{
    endgame_no_sound,
    messages_no_sound,
    crosshair_opt_no_sound,
    always_run_opt_no_sound,
    //detail_no_sound, // Details do nothing?
    scrnsize_no_sound,
    option_empty1_no_sound,
    mouseoptions_no_sound,
    opt_end_no_sound
};

EA::Array<MenuItem, 7> OptionsMenuNoSound = {
    {1, "M_ENDGAM", endGame, 'e'},
    {1, "M_MESSG", changeMessages, 'm'},
    {1, "TXT_CROS", changeCrosshair, 'c'},
    {1, "TXT_ARUN", changeAlwaysRun, 'r'},
    //{1,"M_DETAIL",  changeDetail,'g'}, // Details do nothing?
    {2, "M_SCRNSZ", sizeDisplay, 's'},
    {-1, "", 0},
    {1, "TXT_MOPT", mouseOptions, 'f'}};

MenuDef OptionsNoSoundDef = {
    opt_end_no_sound, &MainDef, OptionsMenuNoSound.data(), drawOptions, 60, 37, 0};

enum
{
    endgame_no_sound_no_mouse,
    messages_no_sound_no_mouse,
    crosshair_opt_no_sound_no_mouse,
    always_run_top_no_sound_no_mouse,
    //detail_no_sound_no_mouse, // Details do nothing?
    scrnsize_no_sound_no_mouse,
    option_empty1_no_sound_no_mouse,
    opt_end_no_sound_no_mouse
};

EA::Array<MenuItem, 6> OptionsMenuNoSoundNoMouse = {
    {1, "M_ENDGAM", endGame, 'e'},
    {1, "M_MESSG", changeMessages, 'm'},
    {1, "TXT_CROS", changeCrosshair, 'c'},
    {1, "TXT_ARUN", changeAlwaysRun, 'r'},
    //{1,"M_DETAIL",  changeDetail,'g'}, // Details do nothing?
    {2, "M_SCRNSZ", sizeDisplay, 's'},
    {-1, "", 0}};

MenuDef OptionsNoSoundNoMouseDef = {opt_end_no_sound_no_mouse,
                                    &MainDef,
                                    OptionsMenuNoSoundNoMouse.data(),
                                    drawOptions,
                                    60,
                                    37,
                                    0};

//
// MOUSE OPTIONS
//
enum
{
    mousemov,
    mousesens,
    mouse_option_empty1,
    mouse_opt_end
};

EA::Array<MenuItem, 3> MouseOptionsMenu = {
    {1, "TXT_MMOV", mouseMove, 'f'},
    {2, "M_MSENS", changeSensitivity, 'm'},
    {-1, "", 0},
};

MenuDef MouseOptionsDef = {mouse_opt_end,
                           &OptionsDef,
                           MouseOptionsMenu.data(),
                           drawMouseOptions,
                           60,
                           70,
                           0};

//
// Read This! MENU 1 & 2
//
enum
{
    rdthsempty1,
    read1_end
};

EA::Array<MenuItem, 1> ReadMenu1 = {{1, "", readThis2, 0}};

MenuDef ReadDef1 = {
    read1_end, &MainDef, ReadMenu1.data(), drawReadThis1, 280, 185, 0};

enum
{
    rdthsempty2,
    read2_end
};

EA::Array<MenuItem, 1> ReadMenu2 = {{1, "", finishReadThis, 0}};

MenuDef ReadDef2 = {
    read2_end, &ReadDef1, ReadMenu2.data(), drawReadThis2, 330, 175, 0};

//
// SOUND VOLUME MENU
//
MenuItem* SoundMenu;

enum
{
    sfx_vol,
    sfx_empty1,
    music_vol,
    sfx_empty2,
    sound_end
};

EA::Array<MenuItem, 4> SoundMenuFull = {{2, "M_SFXVOL", sfxVol, 's'},
                                        {-1, "", 0},
                                        {2, "M_MUSVOL", musicVol, 'm'},
                                        {-1, "", 0}};

MenuDef SoundDef = {
    sound_end, &OptionsDef, SoundMenuFull.data(), drawSound, 80, 64, 0};

enum
{
    music_vol_no_sfx,
    sfx_empty2_no_sfx,
    sound_end_no_sfx
};

EA::Array<MenuItem, 2> SoundMenuNoSFX = {{2, "M_MUSVOL", musicVol, 'm'},
                                         {-1, "", 0}};

MenuDef SoundNoSFXDef = {
    sound_end_no_sfx, &OptionsDef, SoundMenuNoSFX.data(), drawSound, 80, 64, 0};

enum
{
    sfx_vol_no_music,
    sfx_empty1_no_music,
    sound_end_no_music
};

EA::Array<MenuItem, 2> SoundMenuNoMusic = {{2, "M_SFXVOL", sfxVol, 's'},
                                           {-1, "", 0}};

MenuDef SoundNoMusicDef = {
    sound_end_no_music, &OptionsDef, SoundMenuNoMusic.data(), drawSound, 80, 64, 0};

//
// LOAD GAME MENU
//
enum
{
    load1,
    load2,
    load3,
    load4,
    load5,
    load6,
    load_end
};

EA::Array<MenuItem, 6> DOOM_LoadMenu = {{1, "", loadSelect, '1'},
                                        {1, "", loadSelect, '2'},
                                        {1, "", loadSelect, '3'},
                                        {1, "", loadSelect, '4'},
                                        {1, "", loadSelect, '5'},
                                        {1, "", loadSelect, '6'}};

MenuDef LoadDef = {load_end, &MainDef, DOOM_LoadMenu.data(), drawLoad, 80, 54, 0};

//
// SAVE GAME MENU
//
EA::Array<MenuItem, 6> SaveMenu = {{1, "", saveSelect, '1'},
                                   {1, "", saveSelect, '2'},
                                   {1, "", saveSelect, '3'},
                                   {1, "", saveSelect, '4'},
                                   {1, "", saveSelect, '5'},
                                   {1, "", saveSelect, '6'}};

MenuDef SaveDef = {load_end, &MainDef, SaveMenu.data(), drawSave, 80, 54, 0};

DOOM_DIAGNOSTIC_POP

//
// drawCustomMenuText
//  Draw several segments of patches to make up new text
//
void drawCustomMenuText(const char* name, int x, int y)
{
    for (int i = 0; i < custom_texts_count; ++i)
    {
        MenuCustomText* custom_text = menu_custom_texts.data() + i;
        if (doom_strcmp(custom_text->name, name) == 0)
        {
            MenuCustomTextSeg* seg = custom_text->segs;
            while (seg->lump)
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
    EA::Array<char, 256> name;

    for (int i = 0; i < load_end; i++)
    {
#if 0
        if (Doom::checkParm("-cdrom"))
            //doom_sprintf(name, "c:\\doomdata\\" SAVEGAMENAME "%d.dsg", i);
        else
#endif
        {
            //doom_sprintf(name, SAVEGAMENAME"%d.dsg", i);
            doom_strcpy(name.data(), SAVEGAMENAME);
            doom_concat(name.data(), doom_itoa(i, 10));
            doom_concat(name.data(), ".dsg");
        }

        handle = doom_open(name.data(), "r");
        if (handle == nullptr)
        {
            doom_strcpy(&state.savegamestrings[i][0], EMPTYSTRING);
            DOOM_LoadMenu[i].status = 0;
            continue;
        }
        doom_read(handle, &state.savegamestrings[i], menuSaveStringSize);
        doom_close(handle);
        DOOM_LoadMenu[i].status = 1;
    }
}

//
// loadGameMenu & Cie.
//
void drawLoad()
{
    auto& state = menuState();

    drawPatchDirect(
        72, 28, 0, static_cast<Patch*>(cacheLumpName("M_LOADG")));
    for (int i = 0; i < load_end; i++)
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
    drawPatchDirect(
        x - 8, y + 7, 0, static_cast<Patch*>(cacheLumpName("M_LSLEFT")));

    for (int i = 0; i < 24; i++)
    {
        drawPatchDirect(
            x, y + 7, 0, static_cast<Patch*>(cacheLumpName("M_LSCNTR")));
        x += 8;
    }

    drawPatchDirect(
        x, y + 7, 0, static_cast<Patch*>(cacheLumpName("M_LSRGHT")));
}

//
// User wants to load this game
//
void loadSelect(int choice)
{
    EA::Array<char, 256> name;

#if 0
    if (Doom::checkParm("-cdrom"))
        //doom_sprintf(name, "c:\\doomdata\\"SAVEGAMENAME"%d.dsg", choice);
    else
#endif
    {
        //doom_sprintf(name, SAVEGAMENAME"%d.dsg", choice);
        doom_strcpy(name.data(), SAVEGAMENAME);
        doom_concat(name.data(), doom_itoa(choice, 10));
        doom_concat(name.data(), ".dsg");
    }
    loadGame(name.data());
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

    drawPatchDirect(
        72, 28, 0, static_cast<Patch*>(cacheLumpName("M_SAVEG")));
    for (i = 0; i < load_end; i++)
    {
        drawSaveLoadBorder(LoadDef.x, LoadDef.y + LINEHEIGHT * i);
        writeText(
            LoadDef.x, LoadDef.y + LINEHEIGHT * i, state.savegamestrings[i].data());
    }

    if (state.saveStringEnter)
    {
        i = stringWidth(state.savegamestrings[state.saveSlot].data());
        writeText(LoadDef.x + i, LoadDef.y + LINEHEIGHT * state.saveSlot, "_");
    }
}

//
// menuResponder calls this when user is finished
//
void doSave(int slot)
{
    auto& state = menuState();

    saveGame(slot, state.savegamestrings[slot].data());
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
    doom_strcpy(state.saveOldString.data(), state.savegamestrings[choice].data());
    if (!doom_strcmp(state.savegamestrings[choice].data(), EMPTYSTRING))
        state.savegamestrings[choice][0] = 0;
    state.saveCharIndex =
        static_cast<int>(doom_strlen(state.savegamestrings[choice].data()));
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

    if (gameFlow().gamestate != GS_LEVEL)
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
        startSound(0, sfx_swtchx);
    }
}

void quickSave()
{
    auto& state = menuState();

    if (!demoState().usergame)
    {
        startSound(0, sfx_oof);
        return;
    }

    if (gameFlow().gamestate != GS_LEVEL)
        return;

    if (state.quickSaveSlot < 0)
    {
        startControlPanel();
        readSaveStrings();
        setupNextMenu(&SaveDef);
        state.quickSaveSlot = -2; // means to pick a slot now
        return;
    }
    //doom_sprintf(tempstring, Doom::QSPROMPT, savegamestrings[quickSaveSlot]);
    doom_strcpy(state.tempstring.data(), QSPROMPT_1);
    doom_concat(state.tempstring.data(),
                state.savegamestrings[state.quickSaveSlot].data());
    doom_strcpy(state.tempstring.data(), QSPROMPT_2);
    startMessage(state.tempstring.data(), quickSaveResponse, true);
}

//
// quickLoad
//
void quickLoadResponse(int ch)
{
    if (ch == 'y')
    {
        loadSelect(menuState().quickSaveSlot);
        startSound(0, sfx_swtchx);
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
    //doom_sprintf(tempstring, Doom::QLPROMPT, savegamestrings[quickSaveSlot]);
    doom_strcpy(state.tempstring.data(), QLPROMPT_1);
    doom_concat(state.tempstring.data(),
                state.savegamestrings[state.quickSaveSlot].data());
    doom_strcpy(state.tempstring.data(), QLPROMPT_2);
    startMessage(state.tempstring.data(), quickLoadResponse, true);
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
        case commercial:
            drawPatchDirect(
                0, 0, 0, static_cast<Patch*>(cacheLumpName("HELP")));
            break;
        case shareware:
        case registered:
        case retail:
            drawPatchDirect(
                0, 0, 0, static_cast<Patch*>(cacheLumpName("HELP1")));
            break;
        default:
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
        case retail:
        case commercial:
            // This hack keeps us from having to change menus.
            drawPatchDirect(
                0, 0, 0, static_cast<Patch*>(cacheLumpName("CREDIT")));
            break;
        case shareware:
        case registered:
            drawPatchDirect(
                0, 0, 0, static_cast<Patch*>(cacheLumpName("HELP2")));
            break;
        default:
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

    drawPatchDirect(
        60, 38, 0, static_cast<Patch*>(cacheLumpName("M_SVOL")));

    if (!(doom_flags & DOOM_FLAG_HIDE_SOUND_OPTIONS))
    {
        int offset = (doom_flags & DOOM_FLAG_HIDE_MUSIC_OPTIONS)
                         ? static_cast<int>(sfx_vol_no_music)
                         : static_cast<int>(sfx_vol);
        drawThermo(SoundDef.x,
                   SoundDef.y + LINEHEIGHT * (offset + 1),
                   16,
                   sndset.sfxVolume);
    }

    if (!(doom_flags & DOOM_FLAG_HIDE_MUSIC_OPTIONS))
    {
        int offset = (doom_flags & DOOM_FLAG_HIDE_SOUND_OPTIONS)
                         ? static_cast<int>(music_vol_no_sfx)
                         : static_cast<int>(music_vol);
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
    drawPatchDirect(
        94, 2, 0, static_cast<Patch*>(cacheLumpName("M_DOOM")));
}

//
// newGame
//
void drawNewGame()
{
    drawPatchDirect(
        96, 14, 0, static_cast<Patch*>(cacheLumpName("M_NEWG")));
    drawPatchDirect(
        54, 38, 0, static_cast<Patch*>(cacheLumpName("M_SKILL")));
}

void newGame(int)
{
    if (gameSession().netgame && !demoState().demoplayback)
    {
        startMessage(NEWGAME, {}, false);
        return;
    }

    if (gameVersion().gamemode == commercial)
        setupNextMenu(&NewDef);
    else
        setupNextMenu(&EpiDef);
}

//
// episode
//
void drawEpisode()
{
    drawPatchDirect(
        54, 38, 0, static_cast<Patch*>(cacheLumpName("M_EPISOD")));
}

void verifyNightmare(int ch)
{
    if (ch != 'y')
        return;

    deferInitNew(static_cast<Skill>(nightmare), menuState().epi + 1, 1);
    clearMenus();
}

void chooseSkill(int choice)
{
    if (choice == nightmare)
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

    if ((version.gamemode == shareware) && choice)
    {
        startMessage(SWSTRING, {}, false);
        setupNextMenu(&ReadDef1);
        return;
    }

    // Yet another hack...
    if ((version.gamemode == registered) && (choice > 2))
    {
        doom_print("episode: 4th episode requires UltimateDOOM\n");
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

    drawPatchDirect(
        108, 15, 0, static_cast<Patch*>(cacheLumpName("M_OPTTTL")));

    //Doom::drawPatchDirect (OptionsDef.x + 175,OptionsDef.y+LINEHEIGHT*detail,0,
    //                Doom::cacheLumpName(detailNames[detailLevel])); // Details do nothing?

    drawPatchDirect(OptionsDef.x + 120,
                          OptionsDef.y + LINEHEIGHT * messages,
                          0,
                          static_cast<Patch*>(cacheLumpName(
                              msgNames[menuSettings().showMessages].data())));

    drawPatchDirect(
        OptionsDef.x + 131,
        OptionsDef.y + LINEHEIGHT * crosshair_opt,
        0,
        static_cast<Patch*>(cacheLumpName(msgNames[input.crosshair].data())));

    drawPatchDirect(
        OptionsDef.x + 147,
        OptionsDef.y + LINEHEIGHT * always_run_opt,
        0,
        static_cast<Patch*>(cacheLumpName(msgNames[input.always_run].data())));

    drawThermo(OptionsDef.x,
               OptionsDef.y + LINEHEIGHT * (scrnsize + 1),
               9,
               menuState().screenSize);
}

void drawMouseOptions()
{
    drawCustomMenuText("TXT_MOPT", 74, 45);

    drawPatchDirect(MouseOptionsDef.x + 149,
                          MouseOptionsDef.y + LINEHEIGHT * mousemov,
                          0,
                          static_cast<Patch*>(cacheLumpName(
                              msgNames[inputConfig().mousemove].data())));

    drawThermo(MouseOptionsDef.x,
               MouseOptionsDef.y + LINEHEIGHT * (mousesens + 1),
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
        startSound(0, sfx_oof);
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
        if (gameVersion().gamemode == commercial)
            startSound(0, quitsounds2[(clock.gametic >> 2) & 7]);
        else
            startSound(0, quitsounds[(clock.gametic >> 2) & 7]);
        waitVBlank(105);
    }
    quitGame();
}

void quitDOOM(int)
{
    auto& state = menuState();

    // We pick index 0 which is language sensitive,
    //  or one at random, between 1 and maximum number.
    if (gameVersion().language != english)
    {
        //doom_sprintf(endstring, "%s\n\n"DOSY, endmsg[0]);
        doom_strcpy(state.endstring.data(), endmsg[0]);
        doom_concat(state.endstring.data(), "\n\n" DOSY);
    }
    else
    {
        //doom_sprintf(endstring, "%s\n\n" DOSY, endmsg[gametic % (NUM_QUITMESSAGES - 2) + 1]);
        doom_strcpy(state.endstring.data(),
                    endmsg[gameClock().gametic % (NUM_QUITMESSAGES - 2) + 1]);
        doom_concat(state.endstring.data(), "\n\n" DOSY);
    }

    startMessage(state.endstring.data(), quitResponse, true);
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
    doom_print("changeDetail: low detail mode n.a.\n");
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
    drawPatchDirect(
        xx, y, 0, static_cast<Patch*>(cacheLumpName("M_THERML")));
    xx += 8;
    for (int i = 0; i < thermWidth; i++)
    {
        drawPatchDirect(
            xx, y, 0, static_cast<Patch*>(cacheLumpName("M_THERMM")));
        xx += 8;
    }
    drawPatchDirect(
        xx, y, 0, static_cast<Patch*>(cacheLumpName("M_THERMR")));

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

void startMessage(const char* string, std::function<void(int)> routine, bool input)
{
    auto& overlay = overlayState();
    auto& state = menuState();

    state.messageLastMenuActive = overlay.menuactive;
    messageToPrint = 1;
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
    messageToPrint = 0;
}

//
// Find string width from hu_font chars
//
int stringWidth(const char* string)
{
    int w = 0;

    for (int i = 0; i < doom_strlen(string); i++)
    {
        int c = doom_toupper(string[i]) - HU_FONTSTART;
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
int stringHeight(const char* string)
{
    int height = littleEndian(hudFont().hu_font[0]->height);

    int h = height;
    for (int i = 0; i < doom_strlen(string); i++)
        if (string[i] == '\n')
            h += height;

    return h;
}

//
// Write a string using the hu_font
//
void writeText(int x, int y, const char* string)
{
    auto& font = hudFont();

    const char* ch = string;
    int cx = x;
    int cy = y;

    while (1)
    {
        int c = *ch++;
        if (!c)
            break;
        if (c == '\n')
        {
            cx = x;
            cy += 12;
            continue;
        }

        c = doom_toupper(c) - HU_FONTSTART;
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
bool menuResponder(Event* ev)
{
    auto& overlay = overlayState();
    auto& players_ = playerState();
    auto& settings = menuSettings();

    auto& state = menuState();

    int ch = -1;

    if (ev->type == ev_joystick && state.joywait < currentTic())
    {
        if (ev->data3 == -1)
        {
            ch = KEY_UPARROW;
            state.joywait = currentTic() + 5;
        }
        else if (ev->data3 == 1)
        {
            ch = KEY_DOWNARROW;
            state.joywait = currentTic() + 5;
        }

        if (ev->data2 == -1)
        {
            ch = KEY_LEFTARROW;
            state.joywait = currentTic() + 2;
        }
        else if (ev->data2 == 1)
        {
            ch = KEY_RIGHTARROW;
            state.joywait = currentTic() + 2;
        }

        if (ev->data1 & 1)
        {
            ch = KEY_ENTER;
            state.joywait = currentTic() + 5;
        }
        if (ev->data1 & 2)
        {
            ch = KEY_BACKSPACE;
            state.joywait = currentTic() + 5;
        }
    }
    else
    {
        if (ev->type == ev_mouse && state.mousewait < currentTic())
        {
            state.mousey += ev->data3;
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

            state.mousex += ev->data2;
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

            if (ev->data1 & 1)
            {
                ch = KEY_ENTER;
                state.mousewait = currentTic() + 15;
            }

            if (ev->data1 & 2)
            {
                ch = KEY_BACKSPACE;
                state.mousewait = currentTic() + 15;
            }
        }
        else if (ev->type == ev_keydown)
        {
            ch = ev->data1;
        }
    }

    if (ch == -1)
        return false;

    // Save Game string input
    if (state.saveStringEnter)
    {
        switch (ch)
        {
            case KEY_BACKSPACE:
                if (state.saveCharIndex > 0)
                {
                    state.saveCharIndex--;
                    state.savegamestrings[state.saveSlot][state.saveCharIndex] = 0;
                }
                break;

            case KEY_ESCAPE:
                state.saveStringEnter = 0;
                doom_strcpy(&state.savegamestrings[state.saveSlot][0],
                            state.saveOldString.data());
                break;

            case KEY_ENTER:
                state.saveStringEnter = 0;
                if (state.savegamestrings[state.saveSlot][0])
                    doSave(state.saveSlot);
                break;

            default:
                ch = doom_toupper(ch);
                if (ch != 32)
                    if (ch - HU_FONTSTART < 0 || ch - HU_FONTSTART >= HU_FONTSIZE)
                        break;
                if (ch >= 32 && ch <= 127
                    && state.saveCharIndex < menuSaveStringSize - 1
                    && stringWidth(state.savegamestrings[state.saveSlot].data())
                           < (menuSaveStringSize - 2) * 8)
                {
                    state.savegamestrings[state.saveSlot][state.saveCharIndex++] =
                        ch;
                    state.savegamestrings[state.saveSlot][state.saveCharIndex] = 0;
                }
                break;
        }
        return true;
    }

    // Take care of any messages that need input
    if (messageToPrint)
    {
        if (state.messageNeedsInput == true
            && !(ch == ' ' || ch == 'n' || ch == 'y' || ch == KEY_ESCAPE))
            return false;

        overlay.menuactive = state.messageLastMenuActive;
        messageToPrint = 0;
        state.messageRoutine(ch);

        overlay.menuactive = false;
        startSound(0, sfx_swtchx);
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
                startSound(0, sfx_stnmov);
                return true;

            case KEY_EQUALS: // Screen size up
                if (overlayState().automapactive || hudFlags().chat_on)
                    return false;
                sizeDisplay(1);
                startSound(0, sfx_stnmov);
                return true;

            case KEY_F1: // Help key
                startControlPanel();

                if (gameVersion().gamemode == retail)
                    state.currentMenu = &ReadDef2;
                else
                    state.currentMenu = &ReadDef1;

                state.itemOn = 0;
                startSound(0, sfx_swtchn);
                return true;

            case KEY_F2: // Save
                startControlPanel();
                startSound(0, sfx_swtchn);
                saveGameMenu(0);
                return true;

            case KEY_F3: // Load
                startControlPanel();
                startSound(0, sfx_swtchn);
                loadGameMenu(0);
                return true;

            case KEY_F4: // Sound Volume
                startControlPanel();
                state.currentMenu = &SoundDef;
                state.itemOn = sfx_vol;
                startSound(0, sfx_swtchn);
                return true;

                // case KEY_F5:            // Detail toggle
                //     changeDetail(0);
                //     Doom::startSound(0, sfx_swtchn);
                //     return true;

            case KEY_F5: // Crosshair toggle
                changeCrosshair(0);
                startSound(0, sfx_swtchn);
                return true;

            case KEY_F6: // Quicksave
                startSound(0, sfx_swtchn);
                quickSave();
                return true;

            case KEY_F7: // End game
                startSound(0, sfx_swtchn);
                endGame(0);
                return true;

            case KEY_F8: // Toggle messages
                changeMessages(0);
                startSound(0, sfx_swtchn);
                return true;

            case KEY_F9: // Quickload
                startSound(0, sfx_swtchn);
                quickLoad();
                return true;

            case KEY_F10: // Quit DOOM
                startSound(0, sfx_swtchn);
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
            startSound(0, sfx_swtchn);
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
                startSound(0, sfx_pstop);
            } while (state.currentMenu->menuitems[state.itemOn].status == -1);
            return true;

        case KEY_UPARROW:
            do
            {
                if (!state.itemOn)
                    state.itemOn = state.currentMenu->numitems - 1;
                else
                    state.itemOn--;
                startSound(0, sfx_pstop);
            } while (state.currentMenu->menuitems[state.itemOn].status == -1);
            return true;

        case KEY_LEFTARROW:
            if (state.currentMenu->menuitems[state.itemOn].routine
                && state.currentMenu->menuitems[state.itemOn].status == 2)
            {
                startSound(0, sfx_stnmov);
                state.currentMenu->menuitems[state.itemOn].routine(0);
            }
            return true;

        case KEY_RIGHTARROW:
            if (state.currentMenu->menuitems[state.itemOn].routine
                && state.currentMenu->menuitems[state.itemOn].status == 2)
            {
                startSound(0, sfx_stnmov);
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
                    startSound(0, sfx_stnmov);
                }
                else
                {
                    state.currentMenu->menuitems[state.itemOn].routine(state.itemOn);
                    startSound(0, sfx_pistol);
                }
            }
            return true;

        case KEY_ESCAPE:
            state.currentMenu->lastOn = state.itemOn;
            clearMenus();
            startSound(0, sfx_swtchx);
            return true;

        case KEY_BACKSPACE:
            state.currentMenu->lastOn = state.itemOn;
            if (state.currentMenu->prevMenu)
            {
                state.currentMenu = state.currentMenu->prevMenu;
                state.itemOn = state.currentMenu->lastOn;
                startSound(0, sfx_swtchn);
            }
            return true;

        default:
            for (int i = state.itemOn + 1; i < state.currentMenu->numitems; i++)
                if (state.currentMenu->menuitems[i].alphaKey == ch)
                {
                    state.itemOn = i;
                    startSound(0, sfx_pstop);
                    return true;
                }
            for (int i = 0; i <= state.itemOn; i++)
                if (state.currentMenu->menuitems[i].alphaKey == ch)
                {
                    state.itemOn = i;
                    startSound(0, sfx_pstop);
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
    EA::Array<char, 40> string;

    overlay.inhelpscreens = false;

    // Horiz. & Vertically center string and print it.
    if (messageToPrint)
    {
        int start = 0;
        y = 100 - stringHeight(state.messageString) / 2;
        while (*(state.messageString + start))
        {
            for (i = 0; i < doom_strlen(state.messageString + start); i++)
                if (*(state.messageString + start + i) == '\n')
                {
                    doom_memset(string.data(), 0, 40);
                    doom_strncpy(string.data(), state.messageString + start, i);
                    start += i + 1;
                    break;
                }

            if (i == doom_strlen(state.messageString + start))
            {
                doom_strcpy(string.data(), state.messageString + start);
                start += i;
            }

            x = 160 - stringWidth(string.data()) / 2;
            writeText(x, y, string.data());
            y += littleEndian(hudFont().hu_font[0]->height);
        }
        return;
    }

    if (!overlay.menuactive)
        return;

    // Darken background so the menu is more readable.
    if (doom_flags & DOOM_FLAG_MENU_DARKEN_BG)
    {
        for (int j = 0, len = SCREENWIDTH * SCREENHEIGHT; j < len; ++j)
        {
            byte color = screens[0][j];
            color = colormaps[color + (20 * 256)];
            screens[0][j] = color;
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
        if (menuitem->name[0])
        {
            if (doom_strncmp(menuitem->name, "TXT_", 4) == 0)
            {
                drawCustomMenuText(menuitem->name, x, y);
            }
            else
            {
                drawPatchDirect(
                    x,
                    y,
                    0,
                    static_cast<Patch*>(cacheLumpName(menuitem->name)));
            }
        }
        y += LINEHEIGHT;
    }

    // DRAW SKULL
    drawPatchDirect(x + SKULLXOFF,
                          state.currentMenu->y - 5 + state.itemOn * LINEHEIGHT,
                          0,
                          static_cast<Patch*>(cacheLumpName(
                              skullName[state.whichSkull].data())));
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

    bool hide_mouse = (doom_flags & DOOM_FLAG_HIDE_MOUSE_OPTIONS) ? true : false;
    bool hide_sound = ((doom_flags & DOOM_FLAG_HIDE_MUSIC_OPTIONS)
                       && (doom_flags & DOOM_FLAG_HIDE_SOUND_OPTIONS))
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
    if (doom_flags & DOOM_FLAG_HIDE_MUSIC_OPTIONS)
    {
        SoundMenu = SoundMenuNoMusic.data();
        doom_memcpy(&SoundDef, &SoundNoMusicDef, sizeof(SoundDef));
    }
    else if (doom_flags & DOOM_FLAG_HIDE_SOUND_OPTIONS)
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
    messageToPrint = 0;
    state.messageString = nullptr;
    state.messageLastMenuActive = overlay.menuactive;
    state.quickSaveSlot = -1;

    // Here we could catch other version dependencies,
    //  like HELP1/2, and four episodes.

    switch (gameVersion().gamemode)
    {
        case commercial:
            // This is used because DOOM 2 had only one HELP
            //  page. I use CREDIT as second page now, but
            //  kept this hack for educational purposes.
            MainMenu[readthis] = MainMenu[quitdoom];
            MainDef.numitems--;
            MainDef.y += 8;
            NewDef.prevMenu = &MainDef;
            ReadDef1.routine = drawReadThis1;
            ReadDef1.x = 330;
            ReadDef1.y = 165;
            ReadMenu1[0].routine = finishReadThis;
            break;
        case shareware:
            // Episode 2 and 3 are handled,
            //  branching to an ad screen.
        case registered:
            // We need to remove the fourth episode.
            EpiDef.numitems--;
            break;
        case retail:
            // We are fine.
        default:
            break;
    }
}
} // namespace Doom
