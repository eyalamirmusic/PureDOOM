#pragma once

#include "../d_player.h" // player_t, pspdef_t
#include "../p_mobj.h" // mobj_t, sector_t, angle_t

namespace Doom
{
// Enemy AI: the action functions info.cpp's state table drives, plus the noise
// alert Doom::noiseAlert weapons trigger. p_enemy.cpp keeps the vanilla names as shims.
// Covered by the demos (a level's worth of monsters chasing, attacking, dying) and
// golden-neutral.
void keenDie(mobj_t& mo);
void look(mobj_t& actor);
void chase(mobj_t& actor);
void faceTarget(mobj_t& actor);
void posAttack(mobj_t& actor);
void sPosAttack(mobj_t& actor);
void cPosAttack(mobj_t& actor);
void cPosRefire(mobj_t& actor);
void spidRefire(mobj_t& actor);
void bspiAttack(mobj_t& actor);
void troopAttack(mobj_t& actor);
void sargAttack(mobj_t& actor);
void headAttack(mobj_t& actor);
void cyberAttack(mobj_t& actor);
void bruisAttack(mobj_t& actor);
void skelMissile(mobj_t& actor);
void tracer(mobj_t& actor);
void skelWhoosh(mobj_t& actor);
void skelFist(mobj_t& actor);
void vileChase(mobj_t& actor);
void vileStart(mobj_t& actor);
void startFire(mobj_t& actor);
void fireCrackle(mobj_t& actor);
void fire(mobj_t& actor);
void vileTarget(mobj_t& actor);
void vileAttack(mobj_t& actor);
void fatRaise(mobj_t& actor);
void fatAttack1(mobj_t& actor);
void fatAttack2(mobj_t& actor);
void fatAttack3(mobj_t& actor);
void skullAttack(mobj_t& actor);
void painShootSkull(mobj_t& actor, angle_t angle);
void painAttack(mobj_t& actor);
void painDie(mobj_t& actor);
void scream(mobj_t& actor);
void xScream(mobj_t& actor);
void pain(mobj_t& actor);
void fall(mobj_t& actor);
void explode(mobj_t& thingy);
void bossDeath(mobj_t& mo);
void hoof(mobj_t& mo);
void metal(mobj_t& mo);
void babyMetal(mobj_t& mo);
void openShotgun2(player_t* player, pspdef_t* psp);
void loadShotgun2(player_t* player, pspdef_t* psp);
void closeShotgun2(player_t* player, pspdef_t* psp);
void brainAwake(mobj_t& mo);
void brainPain(mobj_t& mo);
void brainScream(mobj_t& mo);
void brainExplode(mobj_t& mo);
void brainDie(mobj_t& mo);
void brainSpit(mobj_t& mo);
void spawnSound(mobj_t& mo);
void spawnFly(mobj_t& mo);
void playerScream(mobj_t& mo);
void noiseAlert(mobj_t* target, mobj_t& emmiter);
} // namespace Doom
