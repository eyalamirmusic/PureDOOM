#pragma once

#include "../Game/PlayerTypes.h" // Player, PspDef
#include "MobjTypes.h" // Mobj, Sector, angle_t

namespace Doom
{
// Enemy AI: the action functions info.cpp's state table drives, plus the noise
// alert Doom::noiseAlert weapons trigger. p_enemy.cpp keeps the vanilla names as shims.
// Covered by the demos (a level's worth of monsters chasing, attacking, dying) and
// golden-neutral.
void keenDie(Mobj& mo);
void look(Mobj& actor);
void chase(Mobj& actor);
void faceTarget(Mobj& actor);
void posAttack(Mobj& actor);
void sPosAttack(Mobj& actor);
void cPosAttack(Mobj& actor);
void cPosRefire(Mobj& actor);
void spidRefire(Mobj& actor);
void bspiAttack(Mobj& actor);
void troopAttack(Mobj& actor);
void sargAttack(Mobj& actor);
void headAttack(Mobj& actor);
void cyberAttack(Mobj& actor);
void bruisAttack(Mobj& actor);
void skelMissile(Mobj& actor);
void tracer(Mobj& actor);
void skelWhoosh(Mobj& actor);
void skelFist(Mobj& actor);
void vileChase(Mobj& actor);
void vileStart(Mobj& actor);
void startFire(Mobj& actor);
void fireCrackle(Mobj& actor);
void fire(Mobj& actor);
void vileTarget(Mobj& actor);
void vileAttack(Mobj& actor);
void fatRaise(Mobj& actor);
void fatAttack1(Mobj& actor);
void fatAttack2(Mobj& actor);
void fatAttack3(Mobj& actor);
void skullAttack(Mobj& actor);
void painShootSkull(Mobj& actor, angle_t angle);
void painAttack(Mobj& actor);
void painDie(Mobj& actor);
void scream(Mobj& actor);
void xScream(Mobj& actor);
void pain(Mobj& actor);
void fall(Mobj& actor);
void explode(Mobj& thingy);
void bossDeath(Mobj& mo);
void hoof(Mobj& mo);
void metal(Mobj& mo);
void babyMetal(Mobj& mo);
void openShotgun2(Player* player, PspDef* psp);
void loadShotgun2(Player* player, PspDef* psp);
void closeShotgun2(Player* player, PspDef* psp);
void brainAwake(Mobj& mo);
void brainPain(Mobj& mo);
void brainScream(Mobj& mo);
void brainExplode(Mobj& mo);
void brainDie(Mobj& mo);
void brainSpit(Mobj& mo);
void spawnSound(Mobj& mo);
void spawnFly(Mobj& mo);
void playerScream(Mobj& mo);
void noiseAlert(Mobj* target, Mobj& emmiter);
} // namespace Doom
