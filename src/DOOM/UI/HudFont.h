#pragma once

#include "../Sim/MapTypes.h"
#include "../Render/RenderTypes.h" // Patch

#include "../Containers.h"

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
    // '_' - '!' + 1, the same number UI/Hud.h spells HU_FONTSIZE. Kept as a local constant so this
    // header needn't include Hud.h; a static_assert in UI/Hud.cpp ties the two, which is what the
    // lookups in UI/Finale, UI/Menu and Game/Config bound themselves with.
    static constexpr int fontSize = '_' - '!' + 1;

    Array<Patch*, fontSize> hu_font = {}; // the cached '!'..'_' glyph patches
};

// The one HudFont, a view onto the Engine's member - the same pattern as the other clusters
// (statusBarGraphics(), hudFlags(), ...).
HudFont& hudFont();
} // namespace Doom
