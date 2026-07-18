// Rewritten out of vanilla r_sky into namespace Doom.
//
// Sky mapping. The sky is a texture map like any wall, wrapping around: 1024 columns
// equal 360 degrees. Doom::initSkyMap pins the vertical centre whenever the view size
// changes. r_sky.cpp shims Doom::initSkyMap and owns the sky globals.

#include "../m_fixed.h" // FRACUNIT

#include "Sky.h"

// skytexturemid is a Doom::SkyState member now (Engine), a reference exported by the
// r_sky.cpp shim (declared in r_sky.h). Declared at global scope so initSkyMap writes that
// one, not a Doom:: copy - and as int& to match the definition, or a write through a plain
// int here would clobber the low half of the reference's pointer.
extern int& skytexturemid;

namespace Doom
{
void initSkyMap()
{
    skytexturemid = 100 * FRACUNIT;
}
} // namespace Doom
