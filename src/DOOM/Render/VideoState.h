#pragma once

#include "../doomtype.h" // byte

#include <ea_data_structures/Structures/Array.h>
#include <ea_data_structures/Structures/Vector.h>

namespace Doom
{
// Render/Video's dirty-rectangle accumulator: V_MarkRect grows dirtybox to cover every region
// drawn since the last blit (left/bottom/right/top via M_AddToBox). PureDOOM's host reads
// screens[0] whole, so nothing consumes it today, but it is still written every frame.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); externed in
// v_video.h but read only within Render/Video, so the vanilla name becomes a reference onto this
// member (a reference-to-array). The gamma table stays in the shim (const data).
//
// The software renderer's framebuffers are RAII-owned here now (Step 9) - what were three
// boot-once, never-freed doom_malloc / I_AllocLow blocks. The vanilla name screens[] (a
// byte*[5] in Render/Video) stays a raw VIEW array pointing at these owners' data(), because
// it is legitimately reseated - the eacp port swaps screens[0] to its overlay-capture scratch
// and restores it - so it is a pointer that merely refers, kept raw by the RAII rules. workspace
// is the one contiguous SCREENWIDTH*SCREENHEIGHT*4 block V_Init sliced into screens[0..3]
// (screens[0]'s slice is then overwritten by I_InitGraphics, as vanilla left it); frame is the
// software frame proper (screens[0], I_InitGraphics); statusBar is the status bar back-buffer
// (screens[4], ST_Init). Each is filled by the drawers, so the frame goldens pin them exactly.
struct VideoState
{
    EA::Array<int, 4> dirtybox =
        {}; // BOXLEFT/BOXBOTTOM/BOXRIGHT/BOXTOP of the drawn region

    EA::Vector<byte> frame; // screens[0]: the software framebuffer
    EA::Vector<byte> workspace; // the V_Init base block sliced into screens[0..3]
    EA::Vector<byte> statusBar; // screens[4]: the status bar back-buffer
};

VideoState& videoState();
} // namespace Doom
