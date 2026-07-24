#pragma once

#include "../Game/PlayerTypes.h" // Player

// One tic of a player - weapon change, movement, view bob, specials, powerup
// countdowns - is now Player::think() (vanilla's P_PlayerThink), declared on the
// struct in Game/PlayerTypes.h with its helpers (thrust, calcHeight, movePlayer,
// deathThink); p_tick calls it. Bodies in Sim/Player.cpp. This header remains as
// the include the sim reaches for the player type and its per-tic entry point.
