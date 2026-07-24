#pragma once

namespace Doom
{
// doomstat.h's "Internal parameters, used for engine" section - the three loose scalars that are
// neither gameplay rules, display flags, nor config-backed settings: debugfile (the -debugfile
// output handle the netcode writes packet traces to, 0 when unset), precache (load every lump's
// graphics at level load rather than lazily), and singletics (run the loop one built-and-run tic per
// iteration with no adaptive timing - always true in PureDOOM, and load-bearing: netUpdate builds no
// command under it, the fix for the five-tic input lag, see CLAUDE.md).
//
// REFACTOR.md had deferred these as too scattered to bucket; they are migrated here because
// doomstat.h itself groups them ("Internal parameters, used for engine"), which is organizing
// principle enough, and it clears the last non-config-backed loose scalars from doomstat.h. Each was
// defined at file scope above its owner's namespace (debugfile/singletics in Game/DoomMain, precache
// in Game/Game) and externed only in doomstat.h; the vanilla names become references onto the
// members, and none has its address captured by Config.cpp's defaults[] (the trap that blocks the
// sound volumes), so the static-init reference binding is safe.
//
// singletics is live simulation-golden-covered (netUpdate/doomLoop read it every tic); precache is
// golden-neutral (preloading changes only *when* a lump is read, not the pixels), so its load-bearing
// `true` default is pinned by StateClusterTests instead.
struct EngineParams
{
    void* debugfile = nullptr; // -debugfile packet-trace output (0 = none)
    bool precache = true; // load all graphics at level load
    bool singletics = true; // one tic per loop iteration, no adaptiveness
};

// The one EngineParams, a view onto the Engine's member - the same pattern as the other clusters.
EngineParams& engineParams();
} // namespace Doom
