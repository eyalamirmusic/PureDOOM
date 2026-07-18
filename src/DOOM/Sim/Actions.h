#pragma once

#include "../d_player.h" // Player, PspDef
#include "../p_mobj.h" // Mobj, angle_t

namespace Doom::Actions
{
// Thin adapters from the vanilla A_* state-action signature (pointer parameters,
// because Sim/Info.cpp's states[] table stores them by address and every entry
// needs the same shape) onto the namespaced implementation (which takes a
// reference). p_enemy.cpp and p_pspr.cpp used to carry these as global A_*
// functions for exactly that reason; they moved here, unprefixed, once nothing
// outside Sim/Info.cpp needed the global names any more.
void keenDie(Mobj* mo);
void look(Mobj* actor);
void chase(Mobj* actor);
void faceTarget(Mobj* actor);
void posAttack(Mobj* actor);
void sPosAttack(Mobj* actor);
void cPosAttack(Mobj* actor);
void cPosRefire(Mobj* actor);
void spidRefire(Mobj* actor);
void bspiAttack(Mobj* actor);
void troopAttack(Mobj* actor);
void sargAttack(Mobj* actor);
void headAttack(Mobj* actor);
void cyberAttack(Mobj* actor);
void bruisAttack(Mobj* actor);
void skelMissile(Mobj* actor);
void tracer(Mobj* actor);
void skelWhoosh(Mobj* actor);
void skelFist(Mobj* actor);
void vileChase(Mobj* actor);
void vileStart(Mobj* actor);
void startFire(Mobj* actor);
void fireCrackle(Mobj* actor);
void fire(Mobj* actor);
void vileTarget(Mobj* actor);
void vileAttack(Mobj* actor);
void fatRaise(Mobj* actor);
void fatAttack1(Mobj* actor);
void fatAttack2(Mobj* actor);
void fatAttack3(Mobj* actor);
void skullAttack(Mobj* actor);
void painShootSkull(Mobj* actor, angle_t angle);
void painAttack(Mobj* actor);
void painDie(Mobj* actor);
void scream(Mobj* actor);
void xScream(Mobj* actor);
void pain(Mobj* actor);
void fall(Mobj* actor);
void explode(Mobj* thingy);
void bossDeath(Mobj* mo);
void hoof(Mobj* mo);
void metal(Mobj* mo);
void babyMetal(Mobj* mo);
void openShotgun2(Player* player, PspDef* psp);
void loadShotgun2(Player* player, PspDef* psp);
void closeShotgun2(Player* player, PspDef* psp);
void brainAwake(Mobj* mo);
void brainPain(Mobj* mo);
void brainScream(Mobj* mo);
void brainExplode(Mobj* mo);
void brainDie(Mobj* mo);
void brainSpit(Mobj* mo);
void spawnSound(Mobj* mo);
void spawnFly(Mobj* mo);
void playerScream(Mobj* mo);

void weaponReady(Player* player, PspDef* psp);
void reFire(Player* player, PspDef* psp);
void checkReload(Player* player, PspDef* psp);
void lower(Player* player, PspDef* psp);
void raise(Player* player, PspDef* psp);
void gunFlash(Player* player, PspDef* psp);
void punch(Player* player, PspDef* psp);
void saw(Player* player, PspDef* psp);
void fireMissile(Player* player, PspDef* psp);
void fireBFG(Player* player, PspDef* psp);
void firePlasma(Player* player, PspDef* psp);
void firePistol(Player* player, PspDef* psp);
void fireShotgun(Player* player, PspDef* psp);
void fireShotgun2(Player* player, PspDef* psp);
void fireCGun(Player* player, PspDef* psp);
void light0(Player* player, PspDef* psp);
void light1(Player* player, PspDef* psp);
void light2(Player* player, PspDef* psp);
void bfgSpray(Mobj* mo);
void bfgSound(Player* player, PspDef* psp);
} // namespace Doom::Actions
