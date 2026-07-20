#pragma once

#include "../doomtype.h" // byte

#include "../Containers.h"

namespace Doom
{
// The screen melt's file-local scratch framebuffers - the two working screens UI/Wipe reads and
// writes as the mission-begin melt runs: wipe_scr is the frame the composite is built into
// (screens[0]), wipe_scr_end the incoming ("end") screen it melts in from (screens[3]). The
// outgoing ("start") screen is not here: wipe_scr_start / wipe_melt_running stay in the
// f_wipe.cpp shim, because the GPU compositor reads them through f_wipe.h - the same split
// AutomapView used (the app-read globals stay exported in the shim, the private scratch moves
// to the Engine). The column-offset table *is* here, as its owning vector (see below), because
// the exported global is now a view onto it rather than its own allocation.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were UI/Wipe's
// own namespace-scope scratch, read by no other file. UI/Wipe reaches them through a hoisted
// `auto& scratch = wipeState();` local per function rather than a file-scope alias (REFACTOR.md,
// Step 9 strand (a)). The melt runs at every level load (G_DoLoadLevel wipes), so the demo frame
// goldens see it - but relocating the storage changes nothing observable either way (the melt
// walks M_Random, not P_Random, so it never touches the simulation hash).
struct WipeState
{
    byte* wipe_scr =
        nullptr; // the working frame the composite is built into (screens[0])
    byte* wipe_scr_end = nullptr; // the incoming frame melted in from (screens[3])

    // The melt's per-column offset table (how far down each two-pixel column of the
    // outgoing screen has slid). RAII-owned (Step 9) - what was a raw doom_malloc'd
    // block. The vanilla name `wipe_melt_offsets` (UI/Wipe.h, read by the eacp app's
    // GPU melt compositor) is a plain-pointer VIEW onto this vector's data(),
    // refreshed by initMelt right after the resize - the same owner/view split
    // GraphicsData's and CompositeCache's arrays use. exitMelt clears this vector but
    // deliberately does *not* refresh the view afterwards: vanilla's wipe_exitMelt
    // freed the block without clearing the pointer to it, and wipe_melt_running (the
    // "go" flag) is the only thing ever safe to test, not the pointer's validity. A
    // freed-but-unrefreshed raw pointer and a cleared-but-unrefreshed view pointer
    // are equally unsafe to read and equally never read, so behaviour is unchanged.
    Vector<int> wipe_melt_offsets;
};

// The one WipeState, a view onto the Engine's member - the same pattern as the other clusters
// (finaleState(), intermissionState(), ...).
WipeState& wipeState();
} // namespace Doom
