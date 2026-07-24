#pragma once

#include "../doomtype.h" // byte

#include "../Containers.h"

namespace Doom
{
// Render/Video's dirty-rectangle accumulator: markRect grows dirtybox to cover every region
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
// the constructor fills and slices into screens[0..3] (screens[0]'s slice is then
// overwritten by initGraphics, as vanilla left it); frame is the software frame proper
// (screens[0], initGraphics); statusBar is the status bar back-buffer (screens[4],
// initStatusBar). Each is filled by the drawers, so the frame goldens pin them exactly.
struct VideoState
{
    // Allocates the workspace and slices screens[0..3] into it - what initVideo used
    // to do at boot, now owned by construction so a freshly built (or resetEngine'd)
    // Engine has a valid framebuffer without a separate init step. Defined in
    // Render/Video.cpp, where SCREENWIDTH/SCREENHEIGHT already are. screens[0]'s slice
    // is later overwritten by initGraphics, exactly as vanilla left it.
    VideoState();

    // boxLeft/boxBottom/boxRight/boxTop (Math/BBox.h) of the drawn region, in screen
    // pixels. It is plain int, not Fixed - there is no fixed-point quantity here,
    // only pixel coordinates - so Render/Video.cpp grows it with its own int-typed
    // else-if update rather than addToBox, which is typed for a real BBox.
    Array<int, 4> dirtybox = {};

    Vector<byte> frame; // screens[0]: the software framebuffer
    Vector<byte>
        workspace; // the base block the constructor slices into screens[0..3]
    Vector<byte> statusBar; // screens[4]: the status bar back-buffer

    // The five drawing surfaces as raw byte* views into the three owners above.
    // screens[0] is the software frame (a view onto `frame`, set by initGraphics;
    // the eacp port swaps it to its overlay-capture scratch and restores it), [1..3]
    // slices of `workspace`, [4] the status-bar back-buffer. Raw because they are
    // legitimately reseated. Was the loose byte* screens[5].
    Array<byte*, 5> screens = {};

    // The 256-entry RGB palette the finished frame is expanded through. The engine
    // writes it (setPalette, on the damage/pickup/invulnerability flashes); the host
    // and the eacp port read it to expand screens[0] to RGBA, and the frame hash
    // mixes it. Was the loose screen_palette[256 * 3] in Host/Video.cpp.
    Array<byte, 256 * 3> screen_palette = {};
};

VideoState& videoState();
} // namespace Doom
