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
void Doom::spawnMapThing(MapThing* mthing);

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
    // Determine number of lumps:
    //  total lump length / vertex record length.
    numvertexes = wad().length(lump) / sizeof(MapVertex);

    // Owned by the Level now (Sim/Level.h); vertexes is a view onto its vector.
    // assign, not resize, so a shorter second level does not inherit the first
    // level's tail - Z_Malloc handed back fresh zeroed memory every load.
    level().vertexes.assign(numvertexes, Vertex {});
    vertexes = level().vertexes.data();

    // Load data into cache.
    byte* data = static_cast<byte*>(cacheLumpNum(lump));

    MapVertex* ml = reinterpret_cast<MapVertex*>(data);
    Vertex* li = vertexes;

    // Copy and convert vertex coordinates,
    // internal representation as fixed.
    for (int i = 0; i < numvertexes; i++, li++, ml++)
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
    numsegs = wad().length(lump) / sizeof(MapSeg);
    level().segs.assign(numsegs, Seg {});
    segs = level().segs.data();
    byte* data = static_cast<byte*>(cacheLumpNum(lump));

    MapSeg* ml = reinterpret_cast<MapSeg*>(data);
    Seg* li = segs;
    for (int i = 0; i < numsegs; i++, li++, ml++)
    {
        li->v1 = &vertexes[littleEndian(ml->v1)];
        li->v2 = &vertexes[littleEndian(ml->v2)];

        li->angle = angle_t {(unsigned) (littleEndian(ml->angle)) << 16};
        li->offset = Fixed::fromInt(littleEndian(ml->offset));
        int linedef = littleEndian(ml->linedef);
        Line* ldef = &lines[linedef];
        li->linedef = ldef;
        int side = littleEndian(ml->side);
        li->sidedef = &sides[ldef->sidenum[side]];
        li->frontsector = sides[ldef->sidenum[side]].sector;
        if (ldef->flags & ML_TWOSIDED)
            li->backsector = sides[ldef->sidenum[side ^ 1]].sector;
        else
            li->backsector = nullptr;
    }
}

//
// loadSubsectors
//
void loadSubsectors(int lump)
{
    numsubsectors = wad().length(lump) / sizeof(MapSubsector);
    level().subsectors.assign(numsubsectors, SubSector {});
    subsectors = level().subsectors.data();
    byte* data = static_cast<byte*>(cacheLumpNum(lump));

    MapSubsector* ms = reinterpret_cast<MapSubsector*>(data);
    SubSector* ss = subsectors;

    for (int i = 0; i < numsubsectors; i++, ss++, ms++)
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
    numsectors = wad().length(lump) / sizeof(MapSector);
    level().sectors.assign(numsectors, Sector {});
    sectors = level().sectors.data();
    byte* data = static_cast<byte*>(cacheLumpNum(lump));

    MapSector* ms = reinterpret_cast<MapSector*>(data);
    Sector* ss = sectors;
    for (int i = 0; i < numsectors; i++, ss++, ms++)
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
    numnodes = wad().length(lump) / sizeof(MapNode);
    level().nodes.assign(numnodes, Node {});
    nodes = level().nodes.data();
    byte* data = static_cast<byte*>(cacheLumpNum(lump));

    MapNode* mn = reinterpret_cast<MapNode*>(data);
    Node* no = nodes;

    for (int i = 0; i < numnodes; i++, no++, mn++)
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
        if (gameVersion().gamemode != commercial)
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

        spawnMapThing(mt);
    }
}

//
// loadLineDefs
// Also counts secret lines for intermissions.
//
void loadLineDefs(int lump)
{
    numlines = wad().length(lump) / sizeof(MapLinedef);
    level().lines.assign(numlines, Line {});
    lines = level().lines.data();
    byte* data = static_cast<byte*>(cacheLumpNum(lump));

    MapLinedef* mld = reinterpret_cast<MapLinedef*>(data);
    Line* ld = lines;
    for (int i = 0; i < numlines; i++, mld++, ld++)
    {
        ld->flags = littleEndian(mld->flags);
        ld->special = littleEndian(mld->special);
        ld->tag = littleEndian(mld->tag);
        Vertex* v1 = ld->v1 = &vertexes[littleEndian(mld->v1)];
        Vertex* v2 = ld->v2 = &vertexes[littleEndian(mld->v2)];
        ld->dx = v2->x - v1->x;
        ld->dy = v2->y - v1->y;

        if (!ld->dx)
            ld->slopetype = ST_VERTICAL;
        else if (!ld->dy)
            ld->slopetype = ST_HORIZONTAL;
        else
        {
            if (FixedDiv(ld->dy, ld->dx).isPositive())
                ld->slopetype = ST_POSITIVE;
            else
                ld->slopetype = ST_NEGATIVE;
        }

        if (v1->x < v2->x)
        {
            ld->bbox[BOXLEFT] = v1->x;
            ld->bbox[BOXRIGHT] = v2->x;
        }
        else
        {
            ld->bbox[BOXLEFT] = v2->x;
            ld->bbox[BOXRIGHT] = v1->x;
        }

        if (v1->y < v2->y)
        {
            ld->bbox[BOXBOTTOM] = v1->y;
            ld->bbox[BOXTOP] = v2->y;
        }
        else
        {
            ld->bbox[BOXBOTTOM] = v2->y;
            ld->bbox[BOXTOP] = v1->y;
        }

        ld->sidenum[0] = littleEndian(mld->sidenum[0]);
        ld->sidenum[1] = littleEndian(mld->sidenum[1]);

        if (ld->sidenum[0] != -1)
            ld->frontsector = sides[ld->sidenum[0]].sector;
        else
            ld->frontsector = nullptr;

        if (ld->sidenum[1] != -1)
            ld->backsector = sides[ld->sidenum[1]].sector;
        else
            ld->backsector = nullptr;
    }
}

//
// loadSideDefs
//
void loadSideDefs(int lump)
{
    numsides = wad().length(lump) / sizeof(MapSidedef);
    level().sides.assign(numsides, Side {});
    sides = level().sides.data();
    byte* data = static_cast<byte*>(cacheLumpNum(lump));

    MapSidedef* msd = reinterpret_cast<MapSidedef*>(data);
    Side* sd = sides;
    for (int i = 0; i < numsides; i++, msd++, sd++)
    {
        sd->textureoffset = Fixed::fromInt(littleEndian(msd->textureoffset));
        sd->rowoffset = Fixed::fromInt(littleEndian(msd->rowoffset));
        sd->toptexture = textureNumForName(nameView(msd->toptexture, 8));
        sd->bottomtexture = textureNumForName(nameView(msd->bottomtexture, 8));
        sd->midtexture = textureNumForName(nameView(msd->midtexture, 8));
        sd->sector = &sectors[littleEndian(msd->sector)];
    }
}

//
// loadBlockMap
//
void loadBlockMap(int lump)
{
    blockmaplump = static_cast<short*>(cacheLumpNum(lump));
    int count = wad().length(lump) / 2;

    for (int i = 0; i < count; i++)
        blockmaplump[i] = littleEndian(blockmaplump[i]);

    // Fill the Level's blockmap descriptor from the lump header, then refresh the
    // vanilla globals as views onto it.
    Blockmap& bmap = level().blockmap;
    bmap.lump = blockmaplump;
    bmap.offsets = blockmaplump + 4;
    bmap.origin = {Fixed {blockmaplump[0] << fracBits},
                   Fixed {blockmaplump[1] << fracBits}};
    bmap.width = blockmaplump[2];
    bmap.height = blockmaplump[3];

    blockmap = bmap.offsets;
    bmaporgx = bmap.origin.x;
    bmaporgy = bmap.origin.y;
    bmapwidth = bmap.width;
    bmapheight = bmap.height;

    // clear out mobj chains. The array is the Level's; the mobjs it will point at
    // are the zone's.
    level().blockLinks.assign(bmapwidth * bmapheight, nullptr);
    blocklinks = level().blockLinks.data();
}

//
// groupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
void groupLines()
{
    Array<fixed_t, 4> bbox;

    // look up sector number for each subsector
    SubSector* ss = subsectors;
    for (int i = 0; i < numsubsectors; i++, ss++)
    {
        Seg* seg = &segs[ss->firstline];
        ss->sector = seg->sidedef->sector;
    }

    // count number of lines in each sector
    Line* li = lines;
    int total = 0;
    for (int i = 0; i < numlines; i++, li++)
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
    Sector* sector = sectors;
    for (int i = 0; i < numsectors; i++, sector++)
    {
        clearBox(bbox.data());
        sector->lines = linebuffer;
        li = lines;
        for (int j = 0; j < numlines; j++, li++)
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
        sector->soundorg.x = (bbox[BOXRIGHT] + bbox[BOXLEFT]) / 2;
        sector->soundorg.y = (bbox[BOXTOP] + bbox[BOXBOTTOM]) / 2;

        // adjust bounding box to map blocks
        int block = (bbox[BOXTOP] - bmaporgy + MAXRADIUS).raw >> MAPBLOCKSHIFT;
        block = block >= bmapheight ? bmapheight - 1 : block;
        sector->blockbox[BOXTOP] = block;

        block = (bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS).raw >> MAPBLOCKSHIFT;
        block = block < 0 ? 0 : block;
        sector->blockbox[BOXBOTTOM] = block;

        block = (bbox[BOXRIGHT] - bmaporgx + MAXRADIUS).raw >> MAPBLOCKSHIFT;
        block = block >= bmapwidth ? bmapwidth - 1 : block;
        sector->blockbox[BOXRIGHT] = block;

        block = (bbox[BOXLEFT] - bmaporgx - MAXRADIUS).raw >> MAPBLOCKSHIFT;
        block = block < 0 ? 0 : block;
        sector->blockbox[BOXLEFT] = block;
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
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        players_.players[i].killcount = players_.players[i].secretcount =
            players_.players[i].itemcount = 0;
    }

    // Initial height of PointOfView
    // will be set by player think.
    players_.players[players_.consoleplayer].viewz = fixed_t {1};

    // Make sure all sounds are stopped before the level's allocations go.
    startLevelSound();

    // Free the previous level's mobjs and thinker specials - what
    // Z_FreeTags(PU_LEVEL, PU_PURGELEVEL - 1) reclaimed when they lived in the
    // zone. Must run before Doom::initThinkers empties the thinker list.
    freeLevelAllocations();

    // UNUSED W_Profile ();
    initThinkers();

    // if working with a devlopment map, reload it
    wad().reload();

    // find map name
    auto lumpname = std::string {};

    if (gameVersion().gamemode == commercial)
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
    loadBlockMap(lumpnum + ML_BLOCKMAP);
    loadVertexes(lumpnum + ML_VERTEXES);
    loadSectors(lumpnum + ML_SECTORS);
    loadSideDefs(lumpnum + ML_SIDEDEFS);

    loadLineDefs(lumpnum + ML_LINEDEFS);
    loadSubsectors(lumpnum + ML_SSECTORS);
    loadNodes(lumpnum + ML_NODES);
    loadSegs(lumpnum + ML_SEGS);

    rejectmatrix = static_cast<byte*>(cacheLumpNum(lumpnum + ML_REJECT));
    groupLines();

    corpseQueue().bodyqueslot = 0;
    auto& spawns = mapSpawns();
    spawns.deathmatch_p = spawns.deathmatchstarts.data();
    loadThings(lumpnum + ML_THINGS);

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
    initSprites(sprnames);
}
} // namespace Doom

// ---------------------------------------------------------------------------
// Global-scope data that was p_setup.cpp. It stays at :: scope because these are the
// vanilla names other translation units (and the eacp port) still link against.
// ---------------------------------------------------------------------------
// The level geometry view-globals (numvertexes/vertexes/... over Doom::Level's
// vectors), the blockmap views and the player/deathmatch starts. Read across the
// renderer and playsim; refreshed by the loaders in Sim/Setup.cpp. Storage here.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//
int numvertexes;
Doom::Vertex* vertexes;

int numsegs;
Doom::Seg* segs;

int numsectors;
Doom::Sector* sectors;

int numsubsectors;
Doom::SubSector* subsectors;

int numnodes;
Doom::Node* nodes;

int numlines;
Doom::Line* lines;

int numsides;
Doom::Side* sides;

// BLOCKMAP
// Created from axis aligned bounding box
// of the map, a rectangular array of
// blocks of size ...
// Used to speed up collision detection
// by spatial subdivision in 2D.
//
// Blockmap size.
int bmapwidth;
int bmapheight; // size in mapblocks
short* blockmap; // int for larger maps
// offsets in blockmap are from here
short* blockmaplump;
// origin of block map
fixed_t bmaporgx;
fixed_t bmaporgy;
// for thing chains
Doom::Mobj** blocklinks;

// REJECT
// For fast sight rejection.
// Speeds up enemy AI by skipping detailed
//  LineOf Sight calculation.
// Without special effect, this could be
//  used as a PVS lookup as well.
//
byte* rejectmatrix;

// The map's spawn spots are a Doom::MapSpawns owned by the Engine now; these are references
// onto it, the arrays as references-to-array (REFACTOR.md, Step 5).
