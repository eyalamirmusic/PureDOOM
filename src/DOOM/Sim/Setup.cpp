// Rewritten out of vanilla p_setup into namespace Doom.
//
// Level loading: parse each map lump into Doom::Level's vectors and refresh the
// geometry view-globals the renderer and playsim index, build the blockmap
// descriptor and per-sector line lists, spawn the things, and set up a fresh level.
// p_setup.cpp shims Doom::setupLevel and Doom::init and owns the view-global storage; the
// per-lump loaders are file-local. Golden-neutral - every demo loads its level
// through this, and LevelTests pins the view invariant.

#include "../Host/Platform.h"
#include "MobjTypes.h"
#include "MapTypes.h"
#include "../Render/RenderTypes.h"

#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h"
#include "../Math/Swap.h"
#include "SimDefs.h"
#include "../Wad/WadFile.h"

#include "../Game/CorpseQueue.h"
#include "../Game/EngineParams.h"
#include "../Game/GameSession.h"
#include "../Game/GameVersion.h"
#include "../Game/IntermissionInfo.h"
#include "../Game/LevelStats.h"
#include "../Game/MapSpawns.h"
#include "../Game/PlayerState.h"
#include "Level.h"
#include "Setup.h"
#include "Tick.h" // levelAlloc / levelFree / freeLevelAllocations
#include "../Render/Data.h"

#include "../Render/Things.h"
#include "Specials.h"
#include "../Containers.h"

// Doom::spawnMapThing is Doom::Mobj's now (global shim); the things loader calls it.
#include "../Game/Game.h"
#include "../Game/Sound.h"
#include "../Host/System.h"
#include "Mobj.h"
#include "Switches.h"
#include "../Math/BBox.h"
#include "ItemRespawnQueue.h"
void Doom::spawnMapThing(MapThing& mthing);

// The thinker functions stay global (p_saveg identity); declared so the spawners
// can store their address.

namespace Doom
{
// Forward declarations so the file's own call order needs no rearranging.
void loadVertexes(int lump);
void loadSegs(int lump);
void loadSubsectors(int lump);
void loadSectors(int lump);
void loadNodes(int lump);
void loadThings(int lump);
void loadLineDefs(int lump);
void loadSideDefs(int lump);
void loadBlockMap(int lump);
void groupLines();
void setupLevel(int episode, int map, int playermask, Skill skill);
void init();

void loadVertexes(int lump)
{
    auto& lvl = level();

    // Determine number of lumps:
    //  total lump length / vertex record length.
    const int count = wad().length(lump) / sizeof(MapVertex);

    // Owned by the Level (Sim/Level.h). assign, not resize, so a shorter second
    // level does not inherit the first level's tail - Z_Malloc handed back fresh
    // zeroed memory every load.
    lvl.vertexes.assign(count, Vertex {});

    // Load data into cache.
    byte* data = static_cast<byte*>(cacheLumpNum(lump));

    MapVertex* ml = reinterpret_cast<MapVertex*>(data);
    Vertex* li = lvl.vertexes.data();

    // Copy and convert vertex coordinates,
    // internal representation as fixed.
    for (int i = 0; i < count; i++, li++, ml++)
    {
        li->x = Fixed::fromInt(littleEndian(ml->x));
        li->y = Fixed::fromInt(littleEndian(ml->y));
    }

    // Free buffer memory.
}

//
// loadSegs
//
void loadSegs(int lump)
{
    auto& lvl = level();

    const int count = wad().length(lump) / sizeof(MapSeg);
    lvl.segs.assign(count, Seg {});
    byte* data = static_cast<byte*>(cacheLumpNum(lump));

    MapSeg* ml = reinterpret_cast<MapSeg*>(data);
    Seg* li = lvl.segs.data();
    for (int i = 0; i < count; i++, li++, ml++)
    {
        li->v1 = &level().vertexes[littleEndian(ml->v1)];
        li->v2 = &level().vertexes[littleEndian(ml->v2)];

        li->angle = Angle {(unsigned) (littleEndian(ml->angle)) << 16};
        li->offset = Fixed::fromInt(littleEndian(ml->offset));
        int linedef = littleEndian(ml->linedef);
        Line* ldef = &level().lines[linedef];
        li->linedef = ldef;
        int side = littleEndian(ml->side);
        li->sidedef = &level().sides[ldef->sidenum[side]];
        li->frontsector = level().sides[ldef->sidenum[side]].sector;
        if (ldef->flags & ML_TWOSIDED)
            li->backsector = level().sides[ldef->sidenum[side ^ 1]].sector;
        else
            li->backsector = nullptr;
    }
}

//
// loadSubsectors
//
void loadSubsectors(int lump)
{
    auto& lvl = level();

    const int count = wad().length(lump) / sizeof(MapSubsector);
    lvl.subsectors.assign(count, SubSector {});
    byte* data = static_cast<byte*>(cacheLumpNum(lump));

    MapSubsector* ms = reinterpret_cast<MapSubsector*>(data);
    SubSector* ss = lvl.subsectors.data();

    for (int i = 0; i < count; i++, ss++, ms++)
    {
        ss->numlines = littleEndian(ms->numsegs);
        ss->firstline = littleEndian(ms->firstseg);
    }
}

//
// loadSectors
//
void loadSectors(int lump)
{
    auto& lvl = level();

    const int count = wad().length(lump) / sizeof(MapSector);
    lvl.sectors.assign(count, Sector {});
    byte* data = static_cast<byte*>(cacheLumpNum(lump));

    MapSector* ms = reinterpret_cast<MapSector*>(data);
    Sector* ss = lvl.sectors.data();
    for (int i = 0; i < count; i++, ss++, ms++)
    {
        ss->floorheight = Fixed::fromInt(littleEndian(ms->floorheight));
        ss->ceilingheight = Fixed::fromInt(littleEndian(ms->ceilingheight));
        ss->floorpic = flatNumForName(nameView(ms->floorpic, 8));
        ss->ceilingpic = flatNumForName(nameView(ms->ceilingpic, 8));
        ss->lightlevel = littleEndian(ms->lightlevel);
        ss->special = littleEndian(ms->special);
        ss->tag = littleEndian(ms->tag);
        ss->thinglist = nullptr;
    }
}

//
// loadNodes
//
void loadNodes(int lump)
{
    auto& lvl = level();

    const int count = wad().length(lump) / sizeof(MapNode);
    lvl.nodes.assign(count, Node {});
    byte* data = static_cast<byte*>(cacheLumpNum(lump));

    MapNode* mn = reinterpret_cast<MapNode*>(data);
    Node* no = lvl.nodes.data();

    for (int i = 0; i < count; i++, no++, mn++)
    {
        no->x = Fixed::fromInt(littleEndian(mn->x));
        no->y = Fixed::fromInt(littleEndian(mn->y));
        no->dx = Fixed::fromInt(littleEndian(mn->dx));
        no->dy = Fixed::fromInt(littleEndian(mn->dy));
        for (int j = 0; j < 2; j++)
        {
            no->children[j] = littleEndian(mn->children[j]);
            for (int k = 0; k < 4; k++)
                no->bbox[j][k] = Fixed::fromInt(littleEndian(mn->bbox[j][k]));
        }
    }
}

//
// loadThings
//
void loadThings(int lump)
{
    byte* data = static_cast<byte*>(cacheLumpNum(lump));
    int numthings = wad().length(lump) / sizeof(MapThing);

    MapThing* mt = reinterpret_cast<MapThing*>(data);
    for (int i = 0; i < numthings; i++, mt++)
    {
        bool spawn = true;

        // Do not spawn cool, new monsters if !commercial
        if (gameVersion().gamemode != GameMode::Commercial)
        {
            switch (mt->type)
            {
                case 68: // Arachnotron
                case 64: // Archvile
                case 88: // Boss Brain
                case 89: // Boss Shooter
                case 69: // Hell Knight
                case 67: // Mancubus
                case 71: // Pain Elemental
                case 65: // Former Human Commando
                case 66: // Revenant
                case 84: // Wolf SS
                    spawn = false;
                    break;
            }
        }
        if (spawn == false)
            break;

        // Do spawn all other stuff.
        mt->x = littleEndian(mt->x);
        mt->y = littleEndian(mt->y);
        mt->angle = littleEndian(mt->angle);
        mt->type = littleEndian(mt->type);
        mt->options = littleEndian(mt->options);

        spawnMapThing(*mt);
    }
}

//
// loadLineDefs
// Also counts secret lines for intermissions.
//
void loadLineDefs(int lump)
{
    auto& lvl = level();

    const int count = wad().length(lump) / sizeof(MapLinedef);
    lvl.lines.assign(count, Line {});
    byte* data = static_cast<byte*>(cacheLumpNum(lump));

    MapLinedef* mld = reinterpret_cast<MapLinedef*>(data);
    Line* ld = lvl.lines.data();
    for (int i = 0; i < count; i++, mld++, ld++)
    {
        ld->flags = littleEndian(mld->flags);
        ld->special = littleEndian(mld->special);
        ld->tag = littleEndian(mld->tag);
        Vertex* v1 = ld->v1 = &level().vertexes[littleEndian(mld->v1)];
        Vertex* v2 = ld->v2 = &level().vertexes[littleEndian(mld->v2)];
        ld->dx = v2->x - v1->x;
        ld->dy = v2->y - v1->y;

        if (!ld->dx)
            ld->slopetype = SlopeType::Vertical;
        else if (!ld->dy)
            ld->slopetype = SlopeType::Horizontal;
        else
        {
            if (FixedDiv(ld->dy, ld->dx).isPositive())
                ld->slopetype = SlopeType::Positive;
            else
                ld->slopetype = SlopeType::Negative;
        }

        if (v1->x < v2->x)
        {
            ld->bbox[boxLeft] = v1->x;
            ld->bbox[boxRight] = v2->x;
        }
        else
        {
            ld->bbox[boxLeft] = v2->x;
            ld->bbox[boxRight] = v1->x;
        }

        if (v1->y < v2->y)
        {
            ld->bbox[boxBottom] = v1->y;
            ld->bbox[boxTop] = v2->y;
        }
        else
        {
            ld->bbox[boxBottom] = v2->y;
            ld->bbox[boxTop] = v1->y;
        }

        ld->sidenum[0] = littleEndian(mld->sidenum[0]);
        ld->sidenum[1] = littleEndian(mld->sidenum[1]);

        if (ld->sidenum[0] != -1)
            ld->frontsector = level().sides[ld->sidenum[0]].sector;
        else
            ld->frontsector = nullptr;

        if (ld->sidenum[1] != -1)
            ld->backsector = level().sides[ld->sidenum[1]].sector;
        else
            ld->backsector = nullptr;
    }
}

//
// loadSideDefs
//
void loadSideDefs(int lump)
{
    auto& lvl = level();

    const int count = wad().length(lump) / sizeof(MapSidedef);
    lvl.sides.assign(count, Side {});
    byte* data = static_cast<byte*>(cacheLumpNum(lump));

    MapSidedef* msd = reinterpret_cast<MapSidedef*>(data);
    Side* sd = lvl.sides.data();
    for (int i = 0; i < count; i++, msd++, sd++)
    {
        sd->textureoffset = Fixed::fromInt(littleEndian(msd->textureoffset));
        sd->rowoffset = Fixed::fromInt(littleEndian(msd->rowoffset));
        sd->toptexture = textureNumForName(nameView(msd->toptexture, 8));
        sd->bottomtexture = textureNumForName(nameView(msd->bottomtexture, 8));
        sd->midtexture = textureNumForName(nameView(msd->midtexture, 8));
        sd->sector = &level().sectors[littleEndian(msd->sector)];
    }
}

//
// loadBlockMap
//
void loadBlockMap(int lump)
{
    short* lumpData = static_cast<short*>(cacheLumpNum(lump));
    int count = wad().length(lump) / 2;

    for (int i = 0; i < count; i++)
        lumpData[i] = littleEndian(lumpData[i]);

    // Fill the Level's blockmap descriptor from the lump header.
    Blockmap& bmap = level().blockmap;
    bmap.lump = lumpData;
    bmap.offsets = lumpData + 4;
    bmap.origin = {Fixed {lumpData[0] << fracBits}, Fixed {lumpData[1] << fracBits}};
    bmap.width = lumpData[2];
    bmap.height = lumpData[3];

    // clear out mobj chains. The array is the Level's; the mobjs it will point at
    // are the zone's.
    level().blockLinks.assign(bmap.width * bmap.height, nullptr);
}

//
// groupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
void groupLines()
{
    Array<Fixed, 4> bbox;

    // look up sector number for each subsector
    SubSector* ss = level().subsectors.data();
    for (int i = 0; i < level().subsectors.size(); i++, ss++)
    {
        Seg* seg = &level().segs[ss->firstline];
        ss->sector = seg->sidedef->sector;
    }

    // count number of lines in each sector
    Line* li = level().lines.data();
    int total = 0;
    for (int i = 0; i < level().lines.size(); i++, li++)
    {
        total++;
        li->frontsector->linecount++;

        if (li->backsector && li->backsector != li->frontsector)
        {
            li->backsector->linecount++;
            total++;
        }
    }

    // build line tables for each sector. One flat array carved into per-sector
    // slices, owned by the Level (Sim/Level.h); linebuffer walks it as vanilla's
    // did, and sector->lines points into it.
    level().sectorLines.assign(total, nullptr);
    Line** linebuffer = level().sectorLines.data();
    Sector* sector = level().sectors.data();
    for (int i = 0; i < level().sectors.size(); i++, sector++)
    {
        clearBox(bbox.data());
        sector->lines = linebuffer;
        li = level().lines.data();
        for (int j = 0; j < level().lines.size(); j++, li++)
        {
            if (li->frontsector == sector || li->backsector == sector)
            {
                *linebuffer++ = li;
                addToBox(bbox.data(), li->v1->x, li->v1->y);
                addToBox(bbox.data(), li->v2->x, li->v2->y);
            }
        }
        if (linebuffer - sector->lines != sector->linecount)
            fatalError("Error: groupLines: miscounted");

        // set the DegenMobj to the middle of the bounding box
        sector->soundorg.x = (bbox[boxRight] + bbox[boxLeft]) / 2;
        sector->soundorg.y = (bbox[boxTop] + bbox[boxBottom]) / 2;

        // adjust bounding box to map blocks
        int block = (bbox[boxTop] - level().blockmap.origin.y + MAXRADIUS).raw
                    >> MAPBLOCKSHIFT;
        block =
            block >= level().blockmap.height ? level().blockmap.height - 1 : block;
        sector->blockbox[boxTop] = block;

        block = (bbox[boxBottom] - level().blockmap.origin.y - MAXRADIUS).raw
                >> MAPBLOCKSHIFT;
        block = block < 0 ? 0 : block;
        sector->blockbox[boxBottom] = block;

        block = (bbox[boxRight] - level().blockmap.origin.x + MAXRADIUS).raw
                >> MAPBLOCKSHIFT;
        block = block >= level().blockmap.width ? level().blockmap.width - 1 : block;
        sector->blockbox[boxRight] = block;

        block = (bbox[boxLeft] - level().blockmap.origin.x - MAXRADIUS).raw
                >> MAPBLOCKSHIFT;
        block = block < 0 ? 0 : block;
        sector->blockbox[boxLeft] = block;
    }
}

//
// setupLevel
//
void setupLevel(int episode, int map, int, Skill)
{
    auto& stats = levelStats();
    auto& wminfo_ = intermissionInfo().wminfo;
    auto& players_ = playerState();

    stats.totalkills = stats.totalitems = stats.totalsecret = wminfo_.maxfrags = 0;
    wminfo_.partime = 180;
    for (auto& player: players_.players)
        player.killcount = player.secretcount = player.itemcount = 0;

    // Initial height of PointOfView
    // will be set by player think.
    players_.players[players_.consoleplayer].viewz = Fixed {1};

    // Make sure all sounds are stopped before the level's allocations go.
    startLevelSound();

    // Free the previous level's mobjs and thinker specials - what
    // Z_FreeTags(PU_LEVEL, PU_PURGELEVEL - 1) reclaimed when they lived in the
    // zone. Must run before initThinkers empties the thinker list.
    freeLevelAllocations();

    // UNUSED W_Profile ();
    initThinkers();

    // if working with a devlopment map, reload it
    wad().reload();

    // find map name
    auto lumpname = std::string {};

    if (gameVersion().gamemode == GameMode::Commercial)
    {
        if (map < 10)
            lumpname = concat("map0", map);
        else
            lumpname = concat("map", map);
    }
    else
        lumpname = concat('E', char('0' + episode), 'M', char('0' + map));

    int lumpnum = wad().number(lumpname);

    stats.leveltime = 0;

    // note: most of this ordering is important
    loadBlockMap(lumpnum + mapLumpBlockmap);
    loadVertexes(lumpnum + mapLumpVertexes);
    loadSectors(lumpnum + mapLumpSectors);
    loadSideDefs(lumpnum + mapLumpSidedefs);

    loadLineDefs(lumpnum + mapLumpLinedefs);
    loadSubsectors(lumpnum + mapLumpSsectors);
    loadNodes(lumpnum + mapLumpNodes);
    loadSegs(lumpnum + mapLumpSegs);

    level().rejectMatrix = static_cast<byte*>(cacheLumpNum(lumpnum + mapLumpReject));
    groupLines();

    corpseQueue().bodyqueslot = 0;
    auto& spawns = mapSpawns();
    spawns.deathmatch_p = spawns.deathmatchstarts.data();
    loadThings(lumpnum + mapLumpThings);

    // if deathmatch, randomly spawn the active players
    if (gameSession().deathmatch)
    {
        for (int i = 0; i < MAXPLAYERS; i++)
            if (players_.playeringame[i])
            {
                players_.players[i].mo = nullptr;
                deathMatchSpawnPlayer(i);
            }
    }

    // clear special respawning que
    itemRespawnQueue().iquehead = itemRespawnQueue().iquetail = 0;

    // set up world state
    spawnSpecials();

    // preload graphics
    if (engineParams().precache)
        precacheLevel();
}

//
// init
//
void init()
{
    initSwitchList();
    initPicAnims();
    initSprites(sprnames());
}
} // namespace Doom

// ---------------------------------------------------------------------------
// Global-scope data that was p_setup.cpp. It stays at :: scope because these are the
// vanilla names other translation units (and the eacp port) still link against.
// ---------------------------------------------------------------------------
// The blockmap views and the player/deathmatch starts. Read across the renderer
// and playsim; refreshed by the loaders in Sim/Setup.cpp. Storage here.
//
// The level geometry view-globals (numvertexes/vertexes/...) that used to head this
// block are gone - readers index Level's vectors directly.
// BLOCKMAP
// Created from axis aligned bounding box
// of the map, a rectangular array of
// blocks of size ...
// Used to speed up collision detection
// by spatial subdivision in 2D.
//
// Now Level::blockmap and Level::blockLinks (Sim/Level.h).

// REJECT
// For fast sight rejection.
// Speeds up enemy AI by skipping detailed
//  LineOf Sight calculation.
// Without special effect, this could be
//  used as a PVS lookup as well.
//
// Now Level::rejectMatrix (Sim/Level.h).

// The map's spawn spots are a Doom::MapSpawns owned by the Engine now; these are references
// onto it, the arrays as references-to-array (REFACTOR.md, Step 5).
