#pragma once

#include "../doomtype.h" // doom_boolean

#include <functional>

namespace Doom
{
// Forward declaration of the menu-definition struct (defined in UI/Menu.cpp). currentMenu points at
// one of the file-local menu tables, so it needs the type named but not its layout.
struct MenuDef;

// SAVESTRINGSIZE in UI/Menu (and Game/Game): the length of a savegame description. The
// reference-to-array bindings in Menu.cpp still spell the arrays with the SAVESTRINGSIZE macro, so
// this must equal it - a drift would fail to compile there (char(&)[24] vs char(&)[macro]).
inline constexpr int menuSaveStringSize = 24;

// The menu's transient interaction state - what UI/Menu keeps as the user navigates: which menu and
// item the skull cursor is on and its blink animation, the screen-size preview, the pop-up message
// box (its text, position, timed-vs-input flag and the routine that answers it), and the savegame
// string editor (which slot, the character being edited, the descriptions and the pre-edit backup).
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were UI/Menu's
// own namespace-scope private globals, read by no other file. UI/Menu reaches them through a
// hoisted `auto& state = menuState();` local per function rather than a file-scope or
// function-scope alias (REFACTOR.md, Step 9 strand (a)). The menu is frame-golden-covered
// (Tests/Goldens/menu.frames drives a scripted walk through it), so this is live-verified, not just
// build + app-link - relocating the storage changes nothing observable either way. (messx/messy,
// the message box's never-used position fields, were dropped outright rather than hoisted - no
// reader anywhere ever set or read them.)
//
// Three groups of Menu globals stay put and do *not* move in here:
//  - the config-backed / cross-read globals (screenblocks / detailLevel / showMessages /
//    mouseSensitivity / inhelpscreens / messageToPrint, and menuactive which is already a
//    Doom::OverlayState reference) keep their :: file scope above the namespace - the config-backed
//    ones are blocked until the config rework, the rest are read by other subsystems;
//  - the immutable reference data (the gamma/skull/detail/message lump-name tables, the quit-sound
//    tables, the custom-text segments) stays file-local - fixed constants, not per-run state;
//  - the self-referential menu-definition apparatus - every *Menu[] / *Def table (their prevMenu
//    pointers cross-link the tables and lastOn is written as the user navigates) and the
//    OptionsMenu / SoundMenu variant selectors (whose MenuItem element type is an anonymous-struct
//    typedef that cannot be forward-declared) - stays file-local, the same self-referential trap the
//    intermission's animation tables and the automap cheat hit.
struct MenuState
{
    int screenSize = 0; // temp for screenblocks (0-9)
    int quickSaveSlot = 0; // initMenu sets -1 (= none picked); zero-init here

    // menuResponder's input debounce, folded in from its function-local statics: the
    // joystick/mouse repeat timers and the mouse-movement accumulation that turns raw
    // motion into discrete menu up/down steps.
    int joywait = 0; // tics until the joystick may repeat
    int mousewait = 0; // tics until a mouse button may repeat
    int mousey = 0; // accumulated mouse Y since last step
    int lasty = 0; // mouse Y at the last step
    int mousex = 0; // accumulated mouse X since last step
    int lastx = 0; // mouse X at the last step

    // The pop-up message box.
    const char* messageString = nullptr; // the message text
    int messageLastMenuActive = 0; // menuactive as the message opened
    doom_boolean messageNeedsInput = false; // timed message = no user input
    // Answers the message. Given a non-null default (eacp style) so the responder
    // can call it unconditionally - vanilla passed this as a void* and
    // reinterpret_cast it back to a function pointer, and null-checked it.
    std::function<void(int response)> messageRoutine = [](int) {};

    // The savegame string editor.
    int saveStringEnter = 0; // editing a savegame description
    int saveSlot = 0; // which slot to save in
    int saveCharIndex = 0; // which char we're editing
    char saveOldString[menuSaveStringSize] = {}; // description before the edit
    char savegamestrings[10][menuSaveStringSize] = {}; // the ten slot descriptions

    char endstring[160] = {}; // built quit/end-game confirmation text

    // The skull cursor.
    short itemOn = 0; // menu item the skull is on
    short skullAnimCounter = 0; // skull blink counter
    short whichSkull = 0; // which skull frame to draw

    MenuDef* currentMenu =
        nullptr; // the menu currently shown (into a file-local table)

    char tempstring[80] = {}; // scratch for building confirmation strings
    int epi = 0; // episode picked in the new-game flow
};

// The one MenuState, a view onto the Engine's member - the same pattern as the other clusters
// (finaleState(), intermissionState(), ...).
MenuState& menuState();
} // namespace Doom
