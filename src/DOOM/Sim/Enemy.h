#pragma once

#include "../Game/PlayerTypes.h" // Player, PspDef
#include "MobjTypes.h" // Mobj, Sector, Doom::Angle

namespace Doom
{
// Enemy AI. Each monster's per-state behaviour - vanilla's A_* codepointers -
// is now methods: chase/look, the attacks and the deaths are Mobj methods, and
// the super-shotgun trio are Player methods; Sim/Info.cpp's state table installs
// each as &Mobj::name / &Player::name and setMobjState / setPsprite invoke them.
// See Thinkers/Mobj.h and Game/PlayerTypes.h for the declarations, Enemy.cpp for
// the bodies.
//
// What remains a free function is the one thing the rest of the sim calls
// directly rather than through a state table: the noise alert a firing weapon
// raises to wake nearby monsters (Weapon.cpp calls it). Covered by the demos and
// golden-neutral.
void noiseAlert(Mobj& target, Mobj& emmiter);
} // namespace Doom
