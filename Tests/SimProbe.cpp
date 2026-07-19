#include "SimProbe.h"
#include <DOOM/Render/Video.h>

#include <DOOM/Game/AttractMode.h>
#include <DOOM/Game/DemoState.h>
#include <DOOM/Game/GameFlow.h>
#include <DOOM/Game/GameSession.h>
#include <DOOM/Game/LevelStats.h>
#include <DOOM/Game/OverlayState.h>
#include <DOOM/Game/PlayerState.h>
#include <DOOM/Game/SaveGameState.h>
#include <DOOM/Game/GameVersion.h>
#include <DOOM/UI/Finale.h>
#include <DOOM/UI/FinaleState.h>
#include <DOOM/Sim/Level.h>
#include <DOOM/Sim/Random.h>
#include <DOOM/Sim/SaveGame.h>
#include <DOOM/Sim/ThinkerList.h>
#include <DOOM/Sim/Setup.h>
#include <DOOM/DOOM.h>

#include <DOOM/Game/MapSpawns.h>
#include <DOOM/Game/DoomMain.h>
#include <DOOM/Game/Config.h>
#include <DOOM/Sim/Info.h>
#include <DOOM/Sim/Random.h>
#include <DOOM/Sim/SimDefs.h>
#include <DOOM/Sim/MobjTypes.h>
#include <DOOM/Math/TrigTables.h>
#include <DOOM/Wad/WadFile.h>

#include <DOOM/Sim/Level.h>

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h> // malloc / free, for the counting allocator

#include <vector>

// The live palette. i_video.c defines it and no header declares it; DOOM.c and
#include <DOOM/Game/Game.h>
// the eacp port both reach it exactly this way, so this is the house style
#include <DOOM/Sim/Mobj.h>
// rather than a workaround.
#include <DOOM/Sim/MapUtil.h>
#include <DOOM/Sim/Movement.h>
extern unsigned char screen_palette[256 * 3];

// Doom::fatalError reports through doom_exit, which by default takes the process with
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
// Doom::checkParm walks it for the rest of the run. Doom::spawnSpecials asks for "-avg"
// on every level load, which is long after the function that booted the engine
// has returned, so the array has to outlive it. Static, therefore, and not on
// doomSimBoot's stack.
//
// It went unnoticed until this file passed a second argument: with argc at 1,
// Doom::checkParm's loop never dereferenced myargv at all, and the dangling pointer
// was harmless.
static char simProgram[] = "doom-tests";
static char simConfigFlag[] = "-config";
static char simConfigFile[] = PUREDOOM_TESTS_DIR "/doom-tests.cfg";
static char* simArgv[] = {simProgram, simConfigFlag, simConfigFile};

// The two boots below share everything but what they do with the attract loop.
// A demo boot lowers advancedemo so the game runs only the demo we hand it; a
// title boot leaves it up so the first tic brings up the title screen for a menu
// script to run over.
static int simBootInternal(const char* demoLump, int keepAttract)
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

    // doom_init ends in Doom::startTitle, which raises the attract loop's flag. Left
    // alone, Doom::doAdvanceDemo runs on the very first tic, clears gameaction and
    // starts the title sequence - which then plays demo1, the credits, demo2 and
    // so on. A demo boot lowers the flag so the game runs only the demo we hand
    // it; a title boot (keepAttract) leaves it up, so the first tic brings up
    // TITLEPIC and the attract loop then holds there for ~170 tics.
    if (!keepAttract)
        Doom::attractMode().advancedemo = false;

    // Deliberately NOT -playdemo. That sets `singledemo`, which ends the demo
    // through Doom::quitGame - and Doom::quitGame calls Doom::saveDefaults, which would have every
    // test run scribble on the config. Deferring the demo by hand lets the
    // engine retire it the ordinary way, by clearing demoplayback, and touches
    // nothing outside the process.
    if (demoLump)
        Doom::deferPlayDemo((char*) demoLump);

    simBooted = 1;
    return 1;
}

int doomSimBoot(const char* demoLump)
{
    return simBootInternal(demoLump, 0);
}

int doomSimBootToTitle()
{
    return simBootInternal(0, 1);
}

int doomSimInLevel()
{
    return Doom::demoState().demoplayback
           && Doom::gameFlow().gamestate == Doom::GS_LEVEL;
}

// The demo is deferred, so it is not playing on the tic that queues it. "Not
// playing" therefore only means "finished" once it has actually started.
static int simDemoStarted;

int doomSimRunTic()
{
    if (setjmp(simAbort))
        return 0;

    doom_force_update();

    if (Doom::demoState().demoplayback)
    {
        simDemoStarted = 1;
        return 1;
    }

    return !simDemoStarted;
}

void doomSimReplayDemo(const char* demoLump)
{
    // The previous demo has ended (Doom::checkDemoStatus cleared demoplayback and
    // advanced the attract loop), so this is a fresh start. Lower advancedemo
    // again for the same reason doomSimBoot does, and forget that the last demo
    // ran so doomSimRunTic's "finished" test starts over.
    simDemoStarted = 0;
    Doom::attractMode().advancedemo = false;
    Doom::deferPlayDemo((char*) demoLump);
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

static int simIsMobj(Doom::Thinker* thinker)
{
    // How the probe finds mobjs, not what it mixes: the thinker's virtual kind()
    // replaces the old function-pointer identity test (Doom::Thinker -> Thinker).
    return thinker->kind() == Doom::ThinkerKind::Mobj && !thinker->removed;
}

unsigned long long doomSimStateHash()
{
    auto& rnd = Doom::randomness();
    auto& thinkers = Doom::thinkerList();

    Doom::Thinker* thinker;
    Doom::Player* player = &Doom::playerState().players[0];
    int count = 0;

    simHash = 1469598103934665603ULL;

    // playIndex, not menuIndex: P_Random is the simulation's sequence and
    // M_Random is not. Hashing the wrong one would watch the menu instead of
    // the game. Both go in - menuIndex costs nothing - but playIndex is the one
    // that means anything here.
    simMix(&rnd.playIndex, sizeof(rnd.playIndex));
    simMix(&rnd.menuIndex, sizeof(rnd.menuIndex));
    simMix(&Doom::levelStats().leveltime, sizeof(Doom::levelStats().leveltime));

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

    for (thinker = thinkers.cap.next; thinker && thinker != &thinkers.cap;
         thinker = thinker->next)
    {
        Doom::Mobj* mobj = (Doom::Mobj*) thinker;
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

unsigned long long doomSimFrameHash()
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

// Allocation accounting. Deliberately counts blocks rather than bytes: the point
// is whether an owner released what it took, and a block is the unit an owner
// deals in. Installed through the public doom_set_malloc, so it sees exactly what
// the engine sees and nothing else - the counter can only go negative if the
// engine frees a block it did not allocate through the host, which is itself worth
// failing on.
static long simLiveBlocks = 0;

static void* simCountingMalloc(int size)
{
    void* block = malloc((size_t) size);
    if (block)
        ++simLiveBlocks;
    return block;
}

static void simCountingFree(void* ptr)
{
    if (ptr)
        --simLiveBlocks;
    free(ptr);
}

void doomSimCountAllocations()
{
    doom_set_malloc(simCountingMalloc, simCountingFree);
}

int doomSimLiveAllocations()
{
    return (int) simLiveBlocks;
}

int doomSimLumpCount()
{
    return Doom::wad().count();
}

void doomSimLumpName(int lump, char* nameOut)
{
    int i;

    // A lump name is eight bytes and is only null-terminated if it is shorter.
    for (i = 0; i < 8; ++i)
        nameOut[i] = Doom::wad().info(lump).name[i];

    nameOut[8] = '\0';
}

int doomSimLumpSize(int lump)
{
    return Doom::wad().length(lump);
}

unsigned long long doomSimLumpHash(int lump)
{
    int size = Doom::wad().length(lump);

    simHash = 1469598103934665603ULL;

    // The section markers (S_START, F_END, ...) are real lumps with no bytes in
    // them, and asking the zone for a zero-byte block to read nothing into is
    // not worth finding out about.
    if (size > 0)
        simMix(Doom::cacheLumpNum(lump), size);

    return simHash;
}

// The simulation's random index, which is P_Random's.
int doomSimRndIndex()
{
    return Doom::randomness().playIndex;
}

int doomSimLevelTime()
{
    return Doom::levelStats().leveltime;
}

int doomSimPlayerHealth()
{
    return Doom::playerState().players[0].health;
}

int doomSimPlayerX()
{
    auto& players_ = Doom::playerState();

    return players_.players[0].mo ? players_.players[0].mo->x.toInt() : 0;
}

int doomSimPlayerY()
{
    auto& players_ = Doom::playerState();

    return players_.players[0].mo ? players_.players[0].mo->y.toInt() : 0;
}

int doomSimPlayerAngleDegrees()
{
    auto& players_ = Doom::playerState();

    if (!players_.players[0].mo)
        return 0;

    // angle_t spans a circle in 2^32 units.
    return (int) (players_.players[0].mo->angle.raw / (ANG45.raw / 45));
}

int doomSimMobjCount()
{
    auto& thinkers = Doom::thinkerList();

    Doom::Thinker* thinker;
    int count = 0;

    for (thinker = thinkers.cap.next; thinker && thinker != &thinkers.cap;
         thinker = thinker->next)
        if (simIsMobj(thinker))
            ++count;

    return count;
}

int doomSimGeometryViewsConsistent()
{
    const auto& lvl = Doom::level();

    auto view = [](const void* ptr, int num, const auto& vec)
    { return ptr == vec.data() && num == (int) vec.size(); };

    return view(vertexes, numvertexes, lvl.vertexes) && view(segs, numsegs, lvl.segs)
           && view(subsectors, numsubsectors, lvl.subsectors)
           && view(sectors, numsectors, lvl.sectors)
           && view(nodes, numnodes, lvl.nodes) && view(lines, numlines, lvl.lines)
           && view(sides, numsides, lvl.sides) && blocklinks == lvl.blockLinks.data()
           && bmaporgx == lvl.blockmap.origin.x && bmaporgy == lvl.blockmap.origin.y
           && bmapwidth == lvl.blockmap.width && bmapheight == lvl.blockmap.height
           && blockmap == lvl.blockmap.offsets && blockmaplump == lvl.blockmap.lump;
}

// --- The scenario harness (Step 6) ------------------------------------------
//
// Handles are indices into this vector. A level load frees every PU_LEVEL mobj,
// so the handles it hands out are only valid until the next load - which clears
// the registry and re-registers the fresh player as handle 0.
static std::vector<Doom::Mobj*> simMobjs;

static Doom::Mobj* simMobj(int handle)
{
    if (handle < 0 || handle >= (int) simMobjs.size())
        return 0;

    return simMobjs[(unsigned) handle];
}

int doomSimLoadLevel(int episode, int map, int skill)
{
    auto& players_ = Doom::playerState();
    auto& session = Doom::gameSession();

    if (setjmp(simAbort))
        return 0;

    // A demo playback would have set these from the .lmp header; a direct load
    // has no header, so establish single-player ourselves. Without playeringame[0]
    // the map's player-1 start spawns no mobj and there is nothing to move.
    players_.consoleplayer = players_.displayplayer = 0;
    session.deathmatch = false;
    session.netgame = false;
    players_.playeringame[0] = true;

    // The old level's mobjs are about to be freed by Doom::setupLevel (the level
    // allocation pool is released whole), so every handle into them dies here.
    simMobjs.clear();

    // Doom::initNewGame runs the whole load synchronously (G_DoLoadLevel -> Doom::setupLevel),
    // unlike Doom::deferInitNew which only queues it for the next tic.
    Doom::initNewGame((Doom::Skill) skill, episode, map);

    // Handle 0 is always the player, so a scenario can move it without spawning
    // anything. It is null only if the map had no player-1 start.
    simMobjs.push_back(players_.players[0].mo);

    return players_.players[0].mo != 0;
}

int doomSimPlayerHandle()
{
    return (!simMobjs.empty() && simMobjs[0]) ? 0 : -1;
}

int doomSimSpawnMobj(int type, int x, int y, int z)
{
    if (setjmp(simAbort))
        return -1;

    Doom::Mobj* mobj = Doom::spawnMobj(
        fixed_t {x}, fixed_t {y}, fixed_t {z}, (Doom::MobjType) type);

    if (!mobj)
        return -1;

    simMobjs.push_back(mobj);
    return (int) simMobjs.size() - 1;
}

int doomSimCheckPosition(int handle, int x, int y)
{
    Doom::Mobj* mobj = simMobj(handle);

    if (!mobj)
        return 0;

    if (setjmp(simAbort))
        return 0;

    return Doom::checkPosition(mobj, fixed_t {x}, fixed_t {y}) ? 1 : 0;
}

int doomSimTryMove(int handle, int x, int y)
{
    Doom::Mobj* mobj = simMobj(handle);

    if (!mobj)
        return 0;

    if (setjmp(simAbort))
        return 0;

    return Doom::tryMove(mobj, fixed_t {x}, fixed_t {y}) ? 1 : 0;
}

int doomSimMobjX(int handle)
{
    Doom::Mobj* mobj = simMobj(handle);
    return mobj ? mobj->x.raw : 0;
}

int doomSimMobjY(int handle)
{
    Doom::Mobj* mobj = simMobj(handle);
    return mobj ? mobj->y.raw : 0;
}

int doomSimMobjZ(int handle)
{
    Doom::Mobj* mobj = simMobj(handle);
    return mobj ? mobj->z.raw : 0;
}

int doomSimMobjFlags(int handle)
{
    Doom::Mobj* mobj = simMobj(handle);
    return mobj ? (int) mobj->flags : 0;
}

void doomSimSetMobjFlags(int handle, int flags)
{
    Doom::Mobj* mobj = simMobj(handle);

    if (mobj)
        mobj->flags = flags;
}

// The counter rides a file static rather than a lambda capture, so the callback
// stays a plain function. One process per test (NanoTest), and the probe is
// single-threaded, so the static is safe.
static int simBlockThingCount;

static bool simCountThing(Doom::Mobj*)
{
    ++simBlockThingCount;
    return true;
}

int doomSimThingsInBlockOf(int handle)
{
    Doom::Mobj* mobj = simMobj(handle);

    if (!mobj)
        return -1;

    if (setjmp(simAbort))
        return -1;

    int blockx = (mobj->x - bmaporgx).raw >> Doom::MAPBLOCKSHIFT;
    int blocky = (mobj->y - bmaporgy).raw >> Doom::MAPBLOCKSHIFT;

    simBlockThingCount = 0;
    Doom::forEachThingInBlock(blockx, blocky, simCountThing);
    return simBlockThingCount;
}

void doomSimUnsetThingPosition(int handle)
{
    Doom::Mobj* mobj = simMobj(handle);

    if (mobj)
        Doom::unsetThingPosition(*mobj);
}

void doomSimSetThingPosition(int handle)
{
    Doom::Mobj* mobj = simMobj(handle);

    if (mobj)
        Doom::setThingPosition(*mobj);
}

int doomSimTypeBarrel()
{
    return Doom::MT_BARREL;
}

int doomSimOnFloorZ()
{
    return Doom::ONFLOORZ.raw;
}

int doomSimFlagNoClip()
{
    return Doom::MF_NOCLIP;
}

// --- The save/load serialization net (pre-Thinker) --------------------------
//
// p_saveg's archive/unarchive is the one simulation path the goldens do not
// watch: no demo saves or loads, so Doom::archiveThinkers / Doom::unArchiveThinkers and
// the whole mobj/special byte layout ride unpinned. The Doom::Thinker -> Thinker
// virtualisation and the mobj/special zone-ownership change both rewrite exactly
// that layout, so this builds the missing net first (the Step-0 move): archive
// the live world, reload a fresh base level and unarchive over it - precisely
// what gDoLoadGame does - and assert the world hash is unchanged.
//
// This uses its own hash, not doomSimStateHash: that one is golden-compared and
// append-only, and it covers only the mobjs and the player, not the sectors,
// lines and sides Doom::archiveWorld round-trips. This one walks everything the
// archive serializes - and only the scalar fields it restores exactly, never the
// pointers (target, subsector, the blockmap/sector links) that Doom::setThingPosition
// legitimately recomputes on load and would differ between two world instances.
// The random indices and leveltime ride outside the archive, so they are left
// out of the hash rather than restored.
static unsigned long long simWorldHash()
{
    auto& players_ = Doom::playerState();

    auto& thinkers = Doom::thinkerList();

    simHash = 1469598103934665603ULL;

    // Players - the scalar state Doom::archivePlayers/Doom::unArchivePlayers round-trip
    // (pointers like mo/attacker/message are fixed up or nulled on load).
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (!players_.playeringame[i])
            continue;
        Doom::Player* p = &players_.players[i];
        simMix(&p->health, sizeof(p->health));
        simMix(&p->armorpoints, sizeof(p->armorpoints));
        simMix(&p->armortype, sizeof(p->armortype));
        simMix(&p->readyweapon, sizeof(p->readyweapon));
        simMix(&p->pendingweapon, sizeof(p->pendingweapon));
        simMix(p->ammo, sizeof(p->ammo));
        simMix(p->maxammo, sizeof(p->maxammo));
        simMix(p->weaponowned, sizeof(p->weaponowned));
        simMix(p->powers, sizeof(p->powers));
        simMix(p->cards, sizeof(p->cards));
        simMix(&p->killcount, sizeof(p->killcount));
        simMix(&p->itemcount, sizeof(p->itemcount));
        simMix(&p->secretcount, sizeof(p->secretcount));
    }

    // The world - sectors, lines and sides, exactly the fields Doom::archiveWorld
    // walks (moving floors/ceilings and switched textures live here).
    for (int i = 0; i < numsectors; i++)
    {
        Doom::Sector* s = &sectors[i];
        simMix(&s->floorheight, sizeof(s->floorheight));
        simMix(&s->ceilingheight, sizeof(s->ceilingheight));
        simMix(&s->floorpic, sizeof(s->floorpic));
        simMix(&s->ceilingpic, sizeof(s->ceilingpic));
        simMix(&s->lightlevel, sizeof(s->lightlevel));
        simMix(&s->special, sizeof(s->special));
        simMix(&s->tag, sizeof(s->tag));
    }
    for (int i = 0; i < numlines; i++)
    {
        Doom::Line* l = &lines[i];
        simMix(&l->flags, sizeof(l->flags));
        simMix(&l->special, sizeof(l->special));
        simMix(&l->tag, sizeof(l->tag));
    }
    for (int i = 0; i < numsides; i++)
    {
        Doom::Side* sd = &sides[i];
        simMix(&sd->textureoffset, sizeof(sd->textureoffset));
        simMix(&sd->rowoffset, sizeof(sd->rowoffset));
        simMix(&sd->toptexture, sizeof(sd->toptexture));
        simMix(&sd->bottomtexture, sizeof(sd->bottomtexture));
        simMix(&sd->midtexture, sizeof(sd->midtexture));
    }

    // Every thinker: each mobj by the scalar fields Doom::archiveThinkers restores,
    // plus a total thinker count so a wrong number of specials from
    // Doom::unArchiveSpecials shows up even though the specials are hashed only
    // through the sector state they drive.
    int mobjCount = 0;
    int thinkerCount = 0;
    for (Doom::Thinker* th = thinkers.cap.next; th && th != &thinkers.cap;
         th = th->next)
    {
        ++thinkerCount;
        if (!simIsMobj(th))
            continue;
        Doom::Mobj* m = (Doom::Mobj*) th;
        int frame = (int) (m->state - states);
        simMix(&m->x, sizeof(fixed_t));
        simMix(&m->y, sizeof(fixed_t));
        simMix(&m->z, sizeof(fixed_t));
        simMix(&m->angle, sizeof(angle_t));
        simMix(&m->momx, sizeof(fixed_t));
        simMix(&m->momy, sizeof(fixed_t));
        simMix(&m->momz, sizeof(fixed_t));
        simMix(&m->health, sizeof(int));
        simMix(&m->type, sizeof(int));
        simMix(&frame, sizeof(frame));
        simMix(&m->flags, sizeof(m->flags));
        simMix(&m->tics, sizeof(m->tics));
        simMix(&m->movedir, sizeof(m->movedir));
        simMix(&m->movecount, sizeof(m->movecount));
        simMix(&m->reactiontime, sizeof(m->reactiontime));
        simMix(&m->threshold, sizeof(m->threshold));
        ++mobjCount;
    }
    simMix(&mobjCount, sizeof(mobjCount));
    simMix(&thinkerCount, sizeof(thinkerCount));
    return simHash;
}

int doomSimSaveLoadPreservesWorld()
{
    auto& session = Doom::gameSession();

    auto& save = Doom::saveGameState();

    static byte saveScratch[0x2c000]; // SAVEGAMESIZE

    if (setjmp(simAbort))
        return 0;

    int ep = session.gameepisode;
    int map = session.gamemap;
    int skill = (int) session.gameskill;

    unsigned long long before = simWorldHash();

    // Archive the live world, exactly the P_Archive* sequence doSaveGame runs.
    save.cursor = saveScratch;
    Doom::archivePlayers();
    Doom::archiveWorld();
    Doom::archiveThinkers();
    Doom::archiveSpecials();

    // Reload a fresh base level (gDoLoadGame's gInitNew), which wipes the world
    // the unarchive then rebuilds. doomSimLoadLevel re-arms simAbort on the way
    // through and returns with it pointing at a dead frame, so re-arm it here.
    if (!doomSimLoadLevel(ep, map, skill))
        return 0;
    if (setjmp(simAbort))
        return 0;

    // Unarchive over the fresh world.
    save.cursor = saveScratch;
    Doom::unArchivePlayers();
    Doom::unArchiveWorld();
    Doom::unArchiveThinkers();
    Doom::unArchiveSpecials();

    // The fresh player doomSimLoadLevel registered as handle 0 was just freed and
    // rebuilt by the unarchive; point the registry at the restored one.
    simMobjs.clear();
    simMobjs.push_back(Doom::playerState().players[0].mo);

    unsigned long long after = simWorldHash();
    return before == after ? 1 : 0;
}

// --- The menu/UI harness (Step 8) -------------------------------------------
//
// The game-flow and overlay state the harness reads (gamestate, menuactive,
// is_wiping_screen) come off the Engine through their cluster accessors.

void doomSimPostKeyDown(int key)
{
    doom_key_down((doom_key_t) key);
}

void doomSimPostKeyUp(int key)
{
    doom_key_up((doom_key_t) key);
}

int doomSimStepTic()
{
    if (setjmp(simAbort))
        return 0;

    doom_force_update();
    return 1;
}

int doomSimIsWiping()
{
    return Doom::gameFlow().is_wiping_screen ? 1 : 0;
}

int doomSimGameState()
{
    return (int) Doom::gameFlow().gamestate;
}

int doomSimMenuActive()
{
    return Doom::overlayState().menuactive ? 1 : 0;
}

// --- The automap harness -----------------------------------------------------

int doomSimAutomapActive()
{
    return Doom::overlayState().automapactive ? 1 : 0;
}

// --- The finale harness ------------------------------------------------------

void doomSimStartFinale()
{
    if (setjmp(simAbort))
        return;

    Doom::startFinale();
}

int doomSimFinaleStage()
{
    return Doom::finaleState().finalestage;
}

int doomSimGameMode()
{
    return (int) Doom::gameVersion().gamemode;
}

DoomSimFileRead doomSimReadFileIntoOwner(const char* path)
{
    DoomSimFileRead result = {};

    // readFile calls fatalError rather than returning a failure, so a bad path
    // arrives here as a longjmp and the caller sees the zeroed result.
    if (setjmp(simAbort))
        return result;

    EA::Vector<byte> owner;
    result.length = Doom::readFile(path, owner);
    result.ownerSize = owner.size();
    result.magicIsIwad = owner.size() >= 4 && owner[0] == 'I' && owner[1] == 'W'
                         && owner[2] == 'A' && owner[3] == 'D';
    return result;
}
