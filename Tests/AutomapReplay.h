#pragma once

#include "DemoReplay.h"

#include <DOOM/DOOM.h>

#include <string>
#include <vector>

// UI/Automap.cpp is ~1,300 lines of drawing, panning, zooming and marking, and
// like m_menu before it, no demo reaches it - nothing in a recorded .lmp opens
// the map (vanilla's D_Display skips R_RenderPlayerView entirely while it is up,
// which is why this port had to add its own eacpDoomRevealAutomap). It gets the
// same net m_menu got before its own rewrite: drive synthetic key events through
// the real host path (doom_key_down -> Doom::postEvent -> Doom::gameResponder ->
// Doom::automapResponder), let Doom::automapTicker and Doom::drawAutomap run, and
// hash the finished software frame every tic.
//
// Unlike the menu, there is no static title picture to draw the map over - the
// automap needs a level. E1M1 at skill medium is loaded directly
// (doomSimLoadLevel), the same fixed, deterministic anchor
// Tests/Sim/ScenarioTests.cpp uses. The player is left completely alone: nothing
// here drives a ticcmd, so the world's only motion is whatever E1M1's own
// monsters do on their own thinking - P_Random-driven, and therefore exactly as
// deterministic as everything else a demo pins, and it sharpens the golden rather
// than threatening it. A level load wipes exactly as any level transition does,
// so the melt is run out before the script starts, the same shape as the menu
// harness running the title's entry wipe out first.

namespace DoomTests
{
// One tic's worth of synthetic input: a key to press down this tic, a key to
// release, either or both 0. A held key (pan, zoom) is a `down` on the tic it
// starts and an `up` on the tic it ends, with plain waits in between - that is
// what automapResponder itself expects, since m_paninc and the zoom multipliers
// are set on keydown and cleared on keyup, not polled each tic from a "still
// held" state.
struct AutomapStep
{
    int down = 0;
    int up = 0;
};
using AutomapScript = std::vector<AutomapStep>;

// The part of the walk that runs with the map already open - opening and closing
// it are handled explicitly in runAutomapScript, each checked against
// doomSimAutomapActive rather than assumed.
inline AutomapScript automapScript()
{
    constexpr auto RIGHT = DOOM_KEY_RIGHT_ARROW; // AM_PANRIGHTKEY
    constexpr auto LEFT = DOOM_KEY_LEFT_ARROW; // AM_PANLEFTKEY
    constexpr auto UP = DOOM_KEY_UP_ARROW; // AM_PANUPKEY
    constexpr auto DOWN = DOOM_KEY_DOWN_ARROW; // AM_PANDOWNKEY
    constexpr auto ZOOMIN = DOOM_KEY_EQUALS; // AM_ZOOMINKEY '='
    constexpr auto ZOOMOUT = DOOM_KEY_MINUS; // AM_ZOOMOUTKEY '-'
    constexpr auto BIG = DOOM_KEY_0; // AM_GOBIGKEY '0'
    constexpr auto FOLLOW = DOOM_KEY_F; // AM_FOLLOWKEY
    constexpr auto GRID = DOOM_KEY_G; // AM_GRIDKEY
    constexpr auto MARK = DOOM_KEY_M; // AM_MARKKEY
    constexpr auto CLEAR = DOOM_KEY_C; // AM_CLEARMARKKEY

    auto s = AutomapScript {};
    auto tap = [&](int key) { s.push_back({key, key}); };
    auto down = [&](int key) { s.push_back({key, 0}); };
    auto up = [&](int key) { s.push_back({0, key}); };
    auto wait = [&](int tics)
    {
        for (auto i = 0; i < tics; ++i)
            s.push_back({});
    };

    // followplayer starts true (am_map.cpp's own initializer), and
    // initAutomapVariables primes f_oldloc to DOOM_MAXINT, so the first couple of
    // tics with the map open exercise doFollowPlayer's "snap to the player" path
    // before anything here touches the follow toggle.
    wait(3);

    // Turn following off, then pan in all four directions. automapResponder only
    // sets m_paninc while unfollowed (its own guard sets rc = false otherwise),
    // so the toggle has to happen first or every pan key below would be a no-op.
    tap(FOLLOW);
    down(RIGHT);
    wait(3);
    up(RIGHT);
    down(LEFT);
    wait(3);
    up(LEFT);
    down(UP);
    wait(3);
    up(UP);
    down(DOWN);
    wait(3);
    up(DOWN);

    // Zoom in, then out, each held a few tics so changeWindowScale actually runs
    // (a tap-and-release with no tic between would set and immediately clear the
    // multiplier with the ticker never seeing it).
    down(ZOOMIN);
    wait(3);
    up(ZOOMIN);
    down(ZOOMOUT);
    wait(3);
    up(ZOOMOUT);

    // The grid overlay, on then off.
    tap(GRID);
    wait(1);
    tap(GRID);
    wait(1);

    // The "big" zoomed-all-the-way-out overview: on (minOutWindowScale, having
    // first saveScaleAndLoc'd the panned window) then off (restoreScaleAndLoc).
    // Taken while still unfollowed, so restoreScaleAndLoc's !followplayer branch
    // is the one that runs - it restores the panned m_x/m_y rather than
    // recentring on the player.
    tap(BIG);
    wait(1);
    tap(BIG);
    wait(1);

    // Drop three marks with a tic between each (addMark reads the window's
    // current centre, which the pan/zoom/big steps above kept moving), then
    // clear them all in one go.
    tap(MARK);
    wait(1);
    tap(MARK);
    wait(1);
    tap(MARK);
    wait(1);
    tap(CLEAR);
    wait(1);

    // Follow the player again - f_oldloc is reprimed to DOOM_MAXINT, so this
    // exercises doFollowPlayer's snap a second time, mid-session rather than only
    // on the very first tic the map was open - and let a few plain tics pass.
    // E1M1's monsters keep thinking (P_Random) whether or not the map is open,
    // which is what puts that into the golden.
    tap(FOLLOW);
    wait(5);

    return s;
}

// Doom::GS_LEVEL, as doomSimGameState reports it (the GameState enum's first
// member). The whole script must stay in the level; if gamestate drifted off
// (the level ended, or something reached the title or an intermission) the
// automap would no longer be drawing the world the golden was recorded over.
constexpr auto gsLevel = 0;

// Shareware E1M1 at skill medium - the same fixed, deterministic anchor
// Tests/Sim/ScenarioTests.cpp loads directly: a level shipped with doom1.wad, so
// this asks nothing of the environment beyond what every other Sim test already
// does.
constexpr auto automapEpisode = 1;
constexpr auto automapMap = 1;
constexpr auto automapSkill = 2; // sk_medium

inline Hashes runAutomapScript()
{
    auto frames = Hashes {};

    nano::check(doomSimBoot(0) != 0, "engine booted headless, no demo queued");
    nano::check(doomSimLoadLevel(automapEpisode, automapMap, automapSkill) != 0,
                "E1M1 loaded and the player spawned");

    // The level load wipes exactly as any level transition does (G_DoLoadLevel
    // leaves wipegamestate mismatched against the new gamestate). Run it out
    // before hashing so the golden pins the automap, not the entry wipe.
    doomSimStepTic();

    for (auto guard = 0; doomSimIsWiping() && guard < 200; ++guard)
        doomSimStepTic();

    nano::check(!doomSimIsWiping(), "the level-load wipe finished");
    nano::check(doomSimGameState() == gsLevel, "the level is up");
    nano::check(!doomSimAutomapActive(), "the automap starts closed");

    // Open the map with AM_STARTKEY, and check the responder actually caught it
    // rather than assuming a keypress landed - Doom::gameResponder only reaches
    // Doom::automapResponder while gamestate is GS_LEVEL (Game.cpp,
    // gameResponder), so this doubles as proof the level really is up.
    doomSimPostKeyDown(DOOM_KEY_TAB);
    doomSimPostKeyUp(DOOM_KEY_TAB);
    nano::check(doomSimStepTic() != 0, "the tic ran");
    frames.push_back(doomSimFrameHash());
    nano::check(doomSimAutomapActive() != 0, "AM_STARTKEY opened the automap");

    for (auto step: automapScript())
    {
        if (step.down != 0)
            doomSimPostKeyDown(step.down);
        if (step.up != 0)
            doomSimPostKeyUp(step.up);

        nano::check(doomSimStepTic() != 0, "the tic ran");
        nano::check(doomSimGameState() == gsLevel, "the level stayed up");
        frames.push_back(doomSimFrameHash());
    }

    // Close the map with AM_ENDKEY (the same physical key as AM_STARTKEY) and
    // check it actually closed.
    doomSimPostKeyDown(DOOM_KEY_TAB);
    doomSimPostKeyUp(DOOM_KEY_TAB);
    nano::check(doomSimStepTic() != 0, "the tic ran");
    frames.push_back(doomSimFrameHash());
    nano::check(!doomSimAutomapActive(), "AM_ENDKEY closed the automap");

    nano::check(doomSimGameState() == gsLevel, "the level is still up at the end");

    return frames;
}

inline void checkAutomapMatchesGolden()
{
    auto frames = runAutomapScript();

    nano::check(!frames.empty(), "the automap script drove at least one frame");

    if (updatingGoldens())
    {
        writeGolden("automap",
                    "frames",
                    "the software frame and palette, hashed every tic of a "
                    "scripted automap walk over E1M1.",
                    frames);
        return;
    }

    auto golden = readGolden("automap", "frames");

    if (golden.empty())
    {
        std::printf("\nNo automap golden. Record one with DOOM_UPDATE_GOLDENS=1\n\n");
        nano::check(false, "automap golden exists");
        return;
    }

    const auto shared = std::min(frames.size(), golden.size());

    for (auto i = std::size_t {0}; i < shared; ++i)
    {
        if (frames[i] == golden[i])
            continue;

        std::printf("\nautomap: the rendered frame changed at step %d\n"
                    "  Nothing in a demo opens the automap, so this frame golden\n"
                    "  is UI/Automap.cpp's only net. If the change was intended,\n"
                    "  re-record: DOOM_UPDATE_GOLDENS=1\n\n",
                    (int) i);
        nano::check(false, "automap renderer matches the golden");
        return;
    }

    if (frames.size() != golden.size())
        std::printf("\nautomap.frames: %zu entries, golden has %zu\n\n",
                    frames.size(),
                    golden.size());

    nano::check(frames.size() == golden.size(), "the automap walk is the same length");
}
} // namespace DoomTests
