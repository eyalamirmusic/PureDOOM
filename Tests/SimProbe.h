#pragma once

#include <string>
#include <string_view>

// A plain-data window into the running simulation, for the tests.
//
// The engine's state lives in globals its own headers declare, so this reaches
// them directly rather than through DOOM.h's host API, which only exposes a
// framebuffer and an input queue. Nothing DOOM-typed leaks out: the tests see
// integers and one hash.

// Boots the engine headless against the shareware WAD and queues a demo
// ("demo1", "demo2", "demo3"), or none at all if demoLump is empty - which is
// what a test of something other than the simulation wants. Returns 0 if the
// engine aborted on the way up - a missing WAD, a bad lump - rather than
// leaving the test to crash in the wreckage.
//
// It boots with -config, pointing the engine at Tests/doom-tests.cfg rather
// than at the developer's ~/.doomrc. That file is not decoration: without
// it, Doom::loadDefaults reads whatever the developer happens to have, and
// screenblocks alone (10 in a real config, 9 by default) changes the shape
// of every rendered frame. The simulation survives that - a demo's input
// comes from the .lmp, not the config - but doomSimFrameHash does not.
//
// Demos are how the simulation gets tested at all: a .lmp is a recording of
// the player's input, one ticcmd per tic, and G_ReadDemoTiccmd feeds the
// game from it instead of from the keyboard. DOOM's simulation is
// deterministic by construction - fixed-point maths and a 256-entry random
// table walked by an index - so the same input must produce the same world,
// tic for tic. Anything that changes the world therefore desyncs the demo,
// which is exactly the property a refactor wants watched.
int doomSimBoot(std::string_view demoLump = {});

// Runs one tic. Returns 0 once the demo has played out.
int doomSimRunTic();

// Queues a demo again, in the same process, WITHOUT re-running
// Doom::initGame.
//
// This is the scenario-test enabler, and it is a smaller claim than "boot the
// engine twice". The engine cannot be re-inited (Z_Init would leak the arena),
// but it does not need to be: Doom::initNewGame -> Doom::setupLevel already resets the
// simulation each time a level loads - the geometry (Doom::Level assigns fresh
// vectors), the thinkers (Doom::initThinkers), the random indices (M_ClearRandom),
// the leftover mobjs and specials (Z_FreeTags). So a second scenario runs on a
// clean world in the same process. Replaying one demo twice and getting the
// same tics both times is what proves that reset is clean.
void doomSimReplayDemo(std::string_view demoLump);

// Whether the demo is actually driving a level yet: the engine spends a few
// tics on the title screen first.
int doomSimInLevel();

// Everything the simulation could plausibly diverge in, hashed: the random
// index, the level clock, the player, and every thinking object in the
// level - monsters, projectiles, dropped items - with its position, facing,
// health and animation frame.
//
// rndindex alone is a remarkably sharp canary. Every P_Random call steps it,
// so a refactor that adds, drops or reorders a single call shifts the whole
// subsequent random sequence and the world diverges within a few tics, even
// if the arithmetic was otherwise perfect.
unsigned long long doomSimStateHash();

// The finished software frame: the 320x200 palette-indexed picture the
// engine just drew (screens[0]), and the palette it would be resolved
// through - which is not a constant, the damage, pickup and invulnerability
// flashes all being palette swaps.
//
// The demos already render: Doom::doomLoop calls Doom::displayFrame every tic, so the
// software renderer has been running throughout the existing suite with
// nobody looking at what it produced. This is that missing assertion, and it
// is what pins r_*, the status bar and the HUD through the refactor the way
// doomSimStateHash pins the simulation.
//
// It is deterministic for the same reason the simulation is. The one thing
// that could have spoiled it - the screen melt - does not: PureDOOM moved
// the wipe out of vanilla's busy-wait on the wall clock (still visible,
// #if 0'd, in Doom::displayFrame) and into Doom::updateWipe, which advances it by exactly
// one tic per call.
unsigned long long doomSimFrameHash();

// Allocation accounting, for the RAII net. doomSimCountAllocations installs a
// counting malloc/free pair through the public Doom::host() and must be called
// before doomSimBoot, so that every block the engine takes is one this counter
// saw; doomSimLiveAllocations is then the number outstanding. Together they let
// a test assert what no golden can see - that destroying an Engine gives the
// memory back - which is the only check that reaches an owner whose whole job
// is to release something.
void doomSimCountAllocations();
int doomSimLiveAllocations();

// The WAD directory as the reader sees it, so that tearing the zone
// allocator out from under Doom::cacheLumpNum has something to answer to. The
// hash is over the lump's bytes.
int doomSimLumpCount();
std::string doomSimLumpName(int lump);
int doomSimLumpSize(int lump);
unsigned long long doomSimLumpHash(int lump);

// Whether the level geometry the engine renders from is owned by Doom::Level
// and the vanilla globals (vertexes, numsegs, ...) are consistent views onto
// it: every pointer equal to its vector's data(), every count equal to its
// vector's size(). The transitional design leans on a loader refreshing the
// global after every resize; this catches one that forgot.
int doomSimGeometryViewsConsistent();

// Individual probes, so a failure can say what actually differs rather than
// just that two hashes do.
int doomSimRndIndex();
int doomSimLevelTime();
int doomSimPlayerHealth();
int doomSimPlayerX();
int doomSimPlayerY();
int doomSimPlayerAngleDegrees();
int doomSimMobjCount();

// --- The scenario harness (Step 6) ---------------------------------------
//
// The demos exercise Doom::tryMove and Doom::checkPosition thousands of times, but
// only in aggregate: when one desyncs, the tic hash says the world moved, not
// which playsim call answered wrong. These drive the playsim directly - load a
// level, place a thing, ask whether a spot is legal, try to move onto it - so a
// scenario test can pin one collision fact in isolation and give the rewrite of
// p_map somewhere to fail *loudly and locally*.
//
// It rests on the multi-scenario capability proven in Step 4: a level load
// resets the whole simulation cleanly (Doom::Level reassigns its geometry,
// Z_FreeTags clears the mobjs), so a scenario runs on a fresh world in the same
// process. Handles are plain ints into a probe-side registry, so nothing
// DOOM-typed leaks through this header; a level load invalidates every handle
// (its mobjs are freed) and re-registers the player as handle 0.
//
// Coordinates are raw 16.16 fixed-point (fixed_t is int32), so a test can place
// and read a thing to the exact unit rather than the truncated whole units the
// human-readable player probes above report.

// Loads episode/map at the given skill directly (Doom::initNewGame), no demo, and
// forces single-player so the player mobj spawns. Returns 1 on success, 0 if
// the engine aborted. Must be called after doomSimBoot().
int doomSimLoadLevel(int episode, int map, int skill);

// The player-1 mobj as a handle (0), or -1 if no level is loaded.
int doomSimPlayerHandle();

// Spawns a mobj of `type` at fixed-point (x, y, z) - z may be the ONFLOORZ
// sentinel from doomSimOnFloorZ() - links it into the world, and returns a
// handle (>= 0), or -1 if the spawn aborted.
int doomSimSpawnMobj(int type, int x, int y, int z);

// Doom::checkPosition (does the thing fit at (x, y)?) and Doom::tryMove (fit, then
// commit the move and cross any special lines) for a handle's mobj. Both
// return 1 if the answer was yes, 0 if no or the handle was bad. TryMove
// leaves the mobj where it was when it answers 0.
int doomSimCheckPosition(int handle, int x, int y);
int doomSimTryMove(int handle, int x, int y);

// A handle's mobj position, raw fixed-point.
int doomSimMobjX(int handle);
int doomSimMobjY(int handle);
int doomSimMobjZ(int handle);

// A handle's flags word, and a blunt setter - enough for a scenario to toggle
// MF_NOCLIP and watch the collision early-out take effect.
int doomSimMobjFlags(int handle);
void doomSimSetMobjFlags(int handle, int flags);

// The stateful map-utility functions, exposed so a scenario can pin them with
// locality rather than only through the collision they feed. A thing is linked
// into a blockmap cell (Doom::setThingPosition) and found there by the iterator
// (Doom::forEachThingInBlock); unlinking it (Doom::unsetThingPosition) takes it back
// out. doomSimThingsInBlockOf counts what the iterator finds in the cell that
// holds a handle's mobj, so a test reads the link/unlink as a count that moves.
int doomSimThingsInBlockOf(int handle);
void doomSimUnsetThingPosition(int handle);
void doomSimSetThingPosition(int handle);

// Named constants, so a scenario test stays free of DOOM's enums (info.h,
// p_mobj.h). Add more here as scenarios need them.
int doomSimTypeBarrel(); // Doom::MT_BARREL: solid, shootable, radius 10
int doomSimOnFloorZ(); // ONFLOORZ, the "rest on the floor" spawn z
int doomSimFlagNoClip(); // MF_NOCLIP

// Archives the live world (players/sectors/thinkers/specials), reloads a
// fresh base level and unarchives over it - the exact p_saveg round trip
// gDoLoadGame runs - and returns 1 iff the world hash is unchanged. The one
// simulation path no demo golden covers, and precisely the mobj/special byte
// layout the Doom::Thinker->Thinker step rewrites: the net that must exist before
// it. Requires a level to be loaded (doomSimLoadLevel) with the world set up.
int doomSimSaveLoadPreservesWorld();

// --- The menu/UI harness (Step 8) ----------------------------------------
//
// m_menu is the one part of the engine no demo covers - nothing in a .lmp
// opens a menu - so before it is rewritten it gets its own frame golden, the
// same way Step 0 gave the renderer one: drive synthetic key events through
// the real host path (Doom::keyDown -> Doom::postEvent -> Doom::menuResponder), let
// Doom::menuTicker and Doom::drawMenu run, and hash the finished software frame.
//
// The background is the attract-mode title screen (TITLEPIC): a static,
// deterministic picture, so what the golden pins is the menu drawn over it
// and nothing else. It rests on the same pinned -config as doomSimBoot, so
// the frame means the same thing on every machine.

// Boots to the title screen instead of queuing a demo: leaves the attract
// loop's advancedemo flag up so the first tic brings up TITLEPIC. Returns 0
// if the engine aborted. Must be the only boot in the process, like
// doomSimBoot.
int doomSimBootToTitle();

// Doom::Post a synthetic key event, exactly as the host's Doom::keyDown/keyUp
// do. The key is a Doom::Key value from DOOM/DOOM.h - the public host API a
// menu is driven through, so the menu test names its keys from that header.
void doomSimPostKeyDown(int key);
void doomSimPostKeyUp(int key);

// Run one tic unconditionally (Doom::forceUpdateGame), with no demo
// bookkeeping.
// While a screen melt is in flight this advances the melt instead of the
// game loop, exactly as the host does. Returns 1 normally, 0 if the engine
// aborted (e.g. a menu action reached Doom::quitGame).
int doomSimStepTic();

// Whether a screen melt is animating right now. The title screen arrives
// through a wipe, so the menu harness runs warm-up tics until this clears
// before it starts hashing - the golden pins the menu, not the entry wipe.
int doomSimIsWiping();

// The current gamestate as a small int (Doom::GS_LEVEL=0 .. Doom::GS_DEMOSCREEN=3), so
// the harness can assert the background stayed the title screen for the whole
// script (the attract loop would otherwise advance to a demo after ~170
// tics), and whether the menu is currently open.
int doomSimGameState();
int doomSimMenuActive();

// --- The automap harness --------------------------------------------------
//
// UI/Automap.cpp is the map window, the pan/zoom, the BSP-less line walk and
// the mark points, and it has no demo coverage either: nothing in a .lmp
// opens it. Unlike the menu it needs an actual level rather than the title
// screen, so the harness loads one (doomSimLoadLevel above) and drives
// AM_STARTKEY and its siblings through the same synthetic Doom::keyDown/keyUp
// path the menu harness uses. This is the one thing that path does not
// already give it: whether the responder actually caught a keypress and
// opened the map, so a script that presses the wrong key - or catches the
// engine in the wrong gamestate, where Doom::gameResponder never reaches
// Doom::automapResponder at all - fails loudly instead of silently recording
// a golden of nothing happening.

// Whether the automap is open right now (Doom::OverlayState's
// automapactive) - the same shape as doomSimMenuActive above.
int doomSimAutomapActive();

// --- The finale harness ----------------------------------------------------
//
// UI/Finale.cpp is the end-of-episode text crawl, the following art/credit
// screen, and (DOOM II only) the character cast call - and it has no demo
// coverage either: an attract-mode demo plays a slice of a level, never a
// whole episode through to its exit. Reaching a finale the honest way means
// completing a level, which is not tractable to drive here, so the harness
// takes the tractable route instead: load a level directly (doomSimLoadLevel
// above) so gameepisode/gamemap are set, then call Doom::startFinale()
// directly - the same entry point Game.cpp's ga_victory case calls - and let
// Doom::finaleTicker/Doom::drawFinale run off doomSimStepTic exactly as the
// level loop would run them.

// Calls Doom::startFinale() directly. Must be called after doomSimLoadLevel,
// whose gameepisode/gamemap choose the finale's text and background flat.
void doomSimStartFinale();

// Doom::FinaleState's finalestage (0 = text crawl, 1 = the art/credit screen,
// 2 = the DOOM II cast call), so a script can drive off the engine's own
// transition rather than a hard-coded tic count.
int doomSimFinaleStage();

// Doom::GameVersion's gamemode (Doom::GameMode: shareware=0, registered=1,
// commercial=2, retail=3, indetermined=4), so a test can check which finale
// branches are reachable from the booted IWAD rather than assume it.
int doomSimGameMode();

// --- The intermission harness ---------------------------------------------
//
// UI/Intermission.cpp is the between-levels scoreboard, and it is the one
// screen with no coverage of any kind: an attract demo replays a slice of a
// level and never completes it, so nothing drives doCompleted ->
// Doom::startIntermission -> Doom::drawIntermission. Unlike the finale there
// is no need to call the screen's entry point directly: Doom::exitLevel() is
// the real thing - the same call an exit switch makes - and everything from
// there (doCompleted's wminfo fill, the scoreboard, worldDone loading the
// next level) runs off doomSimStepTic exactly as the game loop would.

// Calls Doom::exitLevel(), which queues ga_completed for the next tic's
// gameTicker. Must be called with a level loaded (doomSimLoadLevel).
void doomSimExitLevel();

// IntermissionState's phase (Doom::IntermissionPhase: NoState=-1,
// StatCount=0, ShowNextLoc=1), so a script can drive off the engine's own
// transitions rather than hard-coded tic counts.
int doomSimIntermissionPhase();

// The size of IntermissionState's lnames - the per-level name patch table
// loadIntermissionData fills and drawEL reads - so a test can pin that the
// table outlives the intermission's *last* draw. That is not a given:
// updateNoState's expiry runs endIntermission before gamestate leaves
// GS_INTERMISSION, and displayFrame still draws that same tic.
int doomSimIntermissionLnameCount();

// GameSession's gameepisode/gamemap, so a transition test can assert the
// intermission actually delivered the next level rather than assume it.
int doomSimGameEpisode();
int doomSimGameMap();

// The texture table as Doom::initTextures decoded it out of TEXTURE1. The
// lump packs records at whatever byte offset its directory names - usually
// not aligned for the record's int fields (BIGDOOR1's sits at offset % 4 ==
// 2) - so the decode reads through aligned copies, and these let a test hold
// the parsed table against values read from the lump by hand. All must be
// called after doomSimBoot; the name lookup is fatal, not -1, on a missing
// name, exactly as the engine's own lookups are.
int doomSimTextureCount();
int doomSimTextureNumForName(std::string_view name);
int doomSimTextureWidth(int texture);
int doomSimTextureHeight(int texture);
int doomSimTexturePatchCount(int texture);
int doomSimTexturePatchOriginX(int texture, int patch);
int doomSimTexturePatchOriginY(int texture, int patch);

// What Doom::readFile made of a path. It fills an EA::Vector<byte> the caller
// owns, which a test cannot see, so the three facts worth asserting come back
// as plain ints: the length readFile returned, the size of the owner it filled
// (the two must agree), and whether the bytes start with the IWAD magic (so a
// zeroed buffer of the right size cannot pass).
typedef struct DoomSimFileRead
{
    int length;
    int ownerSize;
    int magicIsIwad;
} DoomSimFileRead;

// Zeroed if the engine aborted - readFile is fatal, not false, on a bad path.
// Must be called after doomSimBoot().
DoomSimFileRead doomSimReadFileIntoOwner(std::string_view path);
