#pragma once

#include "../r_defs.h" // Column, Sector

// The vissprite pool's size. Was r_things.h.
#define MAXVISSPRITES 128

namespace Doom
{
// Sprite / thing rendering; r_things.cpp keeps the vanilla R_ names as shims.
void drawMaskedColumn(Column* column);
void sortVisSprites();
void addSprites(Sector* sec);
void initSprites(char** namelist);
void clearSprites();
void drawMasked();
} // namespace Doom
