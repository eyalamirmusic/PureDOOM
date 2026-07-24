#pragma once

#include "../doomtype.h"

#include "WipeState.h" // the melt's cross-read state - was three externs here

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
