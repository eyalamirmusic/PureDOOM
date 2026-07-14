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
    // ("demo1", "demo2", "demo3"). Returns 0 if the engine aborted on the way
    // up - a missing WAD, a bad lump - rather than leaving the test to crash in
    // the wreckage.
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
