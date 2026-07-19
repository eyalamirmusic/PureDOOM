#pragma once

namespace Doom
{
// The flat that means "draw the sky here", and the shift turning a view angle into
// a sky texture column. Were r_sky.h.
constexpr const char* SKYFLATNAME = "F_SKY1";
constexpr int ANGLETOSKYSHIFT = 22;

// Sky rendering setup. The DOOM sky is a texture wrapping around - 1024 columns to
// 360 degrees - and Doom::initSkyMap just pins its vertical centre. r_sky.cpp keeps the
// vanilla name as a shim and owns skyflatnum/skytexture/skytexturemid (read across
// the renderer and the shooting code). Golden-neutral.
void initSkyMap();
} // namespace Doom
