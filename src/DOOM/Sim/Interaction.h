#pragma once

#include "../Game/PlayerTypes.h" // Player
#include "MobjTypes.h" // Mobj

namespace Doom
{
// Interactions: a player touching a pickup, one thing damaging another, and death.
// The vanilla names (Doom::touchSpecialThing, Doom::damageMobj, Doom::givePower) forward here
// from p_inter.cpp; the give-* helpers are internal to Interaction.cpp. Covered by
// the demos' combat (damage, death, thrust) and golden-neutral.

// Player `toucher` picks up `special` (an MF_SPECIAL thing), applying its effect and
// removing it. No-op if out of reach or already at the relevant maximum.
void touchSpecialThing(Mobj* special, Mobj* toucher);

// Apply `damage` to target from inflictor/source (either may be null for
// environmental damage), including thrust, armor, pain and death.
void damageMobj(Mobj* target, Mobj* inflictor, Mobj* source, int damage);

// Grant a powerup to the player; returns false if it had no effect.
doom_boolean givePower(Player* player, int power);
} // namespace Doom
