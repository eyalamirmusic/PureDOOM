#pragma once

#include "../r_defs.h" // column_t, sector_t

namespace Doom
{
// Sprite / thing rendering; r_things.cpp keeps the vanilla R_ names as shims.
void drawMaskedColumn(column_t* column);
void sortVisSprites();
void addSprites(sector_t* sec);
void initSprites(char** namelist);
void clearSprites();
void drawMasked();
} // namespace Doom
