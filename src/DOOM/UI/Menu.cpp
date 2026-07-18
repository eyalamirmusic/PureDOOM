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

#include "../doom_config.h"

#include "../doomdef.h"
#include "../dstrings.h"
#include "../d_main.h"
#include "../i_system.h"
#include "../i_video.h"
#include "../v_video.h"
#include "../Wad/WadFile.h"
#include "../r_local.h"
#include "../hu_stuff.h"
#include "../g_game.h"
#include "../m_argv.h"
#include "../m_swap.h"
#include "../s_sound.h"
#include "../doomstat.h"
#include "../sounds.h" // Data.
#include "../m_menu.h"

#include "../Game/OverlayState.h"

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
#define SAVESTRINGSIZE 24
#define SKULLXOFF -32
#define LINEHEIGHT 16

// --- Globals other subsystems read ------------------------------------------
//
// These are the only menu globals anything outside the menu touches, so they
// stay at file scope rather than moving into namespace Doom below. mouseSensitivity
// is declared in doomstat.h; messageToPrint (m_menu.h) is read by the eacp overlay
// capture.
// menuactive, automapactive and inhelpscreens (which gates Doom::displayFrame's border
// redraw) are a Doom::OverlayState owned by the Engine now; these are references
// onto it (REFACTOR.md, Step 5).
doom_boolean& inhelpscreens = Doom::overlayState().inhelpscreens;
doom_boolean& menuactive = Doom::overlayState().menuactive;
// The config-backed settings (mouse sensitivity, message toggle, detail, view
// size) are Engine members now (UI/MenuSettings.h); these are references onto
// them. Config.cpp binds its defaults[] entries to the members at runtime rather
// than capturing their addresses at static-init, which is what unblocked the move.
int& mouseSensitivity = Doom::menuSettings().mouseSensitivity;
int& showMessages = Doom::menuSettings().showMessages; // 0 = off, 1 = on
int& detailLevel = Doom::menuSettings().detailLevel; // 0 = high, 1 = normal
int& screenblocks = Doom::menuSettings().screenblocks;
int messageToPrint; // 1 = message to be printed

// Globals owned elsewhere that the menu reads or writes. They are declared here,
// at file scope, so the uses inside namespace Doom below resolve to these ::
// entities - a matching extern placed *inside* the namespace would instead be
// taken for a Doom:: member and fail to link (the trap the Step 8 notes call
// out). screens (v_video.h) and colormaps (r_state.h) already come in through
// headers; screen_palette has no header, so it is declared here.
extern int doom_flags;
// hu_font is a Doom::HudFont member (Engine); a reference onto it, so this extern is a
// reference-to-array (a plain array extern would misread the reference's pointer).
extern Doom::Patch* (&hu_font)[HU_FONTSIZE];
// chat_on/message_dontfuckwithme are Doom::HudFlags members (Engine); references onto them.
extern doom_boolean& message_dontfuckwithme;
extern doom_boolean& chat_on; // in heads-up code
// mousemove/crosshair/always_run are Doom::InputConfig members (Engine); references onto them.
extern int& mousemove;
extern int& crosshair;
extern int& always_run;
extern unsigned char screen_palette[256 * 3]; // i_video, no header

// The quit-screen taunts, drawn by quitDOOM. Declared in dstrings.h, read only
// here, so their definition moved out of dstrings.cpp to sit with their one
// reader. ::-scoped for the extern; const so the literals stay off -Wwritable.
const char* endmsg[NUM_QUITMESSAGES + 1] = {
    // DOOM1
    QUITMSG,
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

// The menu's transient interaction state now lives on the Engine (UI/MenuState.h, moved by the
// file-scope-statics sweep - REFACTOR.md, Step 5). The vanilla names below are references onto that
// member, so every use is unchanged. The immutable reference-data tables interspersed among them
// (gammamsg / skullName / detailNames / msgNames / quitsounds / menu_custom_texts) and the
// self-referential menu-definition apparatus further down stay file-local.

// temp for screenblocks (0-9)
static int& screenSize = menuState().screenSize;

// -1 = no quicksave slot picked!
static int& quickSaveSlot = menuState().quickSaveSlot;

// ...and here is the message string!
static const char*& messageString = menuState().messageString;

// message x & y
static int& messx = menuState().messx;
static int& messy = menuState().messy;
static int& messageLastMenuActive = menuState().messageLastMenuActive;

// timed message = no input from user
static doom_boolean& messageNeedsInput = menuState().messageNeedsInput;

static void (*&messageRoutine)(int response) = menuState().messageRoutine;

EA::Array<EA::Array<char, 26>, 5> gammamsg = {EA::Array<char, 26> {{GAMMALVL0}},
                                              EA::Array<char, 26> {{GAMMALVL1}},
                                              EA::Array<char, 26> {{GAMMALVL2}},
                                              EA::Array<char, 26> {{GAMMALVL3}},
                                              EA::Array<char, 26> {{GAMMALVL4}}};

// we are going to be entering a savegame string
static int& saveStringEnter = menuState().saveStringEnter;
static int& saveSlot = menuState().saveSlot; // which slot to save in
static int& saveCharIndex = menuState().saveCharIndex; // which char we're editing
// old save description before edit
static char (&saveOldString)[SAVESTRINGSIZE] = menuState().saveOldString;

static char (&savegamestrings)[10][SAVESTRINGSIZE] = menuState().savegamestrings;

static char (&endstring)[160] = menuState().endstring;

static short& itemOn = menuState().itemOn; // menu item skull is on
static short& skullAnimCounter =
    menuState().skullAnimCounter; // skull animation counter
static short& whichSkull = menuState().whichSkull; // which skull to draw

// graphic name of skulls
// warning: initializer-string for array of chars is too long
EA::Array<EA::Array<char, /*8*/ 9>, 2> skullName = {
    EA::Array<char, 9> {{"M_SKULL1"}}, EA::Array<char, 9> {{"M_SKULL2"}}};

// current menudef
static MenuDef*& currentMenu = menuState().currentMenu;

// The menu-definition tables below lean on partial aggregate initializers on
// purpose: a {0} terminates a custom-text segment list, and a {-1,"",0} marks a
// non-selectable spacer row. Both leave trailing fields defaulted, which is what
// they mean - so the warning about it is silenced just over the data.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

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

static char (&tempstring)[80] = menuState().tempstring;
static int& epi = menuState().epi;
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
void startMessage(const char* string, void* routine, doom_boolean input);
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

#pragma GCC diagnostic pop

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
                void* lump = Doom::cacheLumpName(seg->lump);
                Doom::drawPatchRectDirect(
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
            doom_strcpy(&savegamestrings[i][0], EMPTYSTRING);
            DOOM_LoadMenu[i].status = 0;
            continue;
        }
        doom_read(handle, &savegamestrings[i], SAVESTRINGSIZE);
        doom_close(handle);
        DOOM_LoadMenu[i].status = 1;
    }
}

//
// loadGameMenu & Cie.
//
void drawLoad()
{
    Doom::drawPatchDirect(
        72, 28, 0, static_cast<Patch*>(Doom::cacheLumpName("M_LOADG")));
    for (int i = 0; i < load_end; i++)
    {
        drawSaveLoadBorder(LoadDef.x, LoadDef.y + LINEHEIGHT * i);
        writeText(LoadDef.x, LoadDef.y + LINEHEIGHT * i, savegamestrings[i]);
    }
}

//
// Draw border for the savegame description
//
void drawSaveLoadBorder(int x, int y)
{
    Doom::drawPatchDirect(
        x - 8, y + 7, 0, static_cast<Patch*>(Doom::cacheLumpName("M_LSLEFT")));

    for (int i = 0; i < 24; i++)
    {
        Doom::drawPatchDirect(
            x, y + 7, 0, static_cast<Patch*>(Doom::cacheLumpName("M_LSCNTR")));
        x += 8;
    }

    Doom::drawPatchDirect(
        x, y + 7, 0, static_cast<Patch*>(Doom::cacheLumpName("M_LSRGHT")));
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
    Doom::loadGame(name.data());
    clearMenus();
}

//
// Selected from DOOM menu
//
void loadGameMenu(int)
{
    if (netgame)
    {
        startMessage(LOADNET, 0, false);
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
    int i;

    Doom::drawPatchDirect(
        72, 28, 0, static_cast<Patch*>(Doom::cacheLumpName("M_SAVEG")));
    for (i = 0; i < load_end; i++)
    {
        drawSaveLoadBorder(LoadDef.x, LoadDef.y + LINEHEIGHT * i);
        writeText(LoadDef.x, LoadDef.y + LINEHEIGHT * i, savegamestrings[i]);
    }

    if (saveStringEnter)
    {
        i = stringWidth(savegamestrings[saveSlot]);
        writeText(LoadDef.x + i, LoadDef.y + LINEHEIGHT * saveSlot, "_");
    }
}

//
// menuResponder calls this when user is finished
//
void doSave(int slot)
{
    Doom::saveGame(slot, savegamestrings[slot]);
    clearMenus();

    // PICK QUICKSAVE SLOT YET?
    if (quickSaveSlot == -2)
        quickSaveSlot = slot;
}

//
// User wants to save. Start string input for menuResponder
//
void saveSelect(int choice)
{
    // we are going to be intercepting all chars
    saveStringEnter = 1;

    saveSlot = choice;
    doom_strcpy(saveOldString, savegamestrings[choice]);
    if (!doom_strcmp(savegamestrings[choice], EMPTYSTRING))
        savegamestrings[choice][0] = 0;
    saveCharIndex = static_cast<int>(doom_strlen(savegamestrings[choice]));
}

//
// Selected from DOOM menu
//
void saveGameMenu(int)
{
    if (!usergame)
    {
        startMessage(SAVEDEAD, 0, false);
        return;
    }

    if (gamestate != GS_LEVEL)
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
        doSave(quickSaveSlot);
        Doom::startSound(0, sfx_swtchx);
    }
}

void quickSave()
{
    if (!usergame)
    {
        Doom::startSound(0, sfx_oof);
        return;
    }

    if (gamestate != GS_LEVEL)
        return;

    if (quickSaveSlot < 0)
    {
        startControlPanel();
        readSaveStrings();
        setupNextMenu(&SaveDef);
        quickSaveSlot = -2; // means to pick a slot now
        return;
    }
    //doom_sprintf(tempstring, QSPROMPT, savegamestrings[quickSaveSlot]);
    doom_strcpy(tempstring, QSPROMPT_1);
    doom_concat(tempstring, savegamestrings[quickSaveSlot]);
    doom_strcpy(tempstring, QSPROMPT_2);
    startMessage(tempstring, reinterpret_cast<void*>(quickSaveResponse), true);
}

//
// quickLoad
//
void quickLoadResponse(int ch)
{
    if (ch == 'y')
    {
        loadSelect(quickSaveSlot);
        Doom::startSound(0, sfx_swtchx);
    }
}

void quickLoad()
{
    if (netgame)
    {
        startMessage(QLOADNET, 0, false);
        return;
    }

    if (quickSaveSlot < 0)
    {
        startMessage(QSAVESPOT, 0, false);
        return;
    }
    //doom_sprintf(tempstring, QLPROMPT, savegamestrings[quickSaveSlot]);
    doom_strcpy(tempstring, QLPROMPT_1);
    doom_concat(tempstring, savegamestrings[quickSaveSlot]);
    doom_strcpy(tempstring, QLPROMPT_2);
    startMessage(tempstring, reinterpret_cast<void*>(quickLoadResponse), true);
}

//
// Read This Menus
// Had a "quick hack to fix romero bug"
//
void drawReadThis1()
{
    inhelpscreens = true;
    switch (gamemode)
    {
        case commercial:
            Doom::drawPatchDirect(
                0, 0, 0, static_cast<Patch*>(Doom::cacheLumpName("HELP")));
            break;
        case shareware:
        case registered:
        case retail:
            Doom::drawPatchDirect(
                0, 0, 0, static_cast<Patch*>(Doom::cacheLumpName("HELP1")));
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
    inhelpscreens = true;
    switch (gamemode)
    {
        case retail:
        case commercial:
            // This hack keeps us from having to change menus.
            Doom::drawPatchDirect(
                0, 0, 0, static_cast<Patch*>(Doom::cacheLumpName("CREDIT")));
            break;
        case shareware:
        case registered:
            Doom::drawPatchDirect(
                0, 0, 0, static_cast<Patch*>(Doom::cacheLumpName("HELP2")));
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
    Doom::drawPatchDirect(
        60, 38, 0, static_cast<Patch*>(Doom::cacheLumpName("M_SVOL")));

    if (!(doom_flags & DOOM_FLAG_HIDE_SOUND_OPTIONS))
    {
        int offset = (doom_flags & DOOM_FLAG_HIDE_MUSIC_OPTIONS)
                         ? static_cast<int>(sfx_vol_no_music)
                         : static_cast<int>(sfx_vol);
        drawThermo(
            SoundDef.x, SoundDef.y + LINEHEIGHT * (offset + 1), 16, snd_SfxVolume);
    }

    if (!(doom_flags & DOOM_FLAG_HIDE_MUSIC_OPTIONS))
    {
        int offset = (doom_flags & DOOM_FLAG_HIDE_SOUND_OPTIONS)
                         ? static_cast<int>(music_vol_no_sfx)
                         : static_cast<int>(music_vol);
        drawThermo(
            SoundDef.x, SoundDef.y + LINEHEIGHT * (offset + 1), 16, snd_MusicVolume);
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
    switch (choice)
    {
        case 0:
            if (snd_SfxVolume)
                snd_SfxVolume--;
            break;
        case 1:
            if (snd_SfxVolume < 15)
                snd_SfxVolume++;
            break;
    }

    Doom::setSfxVolume(snd_SfxVolume /* *8 */);
}

void musicVol(int choice)
{
    switch (choice)
    {
        case 0:
            if (snd_MusicVolume)
                snd_MusicVolume--;
            break;
        case 1:
            if (snd_MusicVolume < 15)
                snd_MusicVolume++;
            break;
    }

    Doom::setMusicVolumeLevel(snd_MusicVolume /* *8 */);
}

//
// drawMainMenu
//
void drawMainMenu()
{
    Doom::drawPatchDirect(
        94, 2, 0, static_cast<Patch*>(Doom::cacheLumpName("M_DOOM")));
}

//
// newGame
//
void drawNewGame()
{
    Doom::drawPatchDirect(
        96, 14, 0, static_cast<Patch*>(Doom::cacheLumpName("M_NEWG")));
    Doom::drawPatchDirect(
        54, 38, 0, static_cast<Patch*>(Doom::cacheLumpName("M_SKILL")));
}

void newGame(int)
{
    if (netgame && !demoplayback)
    {
        startMessage(NEWGAME, 0, false);
        return;
    }

    if (gamemode == commercial)
        setupNextMenu(&NewDef);
    else
        setupNextMenu(&EpiDef);
}

//
// episode
//
void drawEpisode()
{
    Doom::drawPatchDirect(
        54, 38, 0, static_cast<Patch*>(Doom::cacheLumpName("M_EPISOD")));
}

void verifyNightmare(int ch)
{
    if (ch != 'y')
        return;

    Doom::deferInitNew(static_cast<Skill>(nightmare), epi + 1, 1);
    clearMenus();
}

void chooseSkill(int choice)
{
    if (choice == nightmare)
    {
        startMessage(NIGHTMARE, reinterpret_cast<void*>(verifyNightmare), true);
        return;
    }

    Doom::deferInitNew(static_cast<Skill>(choice), epi + 1, 1);
    clearMenus();
}

void episode(int choice)
{
    if ((gamemode == shareware) && choice)
    {
        startMessage(SWSTRING, 0, false);
        setupNextMenu(&ReadDef1);
        return;
    }

    // Yet another hack...
    if ((gamemode == registered) && (choice > 2))
    {
        doom_print("episode: 4th episode requires UltimateDOOM\n");
        choice = 0;
    }

    epi = choice;
    setupNextMenu(&NewDef);
}

//
// optionsMenu
//
void drawOptions()
{
    Doom::drawPatchDirect(
        108, 15, 0, static_cast<Patch*>(Doom::cacheLumpName("M_OPTTTL")));

    //Doom::drawPatchDirect (OptionsDef.x + 175,OptionsDef.y+LINEHEIGHT*detail,0,
    //                Doom::cacheLumpName(detailNames[detailLevel])); // Details do nothing?

    Doom::drawPatchDirect(
        OptionsDef.x + 120,
        OptionsDef.y + LINEHEIGHT * messages,
        0,
        static_cast<Patch*>(Doom::cacheLumpName(msgNames[showMessages].data())));

    Doom::drawPatchDirect(
        OptionsDef.x + 131,
        OptionsDef.y + LINEHEIGHT * crosshair_opt,
        0,
        static_cast<Patch*>(Doom::cacheLumpName(msgNames[crosshair].data())));

    Doom::drawPatchDirect(
        OptionsDef.x + 147,
        OptionsDef.y + LINEHEIGHT * always_run_opt,
        0,
        static_cast<Patch*>(Doom::cacheLumpName(msgNames[always_run].data())));

    drawThermo(
        OptionsDef.x, OptionsDef.y + LINEHEIGHT * (scrnsize + 1), 9, screenSize);
}

void drawMouseOptions()
{
    drawCustomMenuText("TXT_MOPT", 74, 45);

    Doom::drawPatchDirect(
        MouseOptionsDef.x + 149,
        MouseOptionsDef.y + LINEHEIGHT * mousemov,
        0,
        static_cast<Patch*>(Doom::cacheLumpName(msgNames[mousemove].data())));

    drawThermo(MouseOptionsDef.x,
               MouseOptionsDef.y + LINEHEIGHT * (mousesens + 1),
               10,
               mouseSensitivity);
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
    showMessages = 1 - showMessages;

    if (!showMessages)
        players[consoleplayer].message = MSGOFF;
    else
        players[consoleplayer].message = MSGON;

    message_dontfuckwithme = true;
}

//
// Toggle crosshair on/off
//
void changeCrosshair(int)
{
    crosshair = 1 - crosshair;

    if (!crosshair)
        players[consoleplayer].message = CROSSOFF;
    else
        players[consoleplayer].message = CROSSON;

    message_dontfuckwithme = true;
}

//
// Toggle always-run on/off
//
void changeAlwaysRun(int)
{
    always_run = 1 - always_run;

    if (!always_run)
        players[consoleplayer].message = ALWAYSRUNOFF;
    else
        players[consoleplayer].message = ALWAYSRUNON;

    message_dontfuckwithme = true;
}

//
// endGame
//
void endGameResponse(int ch)
{
    if (ch != 'y')
        return;

    currentMenu->lastOn = itemOn;
    clearMenus();
    Doom::startTitle();
}

void endGame(int)
{
    if (!usergame)
    {
        Doom::startSound(0, sfx_oof);
        return;
    }

    if (netgame)
    {
        startMessage(NETEND, 0, false);
        return;
    }

    startMessage(ENDGAME, reinterpret_cast<void*>(endGameResponse), true);
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
    if (ch != 'y')
        return;
    if (!netgame)
    {
        if (gamemode == commercial)
            Doom::startSound(0, quitsounds2[(gametic >> 2) & 7]);
        else
            Doom::startSound(0, quitsounds[(gametic >> 2) & 7]);
        waitVBlank(105);
    }
    quitGame();
}

void quitDOOM(int)
{
    // We pick index 0 which is language sensitive,
    //  or one at random, between 1 and maximum number.
    if (language != english)
    {
        //doom_sprintf(endstring, "%s\n\n"DOSY, endmsg[0]);
        doom_strcpy(endstring, endmsg[0]);
        doom_concat(endstring, "\n\n" DOSY);
    }
    else
    {
        //doom_sprintf(endstring, "%s\n\n" DOSY, endmsg[gametic % (NUM_QUITMESSAGES - 2) + 1]);
        doom_strcpy(endstring, endmsg[gametic % (NUM_QUITMESSAGES - 2) + 1]);
        doom_concat(endstring, "\n\n" DOSY);
    }

    startMessage(endstring, reinterpret_cast<void*>(quitResponse), true);
}

void changeSensitivity(int choice)
{
    switch (choice)
    {
        case 0:
            if (mouseSensitivity)
                mouseSensitivity--;
            break;
        case 1:
            if (mouseSensitivity < 9)
                mouseSensitivity++;
            break;
    }
}

void mouseMove(int)
{
    mousemove = 1 - mousemove;

    return;
}

void changeDetail(int)
{
    detailLevel = 1 - detailLevel;

    // FIXME - does not work. Remove anyway?
    doom_print("changeDetail: low detail mode n.a.\n");
}

void sizeDisplay(int choice)
{
    switch (choice)
    {
        case 0:
            if (screenSize > 0)
            {
                screenblocks--;
                screenSize--;
            }
            break;
        case 1:
            if (screenSize < 8)
            {
                screenblocks++;
                screenSize++;
            }
            break;
    }

    Doom::setViewSize(screenblocks, detailLevel);
}

//
// Menu Functions
//
void drawThermo(int x, int y, int thermWidth, int thermDot)
{
    int xx;

    xx = x;
    Doom::drawPatchDirect(
        xx, y, 0, static_cast<Patch*>(Doom::cacheLumpName("M_THERML")));
    xx += 8;
    for (int i = 0; i < thermWidth; i++)
    {
        Doom::drawPatchDirect(
            xx, y, 0, static_cast<Patch*>(Doom::cacheLumpName("M_THERMM")));
        xx += 8;
    }
    Doom::drawPatchDirect(
        xx, y, 0, static_cast<Patch*>(Doom::cacheLumpName("M_THERMR")));

    Doom::drawPatchDirect((x + 8) + thermDot * 8,
                          y,
                          0,
                          static_cast<Patch*>(Doom::cacheLumpName("M_THERMO")));
}

void drawEmptyCell(MenuDef* menu, int item)
{
    Doom::drawPatchDirect(menu->x - 10,
                          menu->y + item * LINEHEIGHT - 1,
                          0,
                          static_cast<Patch*>(Doom::cacheLumpName("M_CELL1")));
}

void drawSelCell(MenuDef* menu, int item)
{
    Doom::drawPatchDirect(menu->x - 10,
                          menu->y + item * LINEHEIGHT - 1,
                          0,
                          static_cast<Patch*>(Doom::cacheLumpName("M_CELL2")));
}

void startMessage(const char* string, void* routine, doom_boolean input)
{
    messageLastMenuActive = menuactive;
    messageToPrint = 1;
    messageString = string;
    messageRoutine = reinterpret_cast<void (*)(int)>(routine);
    messageNeedsInput = input;
    menuactive = true;
    return;
}

void stopMessage()
{
    menuactive = messageLastMenuActive;
    messageToPrint = 0;
}

//
// Find string width from hu_font chars
//
int stringWidth(const char* string)
{
    int w = 0;
    int c;

    for (int i = 0; i < doom_strlen(string); i++)
    {
        c = doom_toupper(string[i]) - HU_FONTSTART;
        if (c < 0 || c >= HU_FONTSIZE)
            w += 4;
        else
            w += SHORT(hu_font[c]->width);
    }

    return w;
}

//
// Find string height from hu_font chars
//
int stringHeight(const char* string)
{
    int h;
    int height = SHORT(hu_font[0]->height);

    h = height;
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
    int w;
    const char* ch;
    int c;
    int cx;
    int cy;

    ch = string;
    cx = x;
    cy = y;

    while (1)
    {
        c = *ch++;
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

        w = SHORT(hu_font[c]->width);
        if (cx + w > SCREENWIDTH)
            break;
        Doom::drawPatchDirect(cx, cy, 0, hu_font[c]);
        cx += w;
    }
}

//
// CONTROL PANEL
//

//
// menuResponder
//
doom_boolean menuResponder(Event* ev)
{
    int ch;
    int& joywait = menuState().joywait;
    int& mousewait = menuState().mousewait;
    int& mousey = menuState().mousey;
    int& lasty = menuState().lasty;
    int& mousex = menuState().mousex;
    int& lastx = menuState().lastx;

    ch = -1;

    if (ev->type == ev_joystick && joywait < currentTic())
    {
        if (ev->data3 == -1)
        {
            ch = KEY_UPARROW;
            joywait = currentTic() + 5;
        }
        else if (ev->data3 == 1)
        {
            ch = KEY_DOWNARROW;
            joywait = currentTic() + 5;
        }

        if (ev->data2 == -1)
        {
            ch = KEY_LEFTARROW;
            joywait = currentTic() + 2;
        }
        else if (ev->data2 == 1)
        {
            ch = KEY_RIGHTARROW;
            joywait = currentTic() + 2;
        }

        if (ev->data1 & 1)
        {
            ch = KEY_ENTER;
            joywait = currentTic() + 5;
        }
        if (ev->data1 & 2)
        {
            ch = KEY_BACKSPACE;
            joywait = currentTic() + 5;
        }
    }
    else
    {
        if (ev->type == ev_mouse && mousewait < currentTic())
        {
            mousey += ev->data3;
            if (mousey < lasty - 30)
            {
                ch = KEY_DOWNARROW;
                mousewait = currentTic() + 5;
                mousey = lasty -= 30;
            }
            else if (mousey > lasty + 30)
            {
                ch = KEY_UPARROW;
                mousewait = currentTic() + 5;
                mousey = lasty += 30;
            }

            mousex += ev->data2;
            if (mousex < lastx - 30)
            {
                ch = KEY_LEFTARROW;
                mousewait = currentTic() + 5;
                mousex = lastx -= 30;
            }
            else if (mousex > lastx + 30)
            {
                ch = KEY_RIGHTARROW;
                mousewait = currentTic() + 5;
                mousex = lastx += 30;
            }

            if (ev->data1 & 1)
            {
                ch = KEY_ENTER;
                mousewait = currentTic() + 15;
            }

            if (ev->data1 & 2)
            {
                ch = KEY_BACKSPACE;
                mousewait = currentTic() + 15;
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
    if (saveStringEnter)
    {
        switch (ch)
        {
            case KEY_BACKSPACE:
                if (saveCharIndex > 0)
                {
                    saveCharIndex--;
                    savegamestrings[saveSlot][saveCharIndex] = 0;
                }
                break;

            case KEY_ESCAPE:
                saveStringEnter = 0;
                doom_strcpy(&savegamestrings[saveSlot][0], saveOldString);
                break;

            case KEY_ENTER:
                saveStringEnter = 0;
                if (savegamestrings[saveSlot][0])
                    doSave(saveSlot);
                break;

            default:
                ch = doom_toupper(ch);
                if (ch != 32)
                    if (ch - HU_FONTSTART < 0 || ch - HU_FONTSTART >= HU_FONTSIZE)
                        break;
                if (ch >= 32 && ch <= 127 && saveCharIndex < SAVESTRINGSIZE - 1
                    && stringWidth(savegamestrings[saveSlot])
                           < (SAVESTRINGSIZE - 2) * 8)
                {
                    savegamestrings[saveSlot][saveCharIndex++] = ch;
                    savegamestrings[saveSlot][saveCharIndex] = 0;
                }
                break;
        }
        return true;
    }

    // Take care of any messages that need input
    if (messageToPrint)
    {
        if (messageNeedsInput == true
            && !(ch == ' ' || ch == 'n' || ch == 'y' || ch == KEY_ESCAPE))
            return false;

        menuactive = messageLastMenuActive;
        messageToPrint = 0;
        if (messageRoutine)
            messageRoutine(ch);

        menuactive = false;
        Doom::startSound(0, sfx_swtchx);
        return true;
    }

    if (devparm && ch == KEY_F1)
    {
        Doom::takeScreenshot();
        return true;
    }

    // F-Keys
    if (!menuactive)
        switch (ch)
        {
            case KEY_MINUS: // Screen size down
                if (automapactive || chat_on)
                    return false;
                sizeDisplay(0);
                Doom::startSound(0, sfx_stnmov);
                return true;

            case KEY_EQUALS: // Screen size up
                if (automapactive || chat_on)
                    return false;
                sizeDisplay(1);
                Doom::startSound(0, sfx_stnmov);
                return true;

            case KEY_F1: // Help key
                startControlPanel();

                if (gamemode == retail)
                    currentMenu = &ReadDef2;
                else
                    currentMenu = &ReadDef1;

                itemOn = 0;
                Doom::startSound(0, sfx_swtchn);
                return true;

            case KEY_F2: // Save
                startControlPanel();
                Doom::startSound(0, sfx_swtchn);
                saveGameMenu(0);
                return true;

            case KEY_F3: // Load
                startControlPanel();
                Doom::startSound(0, sfx_swtchn);
                loadGameMenu(0);
                return true;

            case KEY_F4: // Sound Volume
                startControlPanel();
                currentMenu = &SoundDef;
                itemOn = sfx_vol;
                Doom::startSound(0, sfx_swtchn);
                return true;

                // case KEY_F5:            // Detail toggle
                //     changeDetail(0);
                //     Doom::startSound(0, sfx_swtchn);
                //     return true;

            case KEY_F5: // Crosshair toggle
                changeCrosshair(0);
                Doom::startSound(0, sfx_swtchn);
                return true;

            case KEY_F6: // Quicksave
                Doom::startSound(0, sfx_swtchn);
                quickSave();
                return true;

            case KEY_F7: // End game
                Doom::startSound(0, sfx_swtchn);
                endGame(0);
                return true;

            case KEY_F8: // Toggle messages
                changeMessages(0);
                Doom::startSound(0, sfx_swtchn);
                return true;

            case KEY_F9: // Quickload
                Doom::startSound(0, sfx_swtchn);
                quickLoad();
                return true;

            case KEY_F10: // Quit DOOM
                Doom::startSound(0, sfx_swtchn);
                quitDOOM(0);
                return true;

            case KEY_F11: // gamma toggle
                usegamma++;
                if (usegamma > 4)
                    usegamma = 0;
                players[consoleplayer].message = gammamsg[usegamma].data();
                setPalette(static_cast<byte*>(Doom::cacheLumpName("PLAYPAL")));
                return true;
        }

    // Pop-up menu?
    if (!menuactive)
    {
        if (ch == KEY_ESCAPE)
        {
            startControlPanel();
            Doom::startSound(0, sfx_swtchn);
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
                if (itemOn + 1 > currentMenu->numitems - 1)
                    itemOn = 0;
                else
                    itemOn++;
                Doom::startSound(0, sfx_pstop);
            } while (currentMenu->menuitems[itemOn].status == -1);
            return true;

        case KEY_UPARROW:
            do
            {
                if (!itemOn)
                    itemOn = currentMenu->numitems - 1;
                else
                    itemOn--;
                Doom::startSound(0, sfx_pstop);
            } while (currentMenu->menuitems[itemOn].status == -1);
            return true;

        case KEY_LEFTARROW:
            if (currentMenu->menuitems[itemOn].routine
                && currentMenu->menuitems[itemOn].status == 2)
            {
                Doom::startSound(0, sfx_stnmov);
                currentMenu->menuitems[itemOn].routine(0);
            }
            return true;

        case KEY_RIGHTARROW:
            if (currentMenu->menuitems[itemOn].routine
                && currentMenu->menuitems[itemOn].status == 2)
            {
                Doom::startSound(0, sfx_stnmov);
                currentMenu->menuitems[itemOn].routine(1);
            }
            return true;

        case KEY_ENTER:
            if (currentMenu->menuitems[itemOn].routine
                && currentMenu->menuitems[itemOn].status)
            {
                currentMenu->lastOn = itemOn;
                if (currentMenu->menuitems[itemOn].status == 2)
                {
                    currentMenu->menuitems[itemOn].routine(1); // right arrow
                    Doom::startSound(0, sfx_stnmov);
                }
                else
                {
                    currentMenu->menuitems[itemOn].routine(itemOn);
                    Doom::startSound(0, sfx_pistol);
                }
            }
            return true;

        case KEY_ESCAPE:
            currentMenu->lastOn = itemOn;
            clearMenus();
            Doom::startSound(0, sfx_swtchx);
            return true;

        case KEY_BACKSPACE:
            currentMenu->lastOn = itemOn;
            if (currentMenu->prevMenu)
            {
                currentMenu = currentMenu->prevMenu;
                itemOn = currentMenu->lastOn;
                Doom::startSound(0, sfx_swtchn);
            }
            return true;

        default:
            for (int i = itemOn + 1; i < currentMenu->numitems; i++)
                if (currentMenu->menuitems[i].alphaKey == ch)
                {
                    itemOn = i;
                    Doom::startSound(0, sfx_pstop);
                    return true;
                }
            for (int i = 0; i <= itemOn; i++)
                if (currentMenu->menuitems[i].alphaKey == ch)
                {
                    itemOn = i;
                    Doom::startSound(0, sfx_pstop);
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
    // intro might call this repeatedly
    if (menuactive)
        return;

    menuactive = 1;
    currentMenu = &MainDef; // JDC
    itemOn = currentMenu->lastOn; // JDC
}

//
// drawMenu
// Called after the view has been rendered,
// but before it has been blitted.
//
void drawMenu()
{
    static short x;
    static short y;
    short i;
    short max;
    EA::Array<char, 40> string;
    int start;

    inhelpscreens = false;

    // Horiz. & Vertically center string and print it.
    if (messageToPrint)
    {
        start = 0;
        y = 100 - stringHeight(messageString) / 2;
        while (*(messageString + start))
        {
            for (i = 0; i < doom_strlen(messageString + start); i++)
                if (*(messageString + start + i) == '\n')
                {
                    doom_memset(string.data(), 0, 40);
                    doom_strncpy(string.data(), messageString + start, i);
                    start += i + 1;
                    break;
                }

            if (i == doom_strlen(messageString + start))
            {
                doom_strcpy(string.data(), messageString + start);
                start += i;
            }

            x = 160 - stringWidth(string.data()) / 2;
            writeText(x, y, string.data());
            y += SHORT(hu_font[0]->height);
        }
        return;
    }

    if (!menuactive)
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

    if (currentMenu->routine)
        currentMenu->routine(); // call Draw routine

    // DRAW MENU
    x = currentMenu->x;
    y = currentMenu->y;
    max = currentMenu->numitems;

    for (i = 0; i < max; i++)
    {
        MenuItem* menuitem = currentMenu->menuitems + i;
        if (menuitem->name[0])
        {
            if (doom_strncmp(menuitem->name, "TXT_", 4) == 0)
            {
                drawCustomMenuText(menuitem->name, x, y);
            }
            else
            {
                Doom::drawPatchDirect(
                    x,
                    y,
                    0,
                    static_cast<Patch*>(Doom::cacheLumpName(menuitem->name)));
            }
        }
        y += LINEHEIGHT;
    }

    // DRAW SKULL
    Doom::drawPatchDirect(
        x + SKULLXOFF,
        currentMenu->y - 5 + itemOn * LINEHEIGHT,
        0,
        static_cast<Patch*>(Doom::cacheLumpName(skullName[whichSkull].data())));
}

//
// clearMenus
//
void clearMenus()
{
    menuactive = 0;
}

//
// setupNextMenu
//
void setupNextMenu(MenuDef* menudef)
{
    currentMenu = menudef;
    itemOn = currentMenu->lastOn;
}

//
// menuTicker
//
void menuTicker()
{
    if (--skullAnimCounter <= 0)
    {
        whichSkull ^= 1;
        skullAnimCounter = 8;
    }
}

//
// initMenu
//
void initMenu()
{
    doom_boolean hide_mouse =
        (doom_flags & DOOM_FLAG_HIDE_MOUSE_OPTIONS) ? true : false;
    doom_boolean hide_sound = ((doom_flags & DOOM_FLAG_HIDE_MUSIC_OPTIONS)
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

    currentMenu = &MainDef;
    menuactive = 0;
    itemOn = currentMenu->lastOn;
    whichSkull = 0;
    skullAnimCounter = 10;
    screenSize = screenblocks - 3;
    messageToPrint = 0;
    messageString = nullptr;
    messageLastMenuActive = menuactive;
    quickSaveSlot = -1;

    // Here we could catch other version dependencies,
    //  like HELP1/2, and four episodes.

    switch (gamemode)
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
