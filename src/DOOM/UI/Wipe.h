#pragma once

#include "../doomtype.h"
// The screen-melt's cross-read state: whether a melt is running, the outgoing
// frame it reads, and the per-column offsets it composites by. The eacp port's
// GPU melt reads all three. Was f_wipe.h.

extern bool wipe_melt_running;
extern byte* wipe_scr_start;
extern int* wipe_melt_offsets;
namespace Doom
{
// Which wipe to run. Unlike the melt state above, nothing outside the engine reads
// this, so it lives in namespace Doom rather than at global scope. screenWipe uses
// it to pick a row of three from its init/do/exit function table.
enum class WipeType
{
    ColorXForm,
    Melt,
    NumWipes
};

// Screen wipe / melt; f_wipe.cpp keeps the vanilla wipe_ names as shims.
int startScreen(int x, int y, int width, int height);
int endScreen(int x, int y, int width, int height);
int screenWipe(WipeType wipeno, int x, int y, int width, int height, int ticks);
} // namespace Doom
