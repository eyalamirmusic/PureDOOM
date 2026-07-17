#pragma once

#include "../d_player.h" // player_t

namespace Doom
{
// One tic of a player: weapon change, movement (via tryMove/thrust), view bob,
// specials, powerup countdowns and the powerup colormap. p_user.cpp keeps the
// vanilla name P_PlayerThink as a shim; p_tick calls it. Golden-neutral and covered
// by every demo (the demos are recorded player input).
void playerThink(player_t& player);
} // namespace Doom
