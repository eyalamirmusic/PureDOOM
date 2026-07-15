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
#include <DOOM/v_video.h>
#include <DOOM/w_wad.h>
#include <DOOM/z_zone.h>

#include <DOOM/Sim/Level.h>
#include <DOOM/r_state.h>

#include <setjmp.h>
#include <stdio.h>

#include <vector>

// The live palette. i_video.c defines it and no header declares it; DOOM.c and
// the eacp port both reach it exactly this way, so this is the house style
// rather than a workaround.
extern unsigned char screen_palette[256 * 3];

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
static void simOnPrint(const char* text)
{
    (void) text;
}

// The engine does not copy its argv - doom_init keeps the pointer, and
// M_CheckParm walks it for the rest of the run. P_SpawnSpecials asks for "-avg"
// on every level load, which is long after the function that booted the engine
// has returned, so the array has to outlive it. Static, therefore, and not on
// doomSimBoot's stack.
//
// It went unnoticed until this file passed a second argument: with argc at 1,
// M_CheckParm's loop never dereferenced myargv at all, and the dangling pointer
// was harmless.
static char simProgram[] = "doom-tests";
static char simConfigFlag[] = "-config";
static char simConfigFile[] = PUREDOOM_TESTS_DIR "/doom-tests.cfg";
static char* simArgv[] = {simProgram, simConfigFlag, simConfigFile};

int doomSimBoot(const char* demoLump)
{
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

    doom_init((int) (sizeof(simArgv) / sizeof(simArgv[0])), simArgv, 0);

    // doom_init ends in D_StartTitle, which raises the attract loop's flag. Left
    // alone, D_DoAdvanceDemo runs on the very first tic, clears gameaction and
    // starts the title sequence - which then plays demo1, the credits, demo2 and
    // so on, whatever we asked for. Lower the flag and the game runs only the
    // demo we hand it.
    advancedemo = false;

    // Deliberately NOT -playdemo. That sets `singledemo`, which ends the demo
    // through I_Quit - and I_Quit calls M_SaveDefaults, which would have every
    // test run scribble on the config. Deferring the demo by hand lets the
    // engine retire it the ordinary way, by clearing demoplayback, and touches
    // nothing outside the process.
    if (demoLump)
        G_DeferedPlayDemo((char*) demoLump);

    simBooted = 1;
    return 1;
}

int doomSimInLevel(void)
{
    return demoplayback && gamestate == GS_LEVEL;
}

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

void doomSimReplayDemo(const char* demoLump)
{
    // The previous demo has ended (G_CheckDemoStatus cleared demoplayback and
    // advanced the attract loop), so this is a fresh start. Lower advancedemo
    // again for the same reason doomSimBoot does, and forget that the last demo
    // ran so doomSimRunTic's "finished" test starts over.
    simDemoStarted = 0;
    advancedemo = false;
    G_DeferedPlayDemo((char*) demoLump);
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

unsigned long long doomSimFrameHash(void)
{
    simHash = 1469598103934665603ULL;

    // screens[0] is the frame the engine has just finished drawing, one palette
    // index per pixel. The palette goes in beside it because it is live: a
    // damage flash or an invulnerability sphere changes what those same indices
    // resolve to without changing a single one of them.
    simMix(screens[0], SCREENWIDTH * SCREENHEIGHT);
    simMix(screen_palette, 256 * 3);

    return simHash;
}

int doomSimLumpCount(void)
{
    return numlumps;
}

void doomSimLumpName(int lump, char* nameOut)
{
    int i;

    // A lump name is eight bytes and is only null-terminated if it is shorter.
    for (i = 0; i < 8; ++i)
        nameOut[i] = lumpinfo[lump].name[i];

    nameOut[8] = '\0';
}

int doomSimLumpSize(int lump)
{
    return W_LumpLength(lump);
}

unsigned long long doomSimLumpHash(int lump)
{
    int size = W_LumpLength(lump);

    simHash = 1469598103934665603ULL;

    // The section markers (S_START, F_END, ...) are real lumps with no bytes in
    // them, and asking the zone for a zero-byte block to read nothing into is
    // not worth finding out about.
    if (size > 0)
        simMix(W_CacheLumpNum(lump, PU_CACHE), size);

    return simHash;
}

// The simulation's random index, which is P_Random's.
int doomSimRndIndex(void)
{
    return prndindex;
}

int doomSimLevelTime(void)
{
    return leveltime;
}

int doomSimPlayerHealth(void)
{
    return players[0].health;
}

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

int doomSimGeometryViewsConsistent(void)
{
    const auto& lvl = Doom::level();

    auto view = [](const void* ptr, int num, const auto& vec)
    { return ptr == vec.data() && num == (int) vec.size(); };

    return view(vertexes, numvertexes, lvl.vertexes) && view(segs, numsegs, lvl.segs)
           && view(subsectors, numsubsectors, lvl.subsectors)
           && view(sectors, numsectors, lvl.sectors)
           && view(nodes, numnodes, lvl.nodes) && view(lines, numlines, lvl.lines)
           && view(sides, numsides, lvl.sides) && blocklinks == lvl.blockLinks.data()
           && bmaporgx == lvl.blockmap.origin.x.raw
           && bmaporgy == lvl.blockmap.origin.y.raw
           && bmapwidth == lvl.blockmap.width && bmapheight == lvl.blockmap.height
           && blockmap == lvl.blockmap.offsets && blockmaplump == lvl.blockmap.lump;
}

// --- The scenario harness (Step 6) ------------------------------------------
//
// Handles are indices into this vector. A level load frees every PU_LEVEL mobj,
// so the handles it hands out are only valid until the next load - which clears
// the registry and re-registers the fresh player as handle 0.
static std::vector<mobj_t*> simMobjs;

static mobj_t* simMobj(int handle)
{
    if (handle < 0 || handle >= (int) simMobjs.size())
        return 0;

    return simMobjs[(unsigned) handle];
}

int doomSimLoadLevel(int episode, int map, int skill)
{
    if (setjmp(simAbort))
        return 0;

    // A demo playback would have set these from the .lmp header; a direct load
    // has no header, so establish single-player ourselves. Without playeringame[0]
    // the map's player-1 start spawns no mobj and there is nothing to move.
    consoleplayer = displayplayer = 0;
    deathmatch = false;
    netgame = false;
    playeringame[0] = true;

    // The old level's mobjs are about to be freed by Z_FreeTags in P_SetupLevel,
    // so every handle into them dies here.
    simMobjs.clear();

    // G_InitNew runs the whole load synchronously (G_DoLoadLevel -> P_SetupLevel),
    // unlike G_DeferedInitNew which only queues it for the next tic.
    G_InitNew((skill_t) skill, episode, map);

    // Handle 0 is always the player, so a scenario can move it without spawning
    // anything. It is null only if the map had no player-1 start.
    simMobjs.push_back(players[0].mo);

    return players[0].mo != 0;
}

int doomSimPlayerHandle(void)
{
    return (!simMobjs.empty() && simMobjs[0]) ? 0 : -1;
}

int doomSimSpawnMobj(int type, int x, int y, int z)
{
    if (setjmp(simAbort))
        return -1;

    mobj_t* mobj = P_SpawnMobj(x, y, z, (mobjtype_t) type);

    if (!mobj)
        return -1;

    simMobjs.push_back(mobj);
    return (int) simMobjs.size() - 1;
}

int doomSimCheckPosition(int handle, int x, int y)
{
    mobj_t* mobj = simMobj(handle);

    if (!mobj)
        return 0;

    if (setjmp(simAbort))
        return 0;

    return P_CheckPosition(mobj, x, y) ? 1 : 0;
}

int doomSimTryMove(int handle, int x, int y)
{
    mobj_t* mobj = simMobj(handle);

    if (!mobj)
        return 0;

    if (setjmp(simAbort))
        return 0;

    return P_TryMove(mobj, x, y) ? 1 : 0;
}

int doomSimMobjX(int handle)
{
    mobj_t* mobj = simMobj(handle);
    return mobj ? mobj->x : 0;
}

int doomSimMobjY(int handle)
{
    mobj_t* mobj = simMobj(handle);
    return mobj ? mobj->y : 0;
}

int doomSimMobjZ(int handle)
{
    mobj_t* mobj = simMobj(handle);
    return mobj ? mobj->z : 0;
}

int doomSimMobjFlags(int handle)
{
    mobj_t* mobj = simMobj(handle);
    return mobj ? (int) mobj->flags : 0;
}

void doomSimSetMobjFlags(int handle, int flags)
{
    mobj_t* mobj = simMobj(handle);

    if (mobj)
        mobj->flags = flags;
}

// P_BlockThingsIterator takes a bare function pointer, so the count rides a file
// static rather than a lambda capture. One process per test (NanoTest), and the
// probe is single-threaded, so the static is safe.
static int simBlockThingCount;

static doom_boolean simCountThing(mobj_t*)
{
    ++simBlockThingCount;
    return (doom_boolean) 1;
}

int doomSimThingsInBlockOf(int handle)
{
    mobj_t* mobj = simMobj(handle);

    if (!mobj)
        return -1;

    if (setjmp(simAbort))
        return -1;

    int blockx = (mobj->x - bmaporgx) >> MAPBLOCKSHIFT;
    int blocky = (mobj->y - bmaporgy) >> MAPBLOCKSHIFT;

    simBlockThingCount = 0;
    P_BlockThingsIterator(blockx, blocky, simCountThing);
    return simBlockThingCount;
}

void doomSimUnsetThingPosition(int handle)
{
    mobj_t* mobj = simMobj(handle);

    if (mobj)
        P_UnsetThingPosition(mobj);
}

void doomSimSetThingPosition(int handle)
{
    mobj_t* mobj = simMobj(handle);

    if (mobj)
        P_SetThingPosition(mobj);
}

int doomSimTypeBarrel(void)
{
    return MT_BARREL;
}

int doomSimOnFloorZ(void)
{
    return ONFLOORZ;
}

int doomSimFlagNoClip(void)
{
    return MF_NOCLIP;
}
