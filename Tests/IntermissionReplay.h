#pragma once

#include "DemoReplay.h"

#include <DOOM/DOOM.h>

// UI/Intermission.cpp is the between-levels scoreboard - the "Finished!" stat
// count, the you-are-here episode map, and the hidden NoState countdown that
// hands over to the next level - and it was the one screen with no coverage of
// any kind: an attract-mode demo replays a slice of a level and never completes
// it, so no demo golden, and unlike the menu/automap/finale it was the last to
// get a harness of its own. Nothing in the suite had ever run doCompleted,
// Doom::drawIntermission, or doWorldDone.
//
// Unlike the finale harness there is no need to call the screen's entry point
// directly: Doom::exitLevel() is the real thing - the exact call E1M1's exit
// switch makes - so this script drives the whole genuine transition:
//
//   Doom::exitLevel() -> ga_completed -> doCompleted() (fills wminfo, flips
//   gamestate to GS_INTERMISSION, calls Doom::startIntermission) -> the
//   StatCount count-up -> ShowNextLoc -> NoState -> endIntermission() +
//   worldDone() -> ga_worlddone -> doWorldDone() loads E1M2.
//
// Every tic of that renders through Doom::drawIntermission, which is the point:
// this harness began as a sanitizer net, and it earned its keep on its first
// run - ASAN caught drawEL reading the cleared lnames table on the
// intermission's last tic (fixed at unloadIntermissionData, whose comment tells
// the story; checkIntermissionDataOutlivesItsLastDraw below pins it with no
// sanitizer needed).
//
// One script drives that transition (runIntermissionScript) and two tests read
// it differently: checkLevelTransition asserts the state machine and discards
// the frames, checkIntermissionMatchesGolden holds them against
// Tests/Goldens/intermission.frames. Splitting them is what makes a failure say
// which broke - a transition that stops happening is a different bug from a
// scoreboard that draws wrong - and only the second needs a golden to fail.
//
// Input: leaving StatCount requires a fire/use press (updateStats waits on
// acceleratestage at sp_state 10), posted through the real host path
// (Doom::keyDown -> gamekeydown -> buildTiccmd -> ButtonCode::Attack), key down for one
// whole tic and released the next - checkForAccelerate latches attackdown, and
// posting down+up before a single tic would leave gamekeydown already clear by
// the time buildTiccmd reads it. ShowNextLoc and NoState then advance on their
// own timers (SHOWNEXTLOCDELAY * TICRATE = 140 tics, then 10), no input at all.
//
// Determinism: the world stops ticking the moment gamestate leaves GS_LEVEL, so
// after the exit the only randomness consumed is M_Random (the melts, and
// initAnimatedBack's animation phases) - deterministic for the same reason the
// finale golden is. The level tics before the exit are input-free, so the
// simulation runs them identically every time. Measured, not assumed: the golden
// was recorded twice and diffed, and it holds under Apple Clang Debug and
// Release and under GCC 16 Release.

namespace DoomTests
{
// Shareware E1M1 at skill medium - the same fixed anchor ScenarioTests,
// AutomapReplay and FinaleReplay use. E1M1 in particular because the reported
// failure is "finishing the first level", and because gamemap must not be 8:
// doCompleted routes map 8 to ga_victory (the finale) with no intermission.
constexpr auto interEpisode = 1;
constexpr auto interMap = 1;
constexpr auto interSkill = 2; // sk_medium

// Doom::GameState's members, as doomSimGameState reports them.
constexpr auto gsLevel = 0;
constexpr auto gsIntermission = 1;

// Doom::IntermissionPhase, as doomSimIntermissionPhase reports it.
constexpr auto phaseNoState = -1;
constexpr auto phaseStatCount = 0;
constexpr auto phaseShowNextLoc = 1;

// 2,500 tics is 71 seconds of level clock - see the loop that spends them. The
// player is idle for all of them and E1M1's monsters never wake, so they cost
// the suite about a second and change nothing but leveltime.
constexpr auto levelTicsBeforeExit = 2500;

// The natural single-player count-up runs itself to sp_state 10 in ~190 tics
// for an immediate E1M1 exit (five 35-tic pauses dominate; the zero tallies
// snap in a tic or two each), then waits for the player. 250 is comfortably
// past the whole run, and the script asserts it is still waiting afterwards.
constexpr auto statCountTics = 250;

// ShowNextLoc holds for 140 tics, NoState for 10, plus a tic for ga_worlddone
// to be seen; 400 is generous headroom for the drive-until-level loop.
constexpr auto nextLocCeiling = 400;

// Runs a screen melt out, unhashed - the same shape as the other harnesses'
// warm-ups (a separate name from FinaleReplay.h's runWipeUnhashed so the two
// headers stay includable in one translation unit).
inline void runIntermissionWipeOut()
{
    doomSimStepTic();

    for (auto guard = 0; doomSimIsWiping() && guard < 200; ++guard)
        doomSimStepTic();

    nano::check(!doomSimIsWiping(), "the wipe finished");
}

inline void stepOneTic()
{
    nano::check(doomSimStepTic() != 0, "the tic ran");
}

// One fire press, held for exactly one tic: the tic with the key down is the
// one whose ticcmd carries ButtonCode::Attack and fires checkForAccelerate. Both
// of its tics go through the caller's own stepper, so a caller hashing frames
// sees the two the press costs it rather than a gap in the golden.
template <typename StepTic>
inline void pressFire(const StepTic& stepTic)
{
    doomSimPostKeyDown(static_cast<int>(Doom::Key::Ctrl));
    stepTic();
    doomSimPostKeyUp(static_cast<int>(Doom::Key::Ctrl));
    stepTic();
}

inline void pressFire()
{
    pressFire(stepOneTic);
}

// Drives the whole genuine E1M1 -> scoreboard -> E1M2 transition, asserting the
// state machine at every transition, and returns the frame hash of every tic the
// intermission itself drew.
//
// The two things it does NOT hash are the melts either side: the level melting
// into the scoreboard, and the scoreboard melting into E1M2. Those are the
// generic melt over whatever frame preceded them, not anything
// UI/Intermission.cpp draws, and the finale golden warms its entry wipe out for
// the same reason. Everything between them is drawIntermission's own output -
// the count-up, the you-are-here map, the NoState countdown, and the last tic
// that draws after endIntermission has unloaded.
//
// A tic is hashed only if the intermission is still up when it ends, which is
// what keeps the hand-over tic (doWorldDone flips to GS_LEVEL mid-tic and
// displayFrame then draws the new level) out of a golden that exists to pin the
// scoreboard.
inline Hashes runIntermissionScript()
{
    auto frames = Hashes {};

    const auto stepIntermissionTic = [&frames]
    {
        nano::check(doomSimStepTic() != 0, "the tic ran");

        if (doomSimGameState() == gsIntermission)
            frames.push_back(doomSimFrameHash());
    };

    nano::check(doomSimBoot() != 0, "engine booted headless, no demo queued");
    nano::check(doomSimLoadLevel(interEpisode, interMap, interSkill) != 0,
                "E1M1 loaded and the player spawned");

    runIntermissionWipeOut();
    nano::check(doomSimGameState() == gsLevel, "the level is up");

    // Ordinary, input-free level tics before the exit - and enough of them to
    // matter, because they are the only stat this harness can make non-zero.
    // The player idles at E1M1's start, so kills, items and secrets all finish
    // at 0 and their count-ups (updateStats' sp_state 2/4/6, +2 a tic) run but
    // never visibly roll. The clock does not care what the player did: at
    // levelTicsBeforeExit the scoreboard reads "1:11" against E1M1's 0:30 par,
    // so sp_state 8 counts both up three seconds a tic for two dozen tics and
    // drawTime draws a minutes digit. With ten tics it read 0:00 and the only
    // thing on the screen that moved was par.
    for (auto i = 0; i < levelTicsBeforeExit; ++i)
        nano::check(doomSimStepTic() != 0, "the tic ran");

    // Not just "running": past a minute, which is the claim the comment above
    // makes about what the scoreboard will read and the reason the loop is as
    // long as it is. A shortened warm-up would otherwise quietly take the time
    // count-up back to 0:00 and leave the golden pinning less than it says.
    nano::check(doomSimLevelTime() > 60 * 35,
                "the level clock passed a minute before the exit");

    // The real exit: the same call an exit switch makes. The next tic's
    // gameTicker sees ga_completed and runs doCompleted -> startIntermission.
    doomSimExitLevel();
    nano::check(doomSimGameState() == gsLevel,
                "exitLevel only queues; the level stands until the next tic");
    nano::check(doomSimStepTic() != 0, "the tic ran");
    nano::check(doomSimGameState() == gsIntermission,
                "doCompleted switched gamestate to GS_INTERMISSION");
    nano::check(doomSimIntermissionPhase() == phaseStatCount,
                "the intermission opens on the stat count");

    // The level melts into the scoreboard; from here every frame is
    // drawIntermission's.
    runIntermissionWipeOut();
    nano::check(doomSimGameState() == gsIntermission, "still on the scoreboard");

    // The count-up runs itself out (see statCountTics) and then waits at
    // sp_state 10 for a press - it cannot leave StatCount on its own, which the
    // post-loop check pins.
    for (auto i = 0; i < statCountTics; ++i)
    {
        stepIntermissionTic();
        nano::check(doomSimGameState() == gsIntermission,
                    "the count-up stays on the scoreboard");
    }

    nano::check(doomSimIntermissionPhase() == phaseStatCount,
                "the finished count waits for the player");

    pressFire(stepIntermissionTic);
    nano::check(doomSimIntermissionPhase() == phaseShowNextLoc,
                "the press advanced the scoreboard to the you-are-here map");

    // ShowNextLoc and NoState advance on their own timers, NoState's expiry
    // runs endIntermission + worldDone, and doWorldDone loads the next level.
    for (auto guard = 0;
         doomSimGameState() == gsIntermission && guard < nextLocCeiling;
         ++guard)
        stepIntermissionTic();

    nano::check(doomSimGameState() == gsLevel,
                "the intermission ended and gamestate returned to GS_LEVEL");
    nano::check(doomSimGameEpisode() == interEpisode
                    && doomSimGameMap() == interMap + 1,
                "the level the intermission delivered is E1M2");

    // E1M2 arrives through its own melt; run it out and prove the new level
    // actually simulates rather than just loaded.
    runIntermissionWipeOut();

    for (auto i = 0; i < 10; ++i)
        nano::check(doomSimStepTic() != 0, "the tic ran");

    nano::check(doomSimGameState() == gsLevel, "E1M2 is running");
    nano::check(doomSimLevelTime() > 0, "E1M2's level clock is running");

    return frames;
}

// The state machine on its own: the same drive, with the frames discarded. It is
// the test that says *which* of the two broke - a transition that stops
// happening is a different bug from a scoreboard that draws wrong, and this one
// needs no golden to fail.
inline void checkLevelTransition()
{
    const auto frames = runIntermissionScript();

    nano::check(!frames.empty(), "the intermission drew at least one frame");
}

inline void checkIntermissionMatchesGolden()
{
    auto frames = runIntermissionScript();

    nano::check(!frames.empty(), "the intermission script drove at least one frame");

    if (updatingGoldens())
    {
        writeGolden("intermission",
                    "frames",
                    "the software frame and palette, hashed every tic the "
                    "E1M1 -> E1M2 scoreboard drew: the count-up, the "
                    "you-are-here map and the NoState countdown.",
                    frames);
        return;
    }

    auto golden = readGolden("intermission", "frames");

    if (golden.empty())
    {
        std::printf(
            "\nNo intermission golden. Record one with DOOM_UPDATE_GOLDENS=1\n\n");
        nano::check(false, "intermission golden exists");
        return;
    }

    const auto shared = std::min(frames.size(), golden.size());

    for (auto i = std::size_t {0}; i < shared; ++i)
    {
        if (frames[i] == golden[i])
            continue;

        std::printf("\nintermission: the rendered frame changed at step %d\n"
                    "  No demo completes a level, so this frame golden is\n"
                    "  UI/Intermission.cpp's only net. If the change was\n"
                    "  intended, re-record: DOOM_UPDATE_GOLDENS=1\n\n",
                    (int) i);
        nano::check(false, "intermission renderer matches the golden");
        return;
    }

    if (frames.size() != golden.size())
        std::printf("\nintermission.frames: %zu entries, golden has %zu\n\n",
                    frames.size(),
                    golden.size());

    nano::check(frames.size() == golden.size(),
                "the intermission walk is the same length");
}

// Pins the defect the transition first surfaced under ASAN, in a way any build
// can see. The intermission's last tic has a use-after-unload by construction:
// updateNoState's expiry runs endIntermission() - which cleared lnames - and
// worldDone(), but gamestate stays GS_INTERMISSION until the *next* tic's
// doWorldDone, so displayFrame draws one more intermission frame after the
// unload and drawEL reads lnames[wbs->next]. Vanilla had the identical call
// order and survived it because WI_unloadData only marked the lumps purgeable
// (Z_ChangeTag PU_CACHE, memory left readable); an actual clear() turns that
// last draw into a read past the vector's size, which a plain build happens to
// get away with only because clear() keeps the old buffer.
//
// The invariant asserted is the drawer's precondition, not the implementation:
// any tic that ends with gamestate still on the intermission drew it, so the
// level-name table must still be filled when such a tic ends. The script takes
// the fast route to the last tic - one press per stage; each press must be held
// for a whole tic (see pressFire) - and then watches every remaining tic until
// the hand-over.
inline void checkIntermissionDataOutlivesItsLastDraw()
{
    nano::check(doomSimBoot() != 0, "engine booted headless, no demo queued");
    nano::check(doomSimLoadLevel(interEpisode, interMap, interSkill) != 0,
                "E1M1 loaded and the player spawned");

    runIntermissionWipeOut();
    nano::check(doomSimGameState() == gsLevel, "the level is up");

    doomSimExitLevel();
    nano::check(doomSimStepTic() != 0, "the tic ran");
    nano::check(doomSimGameState() == gsIntermission,
                "doCompleted switched gamestate to GS_INTERMISSION");

    runIntermissionWipeOut();
    nano::check(doomSimIntermissionLnameCount() > 0,
                "loadIntermissionData filled the level-name table");

    // Three presses: snap the count-up to its finished state, leave StatCount
    // for the you-are-here map, and cut its 140-tic delay short.
    pressFire();
    nano::check(doomSimIntermissionPhase() == phaseStatCount,
                "the first press only finishes the count");

    pressFire();
    nano::check(doomSimIntermissionPhase() == phaseShowNextLoc,
                "the second press advanced to the you-are-here map");

    pressFire();
    nano::check(doomSimIntermissionPhase() == phaseNoState,
                "the third press advanced to the NoState countdown");

    // NoState's ten tics, the unload-then-draw tic among them, then doWorldDone.
    constexpr auto noStateCeiling = 40;

    for (auto guard = 0;
         doomSimGameState() == gsIntermission && guard < noStateCeiling;
         ++guard)
    {
        nano::check(doomSimStepTic() != 0, "the tic ran");

        if (doomSimGameState() == gsIntermission)
            nano::check(doomSimIntermissionLnameCount() > 0,
                        "a tic that drew the intermission left the level-name "
                        "table alive (endIntermission unloads on the same tic "
                        "drawIntermission still draws)");
    }

    nano::check(doomSimGameState() == gsLevel,
                "the hand-over to the next level happened");
}
} // namespace DoomTests
