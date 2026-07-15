#pragma once

namespace Doom
{
// Sky rendering setup. The DOOM sky is a texture wrapping around - 1024 columns to
// 360 degrees - and R_InitSkyMap just pins its vertical centre. r_sky.cpp keeps the
// vanilla name as a shim and owns skyflatnum/skytexture/skytexturemid (read across
// the renderer and the shooting code). Golden-neutral.
void initSkyMap();
} // namespace Doom
