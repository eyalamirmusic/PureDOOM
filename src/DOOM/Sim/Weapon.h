#pragma once

#include "../Game/PlayerTypes.h" // Player, PspDef
#include "MobjTypes.h" // Mobj

namespace Doom
{
// The weapon action functions and psprite handling. p_pspr.cpp keeps the vanilla
// A_*/P_* names as shims: info.cpp's state table calls the A_* by their global
// address, and p_user/p_mobj/g_game call the P_* ones. Covered by the demos (the
// attract demos fire every weapon) and golden-neutral.
void weaponReady(Player& player, PspDef& psp);
void reFire(Player& player, PspDef& psp);
void checkReload(Player& player, PspDef& psp);
void lower(Player& player, PspDef& psp);
void raise(Player& player, PspDef& psp);
void gunFlash(Player& player, PspDef& psp);
void punch(Player& player, PspDef& psp);
void saw(Player& player, PspDef& psp);
void fireMissile(Player& player, PspDef& psp);
void fireBFG(Player& player, PspDef& psp);
void firePlasma(Player& player, PspDef& psp);
void firePistol(Player& player, PspDef& psp);
void fireShotgun(Player& player, PspDef& psp);
void fireShotgun2(Player& player, PspDef& psp);
void fireCGun(Player& player, PspDef& psp);
void light0(Player& player, PspDef& psp);
void light1(Player& player, PspDef& psp);
void light2(Player& player, PspDef& psp);
void bfgSpray(Mobj* mo);
void bfgSound(Player& player, PspDef& psp);
void setupPsprites(Player& player);
void movePsprites(Player& player);
void dropWeapon(Player& player);
} // namespace Doom
