// Rewritten out of vanilla r_sky into namespace Doom.
//
// Sky mapping. The sky is a texture map like any wall, wrapping around: 1024 columns
// equal 360 degrees. R_InitSkyMap pins the vertical centre whenever the view size
// changes. r_sky.cpp shims R_InitSkyMap and owns the sky globals.

#include "../m_fixed.h" // FRACUNIT

#include "Sky.h"

// skytexturemid is a global (declared in r_sky.h, read across the renderer); it is
// defined in the r_sky.cpp shim. Declared at global scope so initSkyMap writes that
// one, not a Doom:: copy.
extern int skytexturemid;

namespace Doom
{
void initSkyMap()
{
    skytexturemid = 100 * FRACUNIT;
}
} // namespace Doom
