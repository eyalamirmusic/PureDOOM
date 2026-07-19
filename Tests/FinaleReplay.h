#pragma once

#include "DemoReplay.h"

#include <DOOM/DOOM.h>

#include <string>
#include <vector>

// UI/Finale.cpp is the end-of-episode text crawl, the following art/credit
// screen, and (DOOM II only) the character cast call, and like the automap and
// the menu before it, no demo reaches it: an attract-mode demo replays a slice
// of a level's play, never a whole episode through to its exit. StateClusterTests
// only checks finaleState()'s accessor identity, not anything UI/Finale.cpp
// actually draws.
//
// Reaching a finale the honest way means completing a level - killing every
// monster or finding an exit switch and walking to it - which is not tractable
// to drive here, so this harness takes the tractable stand-in the automap golden
// set the precedent for: doomSimStartFinale calls Doom::startFinale() directly
// (the same entry point Game.cpp's ga_victory case calls from doCompleted), and
// Doom::finaleTicker/Doom::drawFinale then run off doomSimStepTic exactly as the
// ordinary level loop would run them - no synthetic input at all, since
// Doom::finaleResponder only does anything once finalestage reaches 2 (see
// below), so the whole thing this harness drives is timer-driven.
//
// E1M8 (gameepisode=1, gamemap=8) is loaded first with doomSimLoadLevel. That is
// not an arbitrary choice: Doom::startFinale reads gameepisode/gamemap to choose
// the finale's text and background flat, and Game.cpp's doCompleted only reaches
// ga_victory - the path that calls startFinale with no intermission screen in
// between - when gamemap==8 in a non-commercial IWAD (the shareware WAD this
// suite boots is DOOM 1). That is what selects E1TEXT and FLOOR4_8 out of
// Doom::startFinale's switch, and HELP2 (not CREDIT - that needs `retail`, and
// this is `shareware`) out of Doom::drawFinale's.
//
// What is deliberately NOT covered, and why:
//  - The DOOM II cast call (castTicker/castDrawer/castResponder, finalestage 2).
//    Doom::finaleTicker only reaches startCast() when gameVersion().gamemode ==
//    commercial; the shareware WAD this suite boots never is, which the harness
//    checks with doomSimGameMode() rather than assuming.
//  - The DOOM II bunny scroll (bunnyScroll(), gameepisode==3's art stage), and
//    every other episode's art screen (VICTORY2, ENDPIC, CREDIT). E1M8 fixes
//    gameepisode at 1, which draws HELP2.
//  Both are DOOM II or another episode's content, permanently unreachable from
//  the shareware IWAD this whole suite is built around (Tests/SimProbe.cpp's
//  simConfigFile boots doom1.wad, same as every other Sim test).
//
// Determinism: Doom::ticker() - the world simulation, P_Random's caller - does
// not run at all while gamestate is GS_FINALE (Game.cpp's gameTicker only calls
// Doom::finaleTicker for that state), so nothing this harness drives touches
// P_Random. The screen melt does: Doom::initMelt seeds its column offsets from
// Doom::randomness().forMenu() (M_Random), and both of the finale's own wipes -
// the level fading into the finale, and the text fading into HELP2 - run it.
// M_Random is exactly as deterministic as P_Random (the same fixed table,
// walked by its own index), so this changes nothing about reproducibility; it
// is why Tests/Sim/FinaleTests.cpp was run twice before this golden was trusted
// (see its own comment).

namespace DoomTests
{
// Shareware E1M8 at skill medium - see the file comment for why this map in
// particular. Skill medium matches the fixed, deterministic anchor
// Tests/Sim/ScenarioTests.cpp and AutomapReplay.h both use.
constexpr auto finaleEpisode = 1;
constexpr auto finaleMap = 8;
constexpr auto finaleSkill = 2; // sk_medium

// Doom::GS_LEVEL / Doom::GS_FINALE, as doomSimGameState reports them (the
// Doom::GameState enum's first and third members).
constexpr auto gsLevel = 0;
constexpr auto gsFinale = 2;

// Doom::GameMode's shareware, as doomSimGameMode reports it (the enum's first
// member) - what doom1.wad identifies as.
constexpr auto gmShareware = 0;

// Doom::FinaleState's finalestage: 0 = text crawl, 1 = art/credit screen,
// 2 = DOOM II cast call (unreachable here - see the file comment).
constexpr auto finaleStageText = 0;

// Runs a screen melt out, without hashing it: both the level-load wipe (before
// the finale starts) and the entry wipe into the finale itself are the generic
// melt algorithm, not anything UI/Finale.cpp draws - the automap and menu
// harnesses warm the same kind of wipe out unhashed for the same reason. The
// stage-transition wipe (text -> HELP2) is different: that one IS part of what
// this golden pins, and Doom::finaleTicker does not run while is_wiping_screen
// is set (doom_force_update calls Doom::updateWipe instead of Doom::doomLoop),
// so it costs nothing to leave it alone and hash it with everything else.
inline void runWipeUnhashed()
{
    doomSimStepTic();

    for (auto guard = 0; doomSimIsWiping() && guard < 200; ++guard)
        doomSimStepTic();

    nano::check(!doomSimIsWiping(), "the wipe finished");
}

inline Hashes runFinaleScript()
{
    auto frames = Hashes {};

    nano::check(doomSimBoot(0) != 0, "engine booted headless, no demo queued");
    nano::check(doomSimGameMode() == gmShareware,
                "the booted IWAD is shareware - the cast call and the other "
                "episodes' art screens are unreachable from it");
    nano::check(doomSimLoadLevel(finaleEpisode, finaleMap, finaleSkill) != 0,
                "E1M8 loaded and the player spawned");

    // The level load wipes exactly as any level transition does; run it out
    // before starting the finale so the golden pins the finale, not the level's
    // own entry wipe (the same shape AutomapReplay.h and MenuReplay.h use).
    runWipeUnhashed();
    nano::check(doomSimGameState() == gsLevel, "the level is up");

    doomSimStartFinale();
    nano::check(doomSimGameState() == gsFinale,
                "Doom::startFinale switched gamestate to GS_FINALE");
    nano::check(doomSimFinaleStage() == finaleStageText,
                "the finale opens on the text crawl");

    // The entry wipe (the level's last frame melting into the finale's first) -
    // warmed up unhashed; see runWipeUnhashed's comment.
    runWipeUnhashed();
    nano::check(doomSimFinaleStage() == finaleStageText,
                "still on the text crawl after the entry wipe");

    // From here every tic is hashed. E1TEXT is long enough that
    // Doom::finaleTicker does not advance finalestage off the text crawl for
    // about 1,570 tics (doom_strlen(finaletext) * TEXTSPEED + TEXTWAIT) - this
    // loop drives however many that turns out to be, off the engine's own
    // finalestage rather than a hard-coded count, with headroom against a loop
    // that never advances.
    constexpr auto crawlCeiling = 3000;

    for (auto guard = 0;
         doomSimFinaleStage() == finaleStageText && guard < crawlCeiling;
         ++guard)
    {
        nano::check(doomSimStepTic() != 0, "the tic ran");
        nano::check(doomSimGameState() == gsFinale, "gamestate stayed GS_FINALE");
        frames.push_back(doomSimFrameHash());
    }

    nano::check(doomSimFinaleStage() != finaleStageText,
                "the text crawl finished and the stage advanced");

    // The stage-transition wipe (text -> HELP2) - hashed, not warmed up, because
    // this is the transition the golden exists to pin.
    for (auto guard = 0; doomSimIsWiping() && guard < 200; ++guard)
    {
        nano::check(doomSimStepTic() != 0, "the tic ran");
        frames.push_back(doomSimFrameHash());
    }

    nano::check(!doomSimIsWiping(), "the text -> HELP2 wipe finished");

    // A handful of tics of the settled HELP2 screen.
    for (auto i = 0; i < 15; ++i)
    {
        nano::check(doomSimStepTic() != 0, "the tic ran");
        frames.push_back(doomSimFrameHash());
    }

    nano::check(doomSimGameState() == gsFinale, "the finale is still up at the end");

    return frames;
}

inline void checkFinaleMatchesGolden()
{
    auto frames = runFinaleScript();

    nano::check(!frames.empty(), "the finale script drove at least one frame");

    if (updatingGoldens())
    {
        writeGolden("finale",
                    "frames",
                    "the software frame and palette, hashed every tic of "
                    "E1M8's finale: the text crawl, the transition, and the "
                    "HELP2 screen that follows it.",
                    frames);
        return;
    }

    auto golden = readGolden("finale", "frames");

    if (golden.empty())
    {
        std::printf("\nNo finale golden. Record one with DOOM_UPDATE_GOLDENS=1\n\n");
        nano::check(false, "finale golden exists");
        return;
    }

    const auto shared = std::min(frames.size(), golden.size());

    for (auto i = std::size_t {0}; i < shared; ++i)
    {
        if (frames[i] == golden[i])
            continue;

        std::printf("\nfinale: the rendered frame changed at step %d\n"
                    "  No demo reaches a finale, so this frame golden is\n"
                    "  UI/Finale.cpp's only net. If the change was intended,\n"
                    "  re-record: DOOM_UPDATE_GOLDENS=1\n\n",
                    (int) i);
        nano::check(false, "finale renderer matches the golden");
        return;
    }

    if (frames.size() != golden.size())
        std::printf("\nfinale.frames: %zu entries, golden has %zu\n\n",
                    frames.size(),
                    golden.size());

    nano::check(frames.size() == golden.size(),
                "the finale walk is the same length");
}
} // namespace DoomTests
