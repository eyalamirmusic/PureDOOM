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

#ifdef __cplusplus
}
#endif
