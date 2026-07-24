#pragma once

#include "../Game/PlayerTypes.h" // Player, PspDef
#include "MobjTypes.h" // Mobj

// Weapon handling. Every weapon routine is a method now: the A_* codepointers are
// Player methods (plus bfgSpray, a Mobj method), and the psprite plumbing
// (setupPsprites, movePsprites, bringUpWeapon, checkAmmo, fireWeapon, dropWeapon,
// setPsprite) and the hitscan helpers (Mobj::computeBulletSlope / gunShot) are too.
// See Game/PlayerTypes.h and Thinkers/Mobj.h for the declarations, Weapon.cpp for
// the bodies. This header remains as the include the sim reaches for that surface.
