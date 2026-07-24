#pragma once

#include "../Game/PlayerTypes.h" // Player
#include "MobjTypes.h" // Mobj

namespace Doom
{
// Interactions. One thing damaging another is Mobj::damage (declared on the struct in
// Thinkers/Mobj.h), and the pickup effects - the give-* helpers and givePower - are
// Player methods (Game/PlayerTypes.h); bodies in Interaction.cpp. Covered by the
// demos' combat (damage, death, thrust) and golden-neutral.
//
// What stays a free function is touchSpecialThing: it takes two mobjs (the special
// picked up and the toucher) with no single owner, so it reads as a free function
// rather than a method on either. killMobj is the same shape (a source and a target)
// and stays file-local to Interaction.cpp.

// Player `toucher` picks up `special` (a MobjFlag::Special thing), applying its effect
// and removing it. No-op if out of reach or already at the relevant maximum.
void touchSpecialThing(Mobj& special, Mobj& toucher);
} // namespace Doom
