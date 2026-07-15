#pragma once

// A plain-C window into the running simulation, for the tests.
//
// The engine's state lives in globals its own headers declare, so this reaches
// them directly rather than through DOOM.h's host API, which only exposes a
// framebuffer and an input queue. Nothing DOOM-typed leaks out: the tests see
// integers and one hash.

#ifdef __cplusplus
extern "C"
{
#endif

    // Boots the engine headless against the shareware WAD and queues a demo
    // ("demo1", "demo2", "demo3"), or none at all if demoLump is null - which is
    // what a test of something other than the simulation wants. Returns 0 if the
    // engine aborted on the way up - a missing WAD, a bad lump - rather than
    // leaving the test to crash in the wreckage.
    //
    // It boots with -config, pointing the engine at Tests/doom-tests.cfg rather
    // than at the developer's ~/.doomrc. That file is not decoration: without
    // it, M_LoadDefaults reads whatever the developer happens to have, and
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
    int doomSimBoot(const char* demoLump);

    // Runs one tic. Returns 0 once the demo has played out.
    int doomSimRunTic(void);

    // Queues a demo again, in the same process, WITHOUT re-running doom_init.
    //
    // This is the scenario-test enabler, and it is a smaller claim than "boot the
    // engine twice". The engine cannot be re-inited (Z_Init would leak the arena),
    // but it does not need to be: G_InitNew -> P_SetupLevel already resets the
    // simulation each time a level loads - the geometry (Doom::Level assigns fresh
    // vectors), the thinkers (P_InitThinkers), the random indices (M_ClearRandom),
    // the leftover mobjs and specials (Z_FreeTags). So a second scenario runs on a
    // clean world in the same process. Replaying one demo twice and getting the
    // same tics both times is what proves that reset is clean.
    void doomSimReplayDemo(const char* demoLump);

    // Whether the demo is actually driving a level yet: the engine spends a few
    // tics on the title screen first.
    int doomSimInLevel(void);

    // Everything the simulation could plausibly diverge in, hashed: the random
    // index, the level clock, the player, and every thinking object in the
    // level - monsters, projectiles, dropped items - with its position, facing,
    // health and animation frame.
    //
    // rndindex alone is a remarkably sharp canary. Every P_Random call steps it,
    // so a refactor that adds, drops or reorders a single call shifts the whole
    // subsequent random sequence and the world diverges within a few tics, even
    // if the arithmetic was otherwise perfect.
    unsigned long long doomSimStateHash(void);

    // The finished software frame: the 320x200 palette-indexed picture the
    // engine just drew (screens[0]), and the palette it would be resolved
    // through - which is not a constant, the damage, pickup and invulnerability
    // flashes all being palette swaps.
    //
    // The demos already render: D_DoomLoop calls D_Display every tic, so the
    // software renderer has been running throughout the existing suite with
    // nobody looking at what it produced. This is that missing assertion, and it
    // is what pins r_*, the status bar and the HUD through the refactor the way
    // doomSimStateHash pins the simulation.
    //
    // It is deterministic for the same reason the simulation is. The one thing
    // that could have spoiled it - the screen melt - does not: PureDOOM moved
    // the wipe out of vanilla's busy-wait on the wall clock (still visible,
    // #if 0'd, in D_Display) and into D_UpdateWipe, which advances it by exactly
    // one tic per call.
    unsigned long long doomSimFrameHash(void);

    // The WAD directory as the reader sees it, so that tearing the zone
    // allocator out from under W_CacheLumpNum has something to answer to. The
    // hash is over the lump's bytes.
    int doomSimLumpCount(void);
    void doomSimLumpName(int lump, char* nameOut);
    int doomSimLumpSize(int lump);
    unsigned long long doomSimLumpHash(int lump);

    // Whether the level geometry the engine renders from is owned by Doom::Level
    // and the vanilla globals (vertexes, numsegs, ...) are consistent views onto
    // it: every pointer equal to its vector's data(), every count equal to its
    // vector's size(). The transitional design leans on a loader refreshing the
    // global after every resize; this catches one that forgot.
    int doomSimGeometryViewsConsistent(void);

    // Individual probes, so a failure can say what actually differs rather than
    // just that two hashes do.
    int doomSimRndIndex(void);
    int doomSimLevelTime(void);
    int doomSimPlayerHealth(void);
    int doomSimPlayerX(void);
    int doomSimPlayerY(void);
    int doomSimPlayerAngleDegrees(void);
    int doomSimMobjCount(void);

    // --- The scenario harness (Step 6) ---------------------------------------
    //
    // The demos exercise P_TryMove and P_CheckPosition thousands of times, but
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

    // Loads episode/map at the given skill directly (G_InitNew), no demo, and
    // forces single-player so the player mobj spawns. Returns 1 on success, 0 if
    // the engine aborted. Must be called after doomSimBoot(0).
    int doomSimLoadLevel(int episode, int map, int skill);

    // The player-1 mobj as a handle (0), or -1 if no level is loaded.
    int doomSimPlayerHandle(void);

    // Spawns a mobj of `type` at fixed-point (x, y, z) - z may be the ONFLOORZ
    // sentinel from doomSimOnFloorZ() - links it into the world, and returns a
    // handle (>= 0), or -1 if the spawn aborted.
    int doomSimSpawnMobj(int type, int x, int y, int z);

    // P_CheckPosition (does the thing fit at (x, y)?) and P_TryMove (fit, then
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

    // Named constants, so a scenario test stays free of DOOM's enums (info.h,
    // p_mobj.h). Add more here as scenarios need them.
    int doomSimTypeBarrel(void); // MT_BARREL: solid, shootable, radius 10
    int doomSimOnFloorZ(void);   // ONFLOORZ, the "rest on the floor" spawn z
    int doomSimFlagNoClip(void); // MF_NOCLIP

#ifdef __cplusplus
}
#endif
