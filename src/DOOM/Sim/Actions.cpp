// The A_* state-action adapters, moved out of p_enemy.cpp and p_pspr.cpp and
// unprefixed. Each is a thin forward from the vanilla pointer signature
// Sim/Info.cpp's states[] table requires onto the namespaced implementation,
// which takes a reference. Bodies are unchanged from the vanilla shims they
// replace - see REFACTOR.md's idiom-modernization pass for why the reference
// form exists at all.

#include "Actions.h"

#include "Enemy.h"
#include "Weapon.h"

namespace Doom::Actions
{
void keenDie(Mobj* mo)
{
    Doom::keenDie(*mo);
}

void look(Mobj* actor)
{
    Doom::look(*actor);
}

void chase(Mobj* actor)
{
    Doom::chase(*actor);
}

void faceTarget(Mobj* actor)
{
    Doom::faceTarget(*actor);
}

void posAttack(Mobj* actor)
{
    Doom::posAttack(*actor);
}

void sPosAttack(Mobj* actor)
{
    Doom::sPosAttack(*actor);
}

void cPosAttack(Mobj* actor)
{
    Doom::cPosAttack(*actor);
}

void cPosRefire(Mobj* actor)
{
    Doom::cPosRefire(*actor);
}

void spidRefire(Mobj* actor)
{
    Doom::spidRefire(*actor);
}

void bspiAttack(Mobj* actor)
{
    Doom::bspiAttack(*actor);
}

void troopAttack(Mobj* actor)
{
    Doom::troopAttack(*actor);
}

void sargAttack(Mobj* actor)
{
    Doom::sargAttack(*actor);
}

void headAttack(Mobj* actor)
{
    Doom::headAttack(*actor);
}

void cyberAttack(Mobj* actor)
{
    Doom::cyberAttack(*actor);
}

void bruisAttack(Mobj* actor)
{
    Doom::bruisAttack(*actor);
}

void skelMissile(Mobj* actor)
{
    Doom::skelMissile(*actor);
}

void tracer(Mobj* actor)
{
    Doom::tracer(*actor);
}

void skelWhoosh(Mobj* actor)
{
    Doom::skelWhoosh(*actor);
}

void skelFist(Mobj* actor)
{
    Doom::skelFist(*actor);
}

void vileChase(Mobj* actor)
{
    Doom::vileChase(*actor);
}

void vileStart(Mobj* actor)
{
    Doom::vileStart(*actor);
}

void startFire(Mobj* actor)
{
    Doom::startFire(*actor);
}

void fireCrackle(Mobj* actor)
{
    Doom::fireCrackle(*actor);
}

void fire(Mobj* actor)
{
    Doom::fire(*actor);
}

void vileTarget(Mobj* actor)
{
    Doom::vileTarget(*actor);
}

void vileAttack(Mobj* actor)
{
    Doom::vileAttack(*actor);
}

void fatRaise(Mobj* actor)
{
    Doom::fatRaise(*actor);
}

void fatAttack1(Mobj* actor)
{
    Doom::fatAttack1(*actor);
}

void fatAttack2(Mobj* actor)
{
    Doom::fatAttack2(*actor);
}

void fatAttack3(Mobj* actor)
{
    Doom::fatAttack3(*actor);
}

void skullAttack(Mobj* actor)
{
    Doom::skullAttack(*actor);
}

void painShootSkull(Mobj* actor, angle_t angle)
{
    Doom::painShootSkull(*actor, angle);
}

void painAttack(Mobj* actor)
{
    Doom::painAttack(*actor);
}

void painDie(Mobj* actor)
{
    Doom::painDie(*actor);
}

void scream(Mobj* actor)
{
    Doom::scream(*actor);
}

void xScream(Mobj* actor)
{
    Doom::xScream(*actor);
}

void pain(Mobj* actor)
{
    Doom::pain(*actor);
}

void fall(Mobj* actor)
{
    Doom::fall(*actor);
}

void explode(Mobj* thingy)
{
    Doom::explode(*thingy);
}

void bossDeath(Mobj* mo)
{
    Doom::bossDeath(*mo);
}

void hoof(Mobj* mo)
{
    Doom::hoof(*mo);
}

void metal(Mobj* mo)
{
    Doom::metal(*mo);
}

void babyMetal(Mobj* mo)
{
    Doom::babyMetal(*mo);
}

void openShotgun2(Player* player, PspDef* psp)
{
    Doom::openShotgun2(*player, *psp);
}

void loadShotgun2(Player* player, PspDef* psp)
{
    Doom::loadShotgun2(*player, *psp);
}

void closeShotgun2(Player* player, PspDef* psp)
{
    Doom::closeShotgun2(*player, *psp);
}

void brainAwake(Mobj* mo)
{
    Doom::brainAwake(*mo);
}

void brainPain(Mobj* mo)
{
    Doom::brainPain(*mo);
}

void brainScream(Mobj* mo)
{
    Doom::brainScream(*mo);
}

void brainExplode(Mobj* mo)
{
    Doom::brainExplode(*mo);
}

void brainDie(Mobj* mo)
{
    Doom::brainDie(*mo);
}

void brainSpit(Mobj* mo)
{
    Doom::brainSpit(*mo);
}

void spawnSound(Mobj* mo)
{
    Doom::spawnSound(*mo);
}

void spawnFly(Mobj* mo)
{
    Doom::spawnFly(*mo);
}

void playerScream(Mobj* mo)
{
    Doom::playerScream(*mo);
}

void weaponReady(Player* player, PspDef* psp)
{
    Doom::weaponReady(*player, *psp);
}

void reFire(Player* player, PspDef* psp)
{
    Doom::reFire(*player, *psp);
}

void checkReload(Player* player, PspDef* psp)
{
    Doom::checkReload(*player, *psp);
}

void lower(Player* player, PspDef* psp)
{
    Doom::lower(*player, *psp);
}

void raise(Player* player, PspDef* psp)
{
    Doom::raise(*player, *psp);
}

void gunFlash(Player* player, PspDef* psp)
{
    Doom::gunFlash(*player, *psp);
}

void punch(Player* player, PspDef* psp)
{
    Doom::punch(*player, *psp);
}

void saw(Player* player, PspDef* psp)
{
    Doom::saw(*player, *psp);
}

void fireMissile(Player* player, PspDef* psp)
{
    Doom::fireMissile(*player, *psp);
}

void fireBFG(Player* player, PspDef* psp)
{
    Doom::fireBFG(*player, *psp);
}

void firePlasma(Player* player, PspDef* psp)
{
    Doom::firePlasma(*player, *psp);
}

void firePistol(Player* player, PspDef* psp)
{
    Doom::firePistol(*player, *psp);
}

void fireShotgun(Player* player, PspDef* psp)
{
    Doom::fireShotgun(*player, *psp);
}

void fireShotgun2(Player* player, PspDef* psp)
{
    Doom::fireShotgun2(*player, *psp);
}

void fireCGun(Player* player, PspDef* psp)
{
    Doom::fireCGun(*player, *psp);
}

void light0(Player* player, PspDef* psp)
{
    Doom::light0(*player, *psp);
}

void light1(Player* player, PspDef* psp)
{
    Doom::light1(*player, *psp);
}

void light2(Player* player, PspDef* psp)
{
    Doom::light2(*player, *psp);
}

void bfgSpray(Mobj* mo)
{
    Doom::bfgSpray(*mo);
}

void bfgSound(Player* player, PspDef* psp)
{
    Doom::bfgSound(*player, *psp);
}
} // namespace Doom::Actions
