#pragma once

#include "../Game/GameDefs.h" // NUMCARDS
#include "../Sim/MapTypes.h"
#include "../Render/RenderTypes.h" // Patch

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
// UI/StatusBar's own file-local statics (internal linkage, read by no other file). The vanilla
// names become references onto the members (the arrays as references-to-array). Loaded from the WAD
// each level start and drawn into screens[0] every tic, so this is live frame-golden-covered.
struct StatusBarGraphics
{
    // ST_NUMFACES in UI/StatusBar: (straight 3 + turn 2 + special 3) painfaces 5 + 2 extra. The
    // reference-to-array binding there self-checks this against the macro - a drift won't compile.
    static constexpr int numFaces = (3 + 2 + 3) * 5 + 2;

    Patch* sbar = nullptr; // main bar, left half
    Patch* armsbg = nullptr; // main bar, right half (arms panel)
    Patch* tallnum[10] = {}; // 0-9, the tall health/armor/ammo digits
    Patch* tallpercent = nullptr; // the tall % sign
    Patch* shortnum[10] = {}; // 0-9, the small yellow arms-count digits
    Patch* keys[NUMCARDS] = {}; // the key-card and skull icons
    Patch* arms[6][2] = {}; // the six weapons' on/off ownership glyphs
    Patch* faces[numFaces] = {}; // the animated-face patches
    Patch* faceback = nullptr; // the face backdrop (deathmatch)
};

// The one StatusBarGraphics, a view onto the Engine's member - the same pattern as the other
// clusters (statusBarWidgets(), graphicsData(), ...).
StatusBarGraphics& statusBarGraphics();
} // namespace Doom
