#pragma once

#include "../Math/FixedPoint.h" // fixed_t

#include "MapGeometry.h" // DivLine

namespace Doom
{
// The line-of-sight check's scratch (Doom::checkSight and its P_SightBlockLinesIterator /
// P_CrossSubsector walk): the looker's eye z, the trace from the looker to the target as a divline,
// the target point, and the two-slot counter Doom::checkSight bumps (sightcounts[0] rejected by the
// reject matrix, [1] tested for real).
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were Sim/Sight's
// own namespace-scope private globals, read by no other file. The vanilla names become references
// onto the members (sightcounts as a reference-to-array). Live simulation-golden-covered - every
// monster that wakes on the player does so through Doom::checkSight - so the byte-identical *.hashes are
// a live confirmation.
struct SightScratch
{
    fixed_t sightzstart {}; // eye z of the looker
    DivLine strace = {}; // the trace from looker (t1) to target (t2)
    fixed_t t2x {}; // target x
    fixed_t t2y {}; // target y
    int sightcounts[2] = {}; // [0] reject-matrix skips, [1] real tests
};

// The one SightScratch, a view onto the Engine's member - the same pattern as the other clusters
// (actionScratch(), weaponScratch(), ...).
SightScratch& sightScratch();
} // namespace Doom
