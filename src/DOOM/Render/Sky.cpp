// Rewritten out of vanilla r_sky into namespace Doom.
//
// Sky mapping. The sky is a texture map like any wall, wrapping around: 1024 columns
// equal 360 degrees. Doom::initSkyMap pins the vertical centre whenever the view size
// changes. r_sky.cpp shims Doom::initSkyMap and owns the sky globals.

#include "../Math/FixedPoint.h" // FRACUNIT

#include "../Game/SkyState.h"
#include "Sky.h"

namespace Doom
{
void initSkyMap()
{
    skyState().skytexturemid = 100 * FRACUNIT;
}
} // namespace Doom
