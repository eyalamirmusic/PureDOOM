#pragma once

#include "../Game/PlayerTypes.h" // Player, PspDef
#include "MobjTypes.h" // Mobj

namespace Doom
{
// Weapon handling. Each weapon's per-state behaviour - vanilla's A_* weapon
// codepointers (weaponReady, the fire routines, the light frames, ...) - is now
// Player methods, plus bfgSpray, which is a Mobj method (it runs on the BFG ball,
// not the player); Sim/Info.cpp's state table installs each as &Player::name /
// &Mobj::name and setPsprite / setMobjState invoke them. See Game/PlayerTypes.h
// and Thinkers/Mobj.h for the declarations, Weapon.cpp for the bodies.
//
// What remains free here is the psprite plumbing the player think loop and the
// mobj/interaction code drive directly. Covered by the demos (the attract demos
// fire every weapon) and golden-neutral.
void setupPsprites(Player& player);
void movePsprites(Player& player);
void dropWeapon(Player& player);
} // namespace Doom
