#pragma once

#include "../Sim/MapTypes.h"
#include "../Render/RenderTypes.h" // Patch

namespace Doom
{
// The heads-up font: the small red glyphs the HUD, the menu, the finale and the config's
// message-drawing helper all render text with. Doom::startHud caches HU_FONTSTART..HU_FONTEND
// ('!'..'_') from the WAD once (STCFN033..) and everything reads it after - init-once, read-only,
// the UI's shared font, the text equivalent of StatusBarGraphics' loaded patches.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5). It was a loose
// global defined in the hu_stuff.cpp shim and reached by a bare extern in UI/Hud, UI/Menu,
// UI/Finale and Game/Config. Doom::startHud writes the array, so those definitions and every extern
// become references onto this member: a plain array extern against the reference definition would
// read the reference's hidden pointer as the first glyph and fault (the load-bearing trap the whole
// sweep turns on). Live frame-golden-covered - the HUD, the level title and the menu draw text with
// it every tic.
struct HudFont
{
    // HU_FONTSIZE in hu_stuff.h: '_' - '!' + 1. Kept as a local constant so this clean header
    // needn't drag the vanilla hu_stuff.h in; the reference-to-array binding in the shim self-checks
    // it against the macro - a drift won't compile (the same guard StatusBarGraphics uses for
    // ST_NUMFACES).
    static constexpr int fontSize = '_' - '!' + 1;

    Patch* hu_font[fontSize] = {}; // the cached '!'..'_' glyph patches
};

// The one HudFont, a view onto the Engine's member - the same pattern as the other clusters
// (statusBarGraphics(), hudFlags(), ...).
HudFont& hudFont();
} // namespace Doom
