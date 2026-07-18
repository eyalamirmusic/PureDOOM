// Rewritten out of vanilla p_setup into namespace Doom.
//
// Level loading: parse each map lump into Doom::Level's vectors and refresh the
// geometry view-globals the renderer and playsim index, build the blockmap
// descriptor and per-sector line lists, spawn the things, and set up a fresh level.
// p_setup.cpp shims Doom::setupLevel and Doom::init and owns the view-global storage; the
// per-lump loaders are file-local. Golden-neutral - every demo loads its level
// through this, and LevelTests pins the view invariant.

#include "../doom_config.h"

#include "../doomdef.h"
#include "../doomstat.h"
#include "../g_game.h"
#include "../i_system.h"
#include "../m_bbox.h"
#include "../m_swap.h"
#include "../p_local.h"
#include "../s_sound.h"
#include "../Wad/WadFile.h"

#include "Level.h"
#include "Setup.h"
#include "Tick.h" // levelAlloc / levelFree / freeLevelAllocations
#include "../Render/Data.h"

#include "../Render/Things.h"
#include "Specials.h"
#include <ea_data_structures/Structures/Array.h>

// Doom::spawnMapThing is Doom::Mobj's now (global shim); the things loader calls it.
#include "../Game/Game.h"
#include "../Game/Sound.h"
#include "../Host/System.h"
#include "Mobj.h"
#include "Switches.h"
void Doom::spawnMapThing(Doom::MapThing* mthing);

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
    byte* data;
    MapVertex* ml;
    Vertex* li;

    // Determine number of lumps:
    //  total lump length / vertex record length.
    numvertexes = Doom::wad().length(lump) / sizeof(MapVertex);

    // Owned by the Level now (Sim/Level.h); vertexes is a view onto its vector.
    // assign, not resize, so a shorter second level does not inherit the first
    // level's tail - Z_Malloc handed back fresh zeroed memory every load.
    Doom::level().vertexes.assign(numvertexes, Vertex {});
    vertexes = Doom::level().vertexes.data();

    // Load data into cache.
    data = static_cast<byte*>(Doom::cacheLumpNum(lump));

    ml = reinterpret_cast<MapVertex*>(data);
    li = vertexes;

    // Copy and convert vertex coordinates,
    // internal representation as fixed.
    for (int i = 0; i < numvertexes; i++, li++, ml++)
    {
        li->x = SHORT(ml->x) << FRACBITS;
        li->y = SHORT(ml->y) << FRACBITS;
    }

    // Free buffer memory.
}

//
// loadSegs
//
void loadSegs(int lump)
{
    byte* data;
    MapSeg* ml;
    Seg* li;
    Line* ldef;
    int linedef;
    int side;

    numsegs = Doom::wad().length(lump) / sizeof(MapSeg);
    Doom::level().segs.assign(numsegs, Seg {});
    segs = Doom::level().segs.data();
    data = static_cast<byte*>(Doom::cacheLumpNum(lump));

    ml = reinterpret_cast<MapSeg*>(data);
    li = segs;
    for (int i = 0; i < numsegs; i++, li++, ml++)
    {
        li->v1 = &vertexes[SHORT(ml->v1)];
        li->v2 = &vertexes[SHORT(ml->v2)];

        li->angle = (SHORT(ml->angle)) << 16;
        li->offset = (SHORT(ml->offset)) << 16;
        linedef = SHORT(ml->linedef);
        ldef = &lines[linedef];
        li->linedef = ldef;
        side = SHORT(ml->side);
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
    byte* data;
    MapSubsector* ms;
    SubSector* ss;

    numsubsectors = Doom::wad().length(lump) / sizeof(MapSubsector);
    Doom::level().subsectors.assign(numsubsectors, SubSector {});
    subsectors = Doom::level().subsectors.data();
    data = static_cast<byte*>(Doom::cacheLumpNum(lump));

    ms = reinterpret_cast<MapSubsector*>(data);
    ss = subsectors;

    for (int i = 0; i < numsubsectors; i++, ss++, ms++)
    {
        ss->numlines = SHORT(ms->numsegs);
        ss->firstline = SHORT(ms->firstseg);
    }
}

//
// loadSectors
//
void loadSectors(int lump)
{
    byte* data;
    MapSector* ms;
    Sector* ss;

    numsectors = Doom::wad().length(lump) / sizeof(MapSector);
    Doom::level().sectors.assign(numsectors, Sector {});
    sectors = Doom::level().sectors.data();
    data = static_cast<byte*>(Doom::cacheLumpNum(lump));

    ms = reinterpret_cast<MapSector*>(data);
    ss = sectors;
    for (int i = 0; i < numsectors; i++, ss++, ms++)
    {
        ss->floorheight = SHORT(ms->floorheight) << FRACBITS;
        ss->ceilingheight = SHORT(ms->ceilingheight) << FRACBITS;
        ss->floorpic = Doom::flatNumForName(ms->floorpic);
        ss->ceilingpic = Doom::flatNumForName(ms->ceilingpic);
        ss->lightlevel = SHORT(ms->lightlevel);
        ss->special = SHORT(ms->special);
        ss->tag = SHORT(ms->tag);
        ss->thinglist = nullptr;
    }
}

//
// loadNodes
//
void loadNodes(int lump)
{
    byte* data;
    MapNode* mn;
    Node* no;

    numnodes = Doom::wad().length(lump) / sizeof(MapNode);
    Doom::level().nodes.assign(numnodes, Node {});
    nodes = Doom::level().nodes.data();
    data = static_cast<byte*>(Doom::cacheLumpNum(lump));

    mn = reinterpret_cast<MapNode*>(data);
    no = nodes;

    for (int i = 0; i < numnodes; i++, no++, mn++)
    {
        no->x = SHORT(mn->x) << FRACBITS;
        no->y = SHORT(mn->y) << FRACBITS;
        no->dx = SHORT(mn->dx) << FRACBITS;
        no->dy = SHORT(mn->dy) << FRACBITS;
        for (int j = 0; j < 2; j++)
        {
            no->children[j] = SHORT(mn->children[j]);
            for (int k = 0; k < 4; k++)
                no->bbox[j][k] = SHORT(mn->bbox[j][k]) << FRACBITS;
        }
    }
}

//
// loadThings
//
void loadThings(int lump)
{
    byte* data;
    MapThing* mt;
    int numthings;
    doom_boolean spawn;

    data = static_cast<byte*>(Doom::cacheLumpNum(lump));
    numthings = Doom::wad().length(lump) / sizeof(MapThing);

    mt = reinterpret_cast<MapThing*>(data);
    for (int i = 0; i < numthings; i++, mt++)
    {
        spawn = true;

        // Do not spawn cool, new monsters if !commercial
        if (gamemode != commercial)
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
        mt->x = SHORT(mt->x);
        mt->y = SHORT(mt->y);
        mt->angle = SHORT(mt->angle);
        mt->type = SHORT(mt->type);
        mt->options = SHORT(mt->options);

        Doom::spawnMapThing(mt);
    }
}

//
// loadLineDefs
// Also counts secret lines for intermissions.
//
void loadLineDefs(int lump)
{
    byte* data;
    MapLinedef* mld;
    Line* ld;
    Vertex* v1;
    Vertex* v2;

    numlines = Doom::wad().length(lump) / sizeof(MapLinedef);
    Doom::level().lines.assign(numlines, Line {});
    lines = Doom::level().lines.data();
    data = static_cast<byte*>(Doom::cacheLumpNum(lump));

    mld = reinterpret_cast<MapLinedef*>(data);
    ld = lines;
    for (int i = 0; i < numlines; i++, mld++, ld++)
    {
        ld->flags = SHORT(mld->flags);
        ld->special = SHORT(mld->special);
        ld->tag = SHORT(mld->tag);
        v1 = ld->v1 = &vertexes[SHORT(mld->v1)];
        v2 = ld->v2 = &vertexes[SHORT(mld->v2)];
        ld->dx = v2->x - v1->x;
        ld->dy = v2->y - v1->y;

        if (!ld->dx)
            ld->slopetype = ST_VERTICAL;
        else if (!ld->dy)
            ld->slopetype = ST_HORIZONTAL;
        else
        {
            if (FixedDiv(ld->dy, ld->dx) > 0)
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

        ld->sidenum[0] = SHORT(mld->sidenum[0]);
        ld->sidenum[1] = SHORT(mld->sidenum[1]);

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
    byte* data;
    MapSidedef* msd;
    Side* sd;

    numsides = Doom::wad().length(lump) / sizeof(MapSidedef);
    Doom::level().sides.assign(numsides, Side {});
    sides = Doom::level().sides.data();
    data = static_cast<byte*>(Doom::cacheLumpNum(lump));

    msd = reinterpret_cast<MapSidedef*>(data);
    sd = sides;
    for (int i = 0; i < numsides; i++, msd++, sd++)
    {
        sd->textureoffset = SHORT(msd->textureoffset) << FRACBITS;
        sd->rowoffset = SHORT(msd->rowoffset) << FRACBITS;
        sd->toptexture = Doom::textureNumForName(msd->toptexture);
        sd->bottomtexture = Doom::textureNumForName(msd->bottomtexture);
        sd->midtexture = Doom::textureNumForName(msd->midtexture);
        sd->sector = &sectors[SHORT(msd->sector)];
    }
}

//
// loadBlockMap
//
void loadBlockMap(int lump)
{
    int count;

    blockmaplump = static_cast<short*>(Doom::cacheLumpNum(lump));
    count = Doom::wad().length(lump) / 2;

    for (int i = 0; i < count; i++)
        blockmaplump[i] = SHORT(blockmaplump[i]);

    // Fill the Level's blockmap descriptor from the lump header, then refresh the
    // vanilla globals as views onto it.
    Doom::Blockmap& bmap = Doom::level().blockmap;
    bmap.lump = blockmaplump;
    bmap.offsets = blockmaplump + 4;
    bmap.origin = {Doom::Fixed {blockmaplump[0] << FRACBITS},
                   Doom::Fixed {blockmaplump[1] << FRACBITS}};
    bmap.width = blockmaplump[2];
    bmap.height = blockmaplump[3];

    blockmap = bmap.offsets;
    bmaporgx = bmap.origin.x.raw;
    bmaporgy = bmap.origin.y.raw;
    bmapwidth = bmap.width;
    bmapheight = bmap.height;

    // clear out mobj chains. The array is the Level's; the mobjs it will point at
    // are the zone's.
    Doom::level().blockLinks.assign(bmapwidth * bmapheight, nullptr);
    blocklinks = Doom::level().blockLinks.data();
}

//
// groupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
void groupLines()
{
    Line** linebuffer;
    int total;
    Line* li;
    Sector* sector;
    SubSector* ss;
    Seg* seg;
    EA::Array<fixed_t, 4> bbox;
    int block;

    // look up sector number for each subsector
    ss = subsectors;
    for (int i = 0; i < numsubsectors; i++, ss++)
    {
        seg = &segs[ss->firstline];
        ss->sector = seg->sidedef->sector;
    }

    // count number of lines in each sector
    li = lines;
    total = 0;
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
    Doom::level().sectorLines.assign(total, nullptr);
    linebuffer = Doom::level().sectorLines.data();
    sector = sectors;
    for (int i = 0; i < numsectors; i++, sector++)
    {
        M_ClearBox(bbox.data());
        sector->lines = linebuffer;
        li = lines;
        for (int j = 0; j < numlines; j++, li++)
        {
            if (li->frontsector == sector || li->backsector == sector)
            {
                *linebuffer++ = li;
                M_AddToBox(bbox.data(), li->v1->x, li->v1->y);
                M_AddToBox(bbox.data(), li->v2->x, li->v2->y);
            }
        }
        if (linebuffer - sector->lines != sector->linecount)
            fatalError("Error: groupLines: miscounted");

        // set the DegenMobj to the middle of the bounding box
        sector->soundorg.x = (bbox[BOXRIGHT] + bbox[BOXLEFT]) / 2;
        sector->soundorg.y = (bbox[BOXTOP] + bbox[BOXBOTTOM]) / 2;

        // adjust bounding box to map blocks
        block = (bbox[BOXTOP] - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;
        block = block >= bmapheight ? bmapheight - 1 : block;
        sector->blockbox[BOXTOP] = block;

        block = (bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
        block = block < 0 ? 0 : block;
        sector->blockbox[BOXBOTTOM] = block;

        block = (bbox[BOXRIGHT] - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
        block = block >= bmapwidth ? bmapwidth - 1 : block;
        sector->blockbox[BOXRIGHT] = block;

        block = (bbox[BOXLEFT] - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
        block = block < 0 ? 0 : block;
        sector->blockbox[BOXLEFT] = block;
    }
}

//
// setupLevel
//
void setupLevel(int episode, int map, int, Skill)
{
    EA::Array<char, 9> lumpname;
    int lumpnum;

    totalkills = totalitems = totalsecret = wminfo.maxfrags = 0;
    wminfo.partime = 180;
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        players[i].killcount = players[i].secretcount = players[i].itemcount = 0;
    }

    // Initial height of PointOfView
    // will be set by player think.
    players[consoleplayer].viewz = 1;

    // Make sure all sounds are stopped before the level's allocations go.
    Doom::startLevelSound();

    // Free the previous level's mobjs and thinker specials - what
    // Z_FreeTags(PU_LEVEL, PU_PURGELEVEL - 1) reclaimed when they lived in the
    // zone. Must run before Doom::initThinkers empties the thinker list.
    freeLevelAllocations();

    // UNUSED W_Profile ();
    Doom::initThinkers();

    // if working with a devlopment map, reload it
    Doom::wad().reload();

    // find map name
    if (gamemode == commercial)
    {
        if (map < 10)
        {
            //doom_sprintf(lumpname, "map0%i", map);
            doom_strcpy(lumpname.data(), "map0");
            doom_concat(lumpname.data(), doom_itoa(map, 10));
        }
        else
        {
            //doom_sprintf(lumpname, "map%i", map);
            doom_strcpy(lumpname.data(), "map");
            doom_concat(lumpname.data(), doom_itoa(map, 10));
        }
    }
    else
    {
        lumpname[0] = 'E';
        lumpname[1] = '0' + episode;
        lumpname[2] = 'M';
        lumpname[3] = '0' + map;
        lumpname[4] = 0;
    }

    lumpnum = Doom::wad().number(lumpname.data());

    leveltime = 0;

    // note: most of this ordering is important
    loadBlockMap(lumpnum + ML_BLOCKMAP);
    loadVertexes(lumpnum + ML_VERTEXES);
    loadSectors(lumpnum + ML_SECTORS);
    loadSideDefs(lumpnum + ML_SIDEDEFS);

    loadLineDefs(lumpnum + ML_LINEDEFS);
    loadSubsectors(lumpnum + ML_SSECTORS);
    loadNodes(lumpnum + ML_NODES);
    loadSegs(lumpnum + ML_SEGS);

    rejectmatrix = static_cast<byte*>(Doom::cacheLumpNum(lumpnum + ML_REJECT));
    groupLines();

    bodyqueslot = 0;
    deathmatch_p = deathmatchstarts;
    loadThings(lumpnum + ML_THINGS);

    // if deathmatch, randomly spawn the active players
    if (deathmatch)
    {
        for (int i = 0; i < MAXPLAYERS; i++)
            if (playeringame[i])
            {
                players[i].mo = nullptr;
                Doom::deathMatchSpawnPlayer(i);
            }
    }

    // clear special respawning que
    iquehead = iquetail = 0;

    // set up world state
    Doom::spawnSpecials();

    // preload graphics
    if (precache)
        Doom::precacheLevel();
}

//
// init
//
void init()
{
    Doom::initSwitchList();
    Doom::initPicAnims();
    Doom::initSprites(sprnames);
}
} // namespace Doom
