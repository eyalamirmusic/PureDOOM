#pragma once

#include "../d_player.h" // player_t
#include "../p_mobj.h" // mobj_t

namespace Doom
{
// Interactions: a player touching a pickup, one thing damaging another, and death.
// The vanilla names (P_TouchSpecialThing, P_DamageMobj, P_GivePower) forward here
// from p_inter.cpp; the give-* helpers are internal to Interaction.cpp. Covered by
// the demos' combat (damage, death, thrust) and golden-neutral.

// Player `toucher` picks up `special` (an MF_SPECIAL thing), applying its effect and
// removing it. No-op if out of reach or already at the relevant maximum.
void touchSpecialThing(mobj_t* special, mobj_t* toucher);

// Apply `damage` to target from inflictor/source (either may be null for
// environmental damage), including thrust, armor, pain and death.
void damageMobj(mobj_t* target, mobj_t* inflictor, mobj_t* source, int damage);

// Grant a powerup to the player; returns false if it had no effect.
doom_boolean givePower(player_t* player, int power);
} // namespace Doom
