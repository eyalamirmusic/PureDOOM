#pragma once

#include "../d_player.h" // player_t, pspdef_t
#include "../p_mobj.h" // mobj_t, angle_t

namespace Doom::Actions
{
// Thin adapters from the vanilla A_* state-action signature (pointer parameters,
// because Sim/Info.cpp's states[] table stores them by address and every entry
// needs the same shape) onto the namespaced implementation (which takes a
// reference). p_enemy.cpp and p_pspr.cpp used to carry these as global A_*
// functions for exactly that reason; they moved here, unprefixed, once nothing
// outside Sim/Info.cpp needed the global names any more.
void keenDie(mobj_t* mo);
void look(mobj_t* actor);
void chase(mobj_t* actor);
void faceTarget(mobj_t* actor);
void posAttack(mobj_t* actor);
void sPosAttack(mobj_t* actor);
void cPosAttack(mobj_t* actor);
void cPosRefire(mobj_t* actor);
void spidRefire(mobj_t* actor);
void bspiAttack(mobj_t* actor);
void troopAttack(mobj_t* actor);
void sargAttack(mobj_t* actor);
void headAttack(mobj_t* actor);
void cyberAttack(mobj_t* actor);
void bruisAttack(mobj_t* actor);
void skelMissile(mobj_t* actor);
void tracer(mobj_t* actor);
void skelWhoosh(mobj_t* actor);
void skelFist(mobj_t* actor);
void vileChase(mobj_t* actor);
void vileStart(mobj_t* actor);
void startFire(mobj_t* actor);
void fireCrackle(mobj_t* actor);
void fire(mobj_t* actor);
void vileTarget(mobj_t* actor);
void vileAttack(mobj_t* actor);
void fatRaise(mobj_t* actor);
void fatAttack1(mobj_t* actor);
void fatAttack2(mobj_t* actor);
void fatAttack3(mobj_t* actor);
void skullAttack(mobj_t* actor);
void painShootSkull(mobj_t* actor, angle_t angle);
void painAttack(mobj_t* actor);
void painDie(mobj_t* actor);
void scream(mobj_t* actor);
void xScream(mobj_t* actor);
void pain(mobj_t* actor);
void fall(mobj_t* actor);
void explode(mobj_t* thingy);
void bossDeath(mobj_t* mo);
void hoof(mobj_t* mo);
void metal(mobj_t* mo);
void babyMetal(mobj_t* mo);
void openShotgun2(player_t* player, pspdef_t* psp);
void loadShotgun2(player_t* player, pspdef_t* psp);
void closeShotgun2(player_t* player, pspdef_t* psp);
void brainAwake(mobj_t* mo);
void brainPain(mobj_t* mo);
void brainScream(mobj_t* mo);
void brainExplode(mobj_t* mo);
void brainDie(mobj_t* mo);
void brainSpit(mobj_t* mo);
void spawnSound(mobj_t* mo);
void spawnFly(mobj_t* mo);
void playerScream(mobj_t* mo);

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
} // namespace Doom::Actions
