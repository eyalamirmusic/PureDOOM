#pragma once

// The engine's container vocabulary, in one place.
//
// This mirrors eacp's own <eacp/Core/Utils/Containers.h>, which re-exports the
// same EA types into `namespace eacp` for exactly this reason: so application
// code writes the unqualified name. Include this rather than the individual
// ea_data_structures headers, and write `Vector<T>&` rather than `EA::Vector<T>&`
// in a signature.
//
// It sits at the top level beside doomtype.h because it is the same kind of
// thing - a foundation every one of the eight subdirectories depends on,
// Math/ included. It is not a vanilla file; nothing here descends from 1993.
//
// One deliberate exception: DOOM.h, the public interface an embedder includes,
// stays standard-library-only and spells its argument vector `std::vector`. No
// eacp type appears in it, and that is a constraint worth keeping - an embedder
// linking this engine should not have to acquire eacp's containers to call it.

#include <ea_data_structures/Pointers/OwningPointer.h>
#include <ea_data_structures/Structures/Array.h>
#include <ea_data_structures/Structures/OwnedVector.h>
#include <ea_data_structures/Structures/Vector.h>

namespace Doom
{
using EA::Array;
using EA::makeOwned;
using EA::OwnedVector;
using EA::OwningPointer;
using EA::Vector;
} // namespace Doom
