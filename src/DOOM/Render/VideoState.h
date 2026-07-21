#pragma once

#include "../doomtype.h" // byte

#include "../Containers.h"

namespace Doom
{
// Render/Video's dirty-rectangle accumulator: Doom::markRect grows dirtybox to cover every region
// drawn since the last blit (left/bottom/right/top). PureDOOM's host reads screens[0] whole, so
// nothing consumes it today, but it is still written every frame.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); externed in
// v_video.h but read only within Render/Video, so the vanilla name becomes a reference onto this
// member (a reference-to-array). The gamma table stays in the shim (const data).
//
// The software renderer's framebuffers are RAII-owned here (Step 9): frame, workspace
// and statusBar below are the Vector<byte> owners of that memory. The vanilla name
// screens[] (a byte*[5] in Render/Video) stays a raw VIEW array pointing at these owners'
// data(), because it is legitimately reseated - the eacp port swaps screens[0] to its
// overlay-capture scratch and restores it - so it is a pointer that merely refers, kept raw
// by the RAII rules. workspace is the one contiguous SCREENWIDTH*SCREENHEIGHT*4 block
// Doom::initVideo fills and slices into screens[0..3] (screens[0]'s slice is then
// overwritten by initGraphics, as vanilla left it); frame is the software frame proper
// (screens[0], initGraphics); statusBar is the status bar back-buffer (screens[4],
// Doom::initStatusBar). Each is filled by the drawers, so the frame goldens pin them exactly.
struct VideoState
{
    // boxLeft/boxBottom/boxRight/boxTop (Math/BBox.h) of the drawn region, in screen
    // pixels. It is plain int, not fixed_t - there is no fixed-point quantity here,
    // only pixel coordinates - so Render/Video.cpp grows it with its own int-typed
    // else-if update rather than Doom::addToBox, which is typed for a real Doom::BBox.
    Array<int, 4> dirtybox = {};

    Vector<byte> frame; // screens[0]: the software framebuffer
    Vector<byte>
        workspace; // the Doom::initVideo base block sliced into screens[0..3]
    Vector<byte> statusBar; // screens[4]: the status bar back-buffer
};

VideoState& videoState();
} // namespace Doom
