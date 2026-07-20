#pragma once

#include "../Containers.h"

#include <string>
#include <string_view>

#include <functional>

namespace Doom
{
// Forward declaration of the menu-definition struct (defined in UI/Menu.cpp). currentMenu points at
// one of the file-local menu tables, so it needs the type named but not its layout.
struct MenuDef;

// The length of a savegame description: the width of the .dsg's description field, and therefore
// the size of the ten slot buffers below. UI/Menu reads this constant directly; Game/Game keeps its
// own SAVESTRINGSIZE for the on-disk field and static_asserts it equal to this one.
//
// The comment here used to claim something that had stopped being true: that Menu.cpp's
// reference-to-array bindings (char(&)[24]) would fail to compile if the two drifted, so no further
// care was needed. Step 9 strand (a) retired every one of those bindings, which silently removed
// the compile-time link the claim rested on - leaving two 24s with nothing tying them together, and
// a comment asserting a guarantee that no longer existed. See REFACTOR.md item 6's
// duplicate-constant entry; this is the shape where a *later* refactor deletes the mechanism an
// *earlier* comment relied on, and nothing points at the comment.
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
    std::string_view messageString; // the message text
    int messageLastMenuActive = 0; // menuactive as the message opened
    bool messageNeedsInput = false; // timed message = no user input
    // Answers the message. Given a non-null default (eacp style) so the responder
    // can call it unconditionally - vanilla passed this as a void* and
    // reinterpret_cast it back to a function pointer, and null-checked it.
    std::function<void(int response)> messageRoutine = [](int) {};

    // The savegame string editor.
    int saveStringEnter = 0; // editing a savegame description
    int saveSlot = 0; // which slot to save in
    std::string saveOldString; // description before the edit
    Array<std::string, 10> savegamestrings; // the ten slot descriptions

    std::string endstring; // built quit/end-game confirmation text

    // The skull cursor.
    short itemOn = 0; // menu item the skull is on
    short skullAnimCounter = 0; // skull blink counter
    short whichSkull = 0; // which skull frame to draw

    MenuDef* currentMenu =
        nullptr; // the menu currently shown (into a file-local table)

    std::string tempstring; // scratch for building confirmation strings
    int epi = 0; // episode picked in the new-game flow
};

// The one MenuState, a view onto the Engine's member - the same pattern as the other clusters
// (finaleState(), intermissionState(), ...).
MenuState& menuState();
} // namespace Doom
