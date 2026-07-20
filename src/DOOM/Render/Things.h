#pragma once

#include "../Sim/MapTypes.h"
#include "RenderTypes.h" // Column, Sector

#include <span>
#include <string_view>

// The vissprite pool's size. Was r_things.h.

namespace Doom
{
// Sprite / thing rendering; r_things.cpp keeps the vanilla R_ names as shims.
void drawMaskedColumn(Column* column);
void sortVisSprites();
void addSprites(Sector* sec);
void initSprites(std::span<const std::string_view> namelist);
void clearSprites();
void drawMasked();
} // namespace Doom
