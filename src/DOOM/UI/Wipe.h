#pragma once


#include "../doomtype.h"
// The screen-melt's cross-read state: whether a melt is running, the outgoing
// frame it reads, and the per-column offsets it composites by. The eacp port's
// GPU melt reads all three. Was f_wipe.h.
enum
{
    wipe_ColorXForm,
    wipe_Melt,
    wipe_NUMWIPES
};

extern doom_boolean wipe_melt_running;
extern byte* wipe_scr_start;
extern int* wipe_melt_offsets;
namespace Doom
{
// Screen wipe / melt; f_wipe.cpp keeps the vanilla wipe_ names as shims.
int startScreen(int x, int y, int width, int height);
int endScreen(int x, int y, int width, int height);
int screenWipe(int wipeno, int x, int y, int width, int height, int ticks);
} // namespace Doom
