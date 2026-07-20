#pragma once

#include "DemoReplay.h"

#include <DOOM/DOOM.h>

// UI/Intermission.cpp is the between-levels scoreboard - the "Finished!" stat
// count, the you-are-here episode map, and the hidden NoState countdown that
// hands over to the next level - and it is the one screen with no coverage of
// any kind: an attract-mode demo replays a slice of a level and never completes
// it, so no demo golden, and unlike the menu/automap/finale it never got its own
// harness either. Nothing in the suite has ever run doCompleted,
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
// sanitizer needed). It asserts the state machine's transitions rather than
// pinning frames; a frame golden is the natural follow-up now that the
// sanitizers run clean through it.
//
// Input: leaving StatCount requires a fire/use press (updateStats waits on
// acceleratestage at sp_state 10), posted through the real host path
// (Doom::keyDown -> gamekeydown -> buildTiccmd -> BT_ATTACK), key down for one
// whole tic and released the next - checkForAccelerate latches attackdown, and
// posting down+up before a single tic would leave gamekeydown already clear by
// the time buildTiccmd reads it. ShowNextLoc and NoState then advance on their
// own timers (SHOWNEXTLOCDELAY * TICRATE = 140 tics, then 10), no input at all.
//
// Determinism: the world stops ticking the moment gamestate leaves GS_LEVEL, so
// after the exit the only randomness consumed is M_Random (the melts, and
// initAnimatedBack's animation phases) - deterministic for the same reason the
// finale golden is. The ten level tics before the exit are input-free, so the
// simulation runs them identically every time.

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

// One fire press, held for exactly one tic: the tic with the key down is the
// one whose ticcmd carries BT_ATTACK and fires checkForAccelerate.
inline void pressFire()
{
    doomSimPostKeyDown(Doom::DOOM_KEY_CTRL);
    nano::check(doomSimStepTic() != 0, "the tic ran");
    doomSimPostKeyUp(Doom::DOOM_KEY_CTRL);
    nano::check(doomSimStepTic() != 0, "the tic ran");
}

inline void checkLevelTransition()
{
    nano::check(doomSimBoot() != 0, "engine booted headless, no demo queued");
    nano::check(doomSimLoadLevel(interEpisode, interMap, interSkill) != 0,
                "E1M1 loaded and the player spawned");

    runIntermissionWipeOut();
    nano::check(doomSimGameState() == gsLevel, "the level is up");

    // A handful of ordinary level tics, so leveltime moves and the scoreboard's
    // time count has a real level clock behind it.
    for (auto i = 0; i < 10; ++i)
        nano::check(doomSimStepTic() != 0, "the tic ran");

    nano::check(doomSimLevelTime() > 0, "the level clock is running");

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
        nano::check(doomSimStepTic() != 0, "the tic ran");
        nano::check(doomSimGameState() == gsIntermission,
                    "the count-up stays on the scoreboard");
    }

    nano::check(doomSimIntermissionPhase() == phaseStatCount,
                "the finished count waits for the player");

    pressFire();
    nano::check(doomSimIntermissionPhase() == phaseShowNextLoc,
                "the press advanced the scoreboard to the you-are-here map");

    // ShowNextLoc and NoState advance on their own timers, NoState's expiry
    // runs endIntermission + worldDone, and doWorldDone loads the next level.
    for (auto guard = 0;
         doomSimGameState() == gsIntermission && guard < nextLocCeiling;
         ++guard)
        nano::check(doomSimStepTic() != 0, "the tic ran");

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
