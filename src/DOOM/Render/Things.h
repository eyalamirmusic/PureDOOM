#pragma once

#include "../r_defs.h" // column_t, Sector

namespace Doom
{
// Sprite / thing rendering; r_things.cpp keeps the vanilla R_ names as shims.
void drawMaskedColumn(column_t* column);
void sortVisSprites();
void addSprites(Sector* sec);
void initSprites(char** namelist);
void clearSprites();
void drawMasked();
} // namespace Doom
