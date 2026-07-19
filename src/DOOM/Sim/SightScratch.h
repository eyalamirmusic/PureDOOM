#pragma once

namespace Doom
{
// sightcounts is the only member left here: the two-slot counter Doom::checkSight
// bumps (sightcounts[0] rejected by the reject matrix, [1] tested for real). It is
// genuine cross-call state - it accumulates for the engine's whole lifetime, unlike
// the sight trace itself - so it is touched through sightScratch() at its two use
// sites rather than cached by a file-scope reference.
//
// The eye z, the trace and the target point that used to live here (sightzstart,
// strace, t2x, t2y) moved out entirely: they never escaped Doom::checkSight's own
// call chain, so they are per-call locals now, threaded by reference through
// crossBSPNode/crossSubsector the same way topslope/bottomslope already were
// (Sim/Sight.cpp's SightTrace). Live simulation-golden-covered - every monster that
// wakes on the player does so through Doom::checkSight - so the byte-identical
// *.hashes are a live confirmation.
struct SightScratch
{
    int sightcounts[2] = {}; // [0] reject-matrix skips, [1] real tests
};

// The one SightScratch, a view onto the Engine's member - the same pattern as the other clusters
// (actionScratch(), weaponScratch(), ...).
SightScratch& sightScratch();
} // namespace Doom
