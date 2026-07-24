#pragma once

#include "MapTypes.h"
#include "../Render/RenderTypes.h"

#include "Blockmap.h"

#include "../Containers.h"

namespace Doom
{
// A level's static geometry: the arrays that are built once when the level loads
// and thrown away whole when the next one does. This is the PU_LEVEL half of the
// zone allocator's job - the clean half. The other half, mobjs and the thinker
// specials (doors, lifts, lights), have their own per-object lifecycle
// (removeThinker frees them mid-play) and stay on the zone until the playsim is
// rewritten.
//
// Vanilla loads these with Z_Malloc(..., PU_LEVEL) and frees them all at once with
// Z_FreeTags at the top of the next setupLevel. A Level frees them by owning
// them: load a new level and the old vectors are replaced, which is the
// destructor doing what Z_FreeTags did, but scoped and automatic.
//
// The vanilla globals (vertexes, numsegs, sectors, ...) are gone. They were a
// pointer-and-count view onto each vector, refreshed by its loader after every
// assign() - fourteen extern declarations, fourteen definitions, and a standing
// invariant (Tests/Sim/LevelTests.cpp existed to check it) that every one of them
// still pointed at its vector. Readers index the vectors directly now, so the view
// cannot go stale and a count cannot drift from the thing it counts: numsegs is
// segs.size().
//
// The vectors themselves still never move within a level, which is what keeps the
// cross-references the loaders build between them (a seg pointing at a vertex, a
// subsector at a sector) valid.
struct Level
{
    Vector<Vertex> vertexes;
    Vector<Seg> segs;
    Vector<SubSector> subsectors;
    Vector<Sector> sectors;
    Vector<Node> nodes;
    Vector<Line> lines;
    Vector<Side> sides;

    // The per-block mobj chain heads. The array is ours; the mobjs it points at
    // are the zone's.
    Vector<Mobj*> blockLinks;

    // The blockmap descriptor - origin, extent and the lump pointers the iterators
    // read from. Filled by loadBlockMap. blockLinks above is the one blockmap array
    // that is ours to allocate; the lump itself is WadFile's.
    Blockmap blockmap;

    // The REJECT lump: one bit per sector pair, saying the two can never see each
    // other, so checkSight can answer without tracing. WadFile's memory, like
    // blockmap.lump above - the Level only holds the cursor.
    byte* rejectMatrix = nullptr;

    // One flat array of line pointers, carved into per-sector slices that
    // Sector::lines point into. Vanilla calls this `linebuffer`, a single
    // Z_Malloc in groupLines.
    Vector<Line*> sectorLines;
};

// The engine's one level, for as long as the engine has one of everything.
Level& level();
} // namespace Doom
