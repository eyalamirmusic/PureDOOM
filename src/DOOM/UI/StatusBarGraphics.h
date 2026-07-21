#pragma once

#include "../Game/GameDefs.h" // numCards
#include "../Sim/MapTypes.h"
#include "../Render/RenderTypes.h" // Patch

#include "../Containers.h"

namespace Doom
{
// The status bar's loaded graphics - the patches ST_loadGraphics reads from the WAD once and the
// widgets then draw from: sbar/armsbg are the bar's left and right halves, tallnum/tallpercent the
// big health/armor/ammo digits, shortnum the small arms-count digits, keys the six key-card and
// skull icons, arms the on/off weapon-ownership glyphs, and faces/faceback the animated face and
// its backdrop. Init-once and read-only after, the status bar's equivalent of Render's
// GraphicsData.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// UI/StatusBar's own file-local statics (internal linkage, read by no other file). They were reached
// through file-scope reference aliases (the arrays as references-to-array) until the file-local-alias
// sweep (REFACTOR.md, Step 9 strand (a)) retired them - loadGraphics, the loader, and every drawer
// that reads more than one of these patches hoist a single `auto& gfx = statusBarGraphics();` and
// reach them through it. Loaded from the WAD each level start and drawn into screens[0] every tic,
// so this is live frame-golden-covered.
struct StatusBarGraphics
{
    // ST_NUMFACES in UI/StatusBar: (straight 3 + turn 2 + special 3) painfaces 5 + 2 extra. The
    // reference-to-array binding there self-checks this against the macro - a drift won't compile.
    static constexpr int numFaces = (3 + 2 + 3) * 5 + 2;

    Patch* sbar = nullptr; // main bar, left half
    Patch* armsbg = nullptr; // main bar, right half (arms panel)
    Array<Patch*, 10> tallnum = {}; // 0-9, the tall health/armor/ammo digits
    Patch* tallpercent = nullptr; // the tall % sign
    Array<Patch*, 10> shortnum = {}; // 0-9, the small yellow arms-count digits
    Array<Patch*, numCards> keys = {}; // the key-card and skull icons
    Array<Array<Patch*, 2>, 6> arms = {}; // the six weapons' on/off ownership glyphs
    Array<Patch*, numFaces> faces = {}; // the animated-face patches
    Patch* faceback = nullptr; // the face backdrop (deathmatch)
};

// The one StatusBarGraphics, a view onto the Engine's member - the same pattern as the other
// clusters (statusBarWidgets(), graphicsData(), ...).
StatusBarGraphics& statusBarGraphics();
} // namespace Doom
