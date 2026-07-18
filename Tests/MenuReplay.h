#pragma once

#include "DemoReplay.h"

#include <DOOM/DOOM.h>

#include <string>
#include <vector>

// The menu is the one part of the engine no demo reaches - nothing in a .lmp
// opens one - so before m_menu is rewritten it gets the same kind of net the
// renderer got in Step 0: drive synthetic key events through the real host path
// (doom_key_down -> Doom::postEvent -> Doom::menuResponder), let Doom::menuTicker blink the skull
// and Doom::drawMenu paint, and hash the finished software frame every tic.
//
// The background is the attract-mode title screen (TITLEPIC), a static picture,
// so the golden pins the menu drawn over it and nothing else. The script walks
// the menus the way a player would - options and their toggles, the thermometer
// sliders, the mouse and sound submenus, episode and skill select, the help
// pages, load/save, the quit prompt, and the title-screen F-keys - but it never
// commits: it never starts, loads or saves a game, and it answers the quit and
// nightmare prompts with "no". So gamestate never leaves GS_DEMOSCREEN and the
// process is never taken down, while nearly every branch of Doom::menuResponder and
// Doom::drawMenu still runs.

namespace DoomTests
{
// A step is a key to press this tic, or 0 to let a tic pass with no input (which
// is how the skull's blink, on an 8-tic Doom::menuTicker cycle, gets into the golden).
// Keys are doom_key_t values: doom_key_down posts them as event.data1, and the
// responder compares that against the identical KEY_* codes.
using MenuScript = std::vector<int>;

inline MenuScript menuScript()
{
    constexpr auto ESC = DOOM_KEY_ESCAPE;
    constexpr auto ENT = DOOM_KEY_ENTER;
    constexpr auto UP = DOOM_KEY_UP_ARROW;
    constexpr auto DN = DOOM_KEY_DOWN_ARROW;
    constexpr auto LF = DOOM_KEY_LEFT_ARROW;
    constexpr auto RT = DOOM_KEY_RIGHT_ARROW;
    constexpr auto BS = DOOM_KEY_BACKSPACE;
    constexpr auto NO = DOOM_KEY_N; // answers a yes/no prompt with "no"
    constexpr auto WAIT = 0;

    auto s = MenuScript {};
    auto add = [&](std::initializer_list<int> keys) { s.insert(s.end(), keys); };

    // Open the main menu and sit on it long enough for the skull to blink.
    add({ESC, WAIT, WAIT, WAIT, WAIT, WAIT, WAIT, WAIT, WAIT, WAIT});

    // Options: toggle messages off and on, the crosshair and always-run on, then
    // work the screen-size thermometer left and right.
    add({DN, ENT}); // main: newgame -> options -> Options menu
    add({DN, ENT, ENT}); // -> messages, toggle off, toggle on
    add({DN, ENT}); // -> crosshair, toggle
    add({DN, ENT}); // -> always run, toggle
    add({DN, LF, LF, RT, RT}); // -> screen size, slide down then up

    // Mouse options submenu: toggle mouse-move, slide the sensitivity thermo.
    add({DN, ENT}); // scrnsize -> (empty) -> mouse options -> submenu
    add({ENT}); // toggle mouse move
    add({DN, RT, RT, LF}); // -> sensitivity, slide
    add({BS}); // back to Options

    // Sound submenu: slide the sfx and music thermos.
    add({DN, ENT}); // -> sound volume -> Sound menu
    add({RT, LF}); // sfx volume
    add({DN, LF, RT}); // -> (empty) -> music volume, slide
    add({BS, BS}); // Sound -> Options -> Main

    // New game -> episode -> skill; step onto Nightmare and answer its prompt no.
    add({UP, ENT}); // main: options -> newgame -> Episode
    add({DN, UP}); // wander the episode list
    add({ENT}); // ep1 -> skill select
    add({DN, DN}); // -> violence -> nightmare
    add({ENT, WAIT, NO}); // nightmare prompt, then decline (menu closes)

    // Help pages.
    add({ESC}); // reopen main (on newgame)
    add({DN, DN, DN, DN}); // -> read this
    add({ENT, WAIT}); // HELP1
    add({ENT, WAIT}); // HELP2
    add({ENT}); // finish -> main

    // Load (empty slots) and save (refused, no game in progress).
    add({UP, UP}); // read this -> savegame -> loadgame
    add({ENT, WAIT}); // load menu, empty slots
    add({BS}); // back to main (loadgame)
    add({DN, ENT, WAIT, ENT}); // savegame -> "save dead" message, dismiss

    // Quit prompt, declined.
    add({ESC}); // reopen main (on savegame)
    add({DN, DN}); // -> read this -> quit
    add({ENT, WAIT, NO}); // quit prompt, decline

    // Title-screen F-keys, with the menu closed between them.
    add({DOOM_KEY_F1, WAIT, ESC}); // help, then close
    add({DOOM_KEY_F5}); // crosshair toggle
    add({DOOM_KEY_F8}); // messages toggle
    // Gamma cycles 0..4 and wraps, so five presses paint every ramp and restore
    // the palette the frame is resolved through.
    add({DOOM_KEY_F11, DOOM_KEY_F11, DOOM_KEY_F11, DOOM_KEY_F11, DOOM_KEY_F11});

    return s;
}

// GS_DEMOSCREEN, as doomSimGameState reports it (the gamestate_t enum's fourth
// member). The whole script must stay on the title screen; if it drifts off, the
// background is no longer the deterministic picture the golden was recorded over.
constexpr auto gsDemoScreen = 3;

inline Hashes runMenuScript()
{
    auto frames = Hashes {};

    nano::check(doomSimBootToTitle() != 0, "engine booted to the title screen");

    // The title arrives through a screen melt. Run it out before hashing so the
    // golden pins the menu, not the entry wipe: one tic to bring the title up and
    // start the melt, then tics until the melt is done.
    doomSimStepTic();

    for (auto guard = 0; doomSimIsWiping() && guard < 200; ++guard)
        doomSimStepTic();

    nano::check(!doomSimIsWiping(), "the title wipe finished");
    nano::check(doomSimGameState() == gsDemoScreen, "the title screen is up");

    for (auto key: menuScript())
    {
        if (key != 0)
        {
            doomSimPostKeyDown(key);
            doomSimPostKeyUp(key);
        }

        // A menu action that reached Doom::quitGame would return 0 here; the script is
        // written never to, so this doubles as an assertion that it did not.
        nano::check(doomSimStepTic() != 0, "the tic ran");
        frames.push_back(doomSimFrameHash());
    }

    // If the attract loop had advanced to a demo, or a menu action had started a
    // level, the frames after it would be of a different world than the golden's.
    nano::check(doomSimGameState() == gsDemoScreen,
                "the background stayed the title screen");

    return frames;
}

inline void checkMenuMatchesGolden()
{
    auto frames = runMenuScript();

    nano::check(!frames.empty(), "the menu script drove at least one frame");

    if (updatingGoldens())
    {
        writeGolden("menu",
                    "frames",
                    "the software frame and palette, hashed every tic of a "
                    "scripted menu walk over the title screen.",
                    frames);
        return;
    }

    auto golden = readGolden("menu", "frames");

    if (golden.empty())
    {
        std::printf("\nNo menu golden. Record one with DOOM_UPDATE_GOLDENS=1\n\n");
        nano::check(false, "menu golden exists");
        return;
    }

    const auto shared = std::min(frames.size(), golden.size());

    for (auto i = std::size_t {0}; i < shared; ++i)
    {
        if (frames[i] == golden[i])
            continue;

        std::printf("\nmenu: the rendered frame changed at step %d\n"
                    "  Nothing in a demo opens a menu, so this frame golden is\n"
                    "  m_menu's only net. If the change was intended, re-record:\n"
                    "  DOOM_UPDATE_GOLDENS=1\n\n",
                    (int) i);
        nano::check(false, "menu renderer matches the golden");
        return;
    }

    if (frames.size() != golden.size())
        std::printf("\nmenu.frames: %zu entries, golden has %zu\n\n",
                    frames.size(),
                    golden.size());

    nano::check(frames.size() == golden.size(), "the menu walk is the same length");
}
} // namespace DoomTests
