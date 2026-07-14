#include "SimProbe.h"

#include <DOOM/DOOM.h>

#include <DOOM/doomstat.h>
#include <DOOM/d_main.h>
#include <DOOM/g_game.h>
#include <DOOM/info.h>
#include <DOOM/m_random.h>
#include <DOOM/p_local.h>
#include <DOOM/p_mobj.h>
#include <DOOM/tables.h>

#include <setjmp.h>
#include <stdio.h>

// I_Error reports through doom_exit, which by default takes the process with
// it. A test wants the failure, not the corpse, so the engine is entered under
// a setjmp and an abort unwinds back out to the caller.
static jmp_buf simAbort;
static int simBooted;

static void simOnExit(int code)
{
    (void) code;
    longjmp(simAbort, 1);
}

// The engine narrates its startup at length, which drowns the test output.
static void simOnPrint(const char* text) { (void) text; }

int doomSimBoot(const char* demoLump)
{
    char program[] = "doom-tests";
    char* argv[] = {program};

    // The engine is several hundred globals and a zone allocator, and doom_init
    // does not undo any of it: a second boot in one process quietly simulates
    // nothing. Each test therefore needs a process of its own, which is what
    // NanoTest's ctest integration gives it - one case per test, re-running the
    // binary with --test. Running the binary bare puts every test in one
    // process, so say so rather than record an empty golden.
    if (simBooted)
    {
        // Not through doom_print: the first boot silenced it.
        fputs("\nThe engine cannot be booted twice in one process.\n"
              "Run these through ctest, which gives each test its own:\n"
              "    ctest --test-dir build-tests -R Sim\n"
              "or one at a time:  ./SimTests --test Sim/demo1\n\n",
              stderr);
        return 0;
    }

    if (setjmp(simAbort))
        return 0;

    doom_set_exit(simOnExit);
    doom_set_print(simOnPrint);

    doom_init(1, argv, 0);

    // doom_init ends in D_StartTitle, which raises the attract loop's flag. Left
    // alone, D_DoAdvanceDemo runs on the very first tic, clears gameaction and
    // starts the title sequence - which then plays demo1, the credits, demo2 and
    // so on, whatever we asked for. Lower the flag and the game runs only the
    // demo we hand it.
    advancedemo = false;

    // Deliberately NOT -playdemo. That sets `singledemo`, which ends the demo
    // through I_Quit - and I_Quit calls M_SaveDefaults, which would have every
    // test run scribble on the developer's ~/.doomrc. Deferring the demo by
    // hand lets the engine retire it the ordinary way, by clearing
    // demoplayback, and touches nothing outside the process.
    G_DeferedPlayDemo((char*) demoLump);

    simBooted = 1;
    return 1;
}

int doomSimInLevel(void) { return demoplayback && gamestate == GS_LEVEL; }

// The demo is deferred, so it is not playing on the tic that queues it. "Not
// playing" therefore only means "finished" once it has actually started.
static int simDemoStarted;

int doomSimRunTic(void)
{
    if (setjmp(simAbort))
        return 0;

    doom_force_update();

    if (demoplayback)
    {
        simDemoStarted = 1;
        return 1;
    }

    return !simDemoStarted;
}

static unsigned long long simHash;

static void simMix(const void* bytes, int count)
{
    const unsigned char* p = (const unsigned char*) bytes;
    int i;

    for (i = 0; i < count; ++i)
    {
        simHash ^= p[i];
        simHash *= 1099511628211ULL;
    }
}

static int simIsMobj(thinker_t* thinker)
{
    return thinker->function.acp1 == (actionf_p1) P_MobjThinker;
}

unsigned long long doomSimStateHash(void)
{
    thinker_t* thinker;
    player_t* player = &players[0];
    int count = 0;

    simHash = 1469598103934665603ULL;

    // prndindex, not rndindex: P_Random is the simulation's sequence and
    // M_Random is not. Hashing the wrong one would watch the menu instead of
    // the game. Both go in - rndindex costs nothing - but prndindex is the one
    // that means anything here.
    simMix(&prndindex, sizeof(prndindex));
    simMix(&rndindex, sizeof(rndindex));
    simMix(&leveltime, sizeof(leveltime));

    simMix(&player->health, sizeof(player->health));
    simMix(&player->armorpoints, sizeof(player->armorpoints));
    simMix(&player->readyweapon, sizeof(player->readyweapon));
    simMix(player->ammo, sizeof(player->ammo));

    if (player->mo)
    {
        simMix(&player->mo->x, sizeof(fixed_t));
        simMix(&player->mo->y, sizeof(fixed_t));
        simMix(&player->mo->z, sizeof(fixed_t));
        simMix(&player->mo->angle, sizeof(angle_t));
        simMix(&player->mo->momx, sizeof(fixed_t));
        simMix(&player->mo->momy, sizeof(fixed_t));
        simMix(&player->mo->momz, sizeof(fixed_t));
    }

    for (thinker = thinkercap.next; thinker && thinker != &thinkercap;
         thinker = thinker->next)
    {
        mobj_t* mobj = (mobj_t*) thinker;
        int frame;

        if (!simIsMobj(thinker))
            continue;

        frame = (int) (mobj->state - states);

        simMix(&mobj->x, sizeof(fixed_t));
        simMix(&mobj->y, sizeof(fixed_t));
        simMix(&mobj->z, sizeof(fixed_t));
        simMix(&mobj->angle, sizeof(angle_t));
        simMix(&mobj->health, sizeof(int));
        simMix(&mobj->type, sizeof(int));
        simMix(&frame, sizeof(frame));
        ++count;
    }

    simMix(&count, sizeof(count));
    return simHash;
}

// The simulation's random index, which is P_Random's.
int doomSimRndIndex(void) { return prndindex; }

int doomSimLevelTime(void) { return leveltime; }

int doomSimPlayerHealth(void) { return players[0].health; }

int doomSimPlayerX(void)
{
    return players[0].mo ? players[0].mo->x >> FRACBITS : 0;
}

int doomSimPlayerY(void)
{
    return players[0].mo ? players[0].mo->y >> FRACBITS : 0;
}

int doomSimPlayerAngleDegrees(void)
{
    if (!players[0].mo)
        return 0;

    // angle_t spans a circle in 2^32 units.
    return (int) (players[0].mo->angle / (ANG45 / 45));
}

int doomSimMobjCount(void)
{
    thinker_t* thinker;
    int count = 0;

    for (thinker = thinkercap.next; thinker && thinker != &thinkercap;
         thinker = thinker->next)
        if (simIsMobj(thinker))
            ++count;

    return count;
}
