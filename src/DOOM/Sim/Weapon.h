#pragma once

#include "../d_player.h" // player_t, pspdef_t
#include "../p_mobj.h" // mobj_t

namespace Doom
{
// The weapon action functions and psprite handling. p_pspr.cpp keeps the vanilla
// A_*/P_* names as shims: info.cpp's state table calls the A_* by their global
// address, and p_user/p_mobj/g_game call the P_* ones. Covered by the demos (the
// attract demos fire every weapon) and golden-neutral.
void weaponReady(player_t* player, pspdef_t* psp);
void reFire(player_t* player, pspdef_t* psp);
void checkReload(player_t* player, pspdef_t* psp);
void lower(player_t* player, pspdef_t* psp);
void raise(player_t* player, pspdef_t* psp);
void gunFlash(player_t* player, pspdef_t* psp);
void punch(player_t* player, pspdef_t* psp);
void saw(player_t* player, pspdef_t* psp);
void fireMissile(player_t* player, pspdef_t* psp);
void fireBFG(player_t* player, pspdef_t* psp);
void firePlasma(player_t* player, pspdef_t* psp);
void firePistol(player_t* player, pspdef_t* psp);
void fireShotgun(player_t* player, pspdef_t* psp);
void fireShotgun2(player_t* player, pspdef_t* psp);
void fireCGun(player_t* player, pspdef_t* psp);
void light0(player_t* player, pspdef_t* psp);
void light1(player_t* player, pspdef_t* psp);
void light2(player_t* player, pspdef_t* psp);
void bfgSpray(mobj_t* mo);
void bfgSound(player_t* player, pspdef_t* psp);
void setupPsprites(player_t* player);
void movePsprites(player_t* player);
void dropWeapon(player_t* player);
} // namespace Doom
