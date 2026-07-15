#pragma once

namespace Doom
{
// Screen wipe / melt; f_wipe.cpp keeps the vanilla wipe_ names as shims.
int startScreen(int x, int y, int width, int height);
int endScreen(int x, int y, int width, int height);
int screenWipe(int wipeno, int x, int y, int width, int height, int ticks);
} // namespace Doom
