#pragma once

#include "../doomtype.h" // byte

#include "../Containers.h"

namespace Doom
{
// The screen melt's file-local scratch framebuffers - the two working screens UI/Wipe reads and
// writes as the mission-begin melt runs: wipe_scr is the frame the composite is built into
// (screens[0]), wipe_scr_end the incoming ("end") screen it melts in from (screens[3]). The
// outgoing ("start") screen scrStart and the meltRunning flag are here too now - the
// GPU compositor reads all of it through wipeState(), the way it reaches every other
// cluster, rather than through loose globals f_wipe.h used to export.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were UI/Wipe's
// own namespace-scope scratch, read by no other file. UI/Wipe reaches them through a hoisted
// `auto& scratch = wipeState();` local per function rather than a file-scope alias (REFACTOR.md,
// Step 9 strand (a)). The melt runs at every level load (G_DoLoadLevel wipes), so the demo frame
// goldens see it - but relocating the storage changes nothing observable either way (the melt
// walks M_Random, not P_Random, so it never touches the simulation hash).
struct WipeState
{
    // Raised while a melt is running, and the only safe thing to test: exitMelt
    // clears the offset vector's storage but a caller between melts still sees the
    // last frame's stale contents. The GPU melt compositor gates on this. Was the
    // loose global wipe_melt_running.
    bool meltRunning = false;

    // The outgoing frame, as palette indices; initMelt leaves it column-major. The
    // compositor reads it. Was the loose global wipe_scr_start.
    byte* scrStart = nullptr;

    byte* wipe_scr =
        nullptr; // the working frame the composite is built into (screens[0])
    byte* wipe_scr_end = nullptr; // the incoming frame melted in from (screens[3])

    // The melt's per-column offset table (how far down each two-pixel column of the
    // outgoing screen has slid; negative means not started). The compositor reads it
    // directly - it used to reach it through a separate int* view refreshed by
    // initMelt, which is gone: the view existed only so a caller could hold a raw
    // pointer, and Vector already subscripts. exitMelt clears this, so it is only
    // ever read while meltRunning is set (initMelt sizes it in the same breath as
    // raising the flag), which is the "go is the only safe test" rule made
    // structural rather than commented.
    Vector<int> wipe_melt_offsets;
};

// The one WipeState, a view onto the Engine's member - the same pattern as the other clusters
// (finaleState(), intermissionState(), ...).
WipeState& wipeState();
} // namespace Doom
