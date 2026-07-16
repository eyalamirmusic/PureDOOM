#pragma once

#include "../r_defs.h" // patch_t

namespace Doom
{
// The status-bar widget library's own loaded graphic: sttminus, the STTMINUS lump the number
// widget draws in front of a negative count (the "hack display negative frags"). STlib_init caches
// it from the WAD once, at widget-creation time, and drawNum reads it after - init-once, read-only,
// UI/StatusWidgets' single piece of file-scope state, distinct from StatusBarGraphics (which the
// status bar proper loads and owns).
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5). It was a loose global
// in the UI/StatusWidgets unit, read by no other file; its definition becomes a reference onto this
// member. Kept the widget library's own cluster rather than folded into StatusBarGraphics, so the
// widget library stays self-contained and does not reach into the status bar's state. Live
// frame-golden-covered - the bar's number widgets draw into screens[0] every tic.
struct StatusWidgetGraphics
{
    patch_t* sttminus = nullptr; // the STTMINUS lump, drawn before a negative count
};

// The one StatusWidgetGraphics, a view onto the Engine's member - the same pattern as the other
// clusters (statusBarGraphics(), hudFont(), ...).
StatusWidgetGraphics& statusWidgetGraphics();
} // namespace Doom
