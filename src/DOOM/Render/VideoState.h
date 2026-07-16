#pragma once

namespace Doom
{
// Render/Video's dirty-rectangle accumulator: V_MarkRect grows dirtybox to cover every region
// drawn since the last blit (left/bottom/right/top via M_AddToBox). PureDOOM's host reads
// screens[0] whole, so nothing consumes it today, but it is still written every frame.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); externed in
// v_video.h but read only within Render/Video, so the vanilla name becomes a reference onto this
// member (a reference-to-array). screens[] and the gamma table stay in the shim - screens[] is
// host-facing and gammatable is const data.
struct VideoState
{
    int dirtybox[4] = {}; // BOXLEFT/BOXBOTTOM/BOXRIGHT/BOXTOP of the drawn region
};

VideoState& videoState();
} // namespace Doom
