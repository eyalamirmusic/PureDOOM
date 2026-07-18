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
void keenDie(mobj_t* mo)
{
    Doom::keenDie(*mo);
}

void look(mobj_t* actor)
{
    Doom::look(*actor);
}

void chase(mobj_t* actor)
{
    Doom::chase(*actor);
}

void faceTarget(mobj_t* actor)
{
    Doom::faceTarget(*actor);
}

void posAttack(mobj_t* actor)
{
    Doom::posAttack(*actor);
}

void sPosAttack(mobj_t* actor)
{
    Doom::sPosAttack(*actor);
}

void cPosAttack(mobj_t* actor)
{
    Doom::cPosAttack(*actor);
}

void cPosRefire(mobj_t* actor)
{
    Doom::cPosRefire(*actor);
}

void spidRefire(mobj_t* actor)
{
    Doom::spidRefire(*actor);
}

void bspiAttack(mobj_t* actor)
{
    Doom::bspiAttack(*actor);
}

void troopAttack(mobj_t* actor)
{
    Doom::troopAttack(*actor);
}

void sargAttack(mobj_t* actor)
{
    Doom::sargAttack(*actor);
}

void headAttack(mobj_t* actor)
{
    Doom::headAttack(*actor);
}

void cyberAttack(mobj_t* actor)
{
    Doom::cyberAttack(*actor);
}

void bruisAttack(mobj_t* actor)
{
    Doom::bruisAttack(*actor);
}

void skelMissile(mobj_t* actor)
{
    Doom::skelMissile(*actor);
}

void tracer(mobj_t* actor)
{
    Doom::tracer(*actor);
}

void skelWhoosh(mobj_t* actor)
{
    Doom::skelWhoosh(*actor);
}

void skelFist(mobj_t* actor)
{
    Doom::skelFist(*actor);
}

void vileChase(mobj_t* actor)
{
    Doom::vileChase(*actor);
}

void vileStart(mobj_t* actor)
{
    Doom::vileStart(*actor);
}

void startFire(mobj_t* actor)
{
    Doom::startFire(*actor);
}

void fireCrackle(mobj_t* actor)
{
    Doom::fireCrackle(*actor);
}

void fire(mobj_t* actor)
{
    Doom::fire(*actor);
}

void vileTarget(mobj_t* actor)
{
    Doom::vileTarget(*actor);
}

void vileAttack(mobj_t* actor)
{
    Doom::vileAttack(*actor);
}

void fatRaise(mobj_t* actor)
{
    Doom::fatRaise(*actor);
}

void fatAttack1(mobj_t* actor)
{
    Doom::fatAttack1(*actor);
}

void fatAttack2(mobj_t* actor)
{
    Doom::fatAttack2(*actor);
}

void fatAttack3(mobj_t* actor)
{
    Doom::fatAttack3(*actor);
}

void skullAttack(mobj_t* actor)
{
    Doom::skullAttack(*actor);
}

void painShootSkull(mobj_t* actor, angle_t angle)
{
    Doom::painShootSkull(*actor, angle);
}

void painAttack(mobj_t* actor)
{
    Doom::painAttack(*actor);
}

void painDie(mobj_t* actor)
{
    Doom::painDie(*actor);
}

void scream(mobj_t* actor)
{
    Doom::scream(*actor);
}

void xScream(mobj_t* actor)
{
    Doom::xScream(*actor);
}

void pain(mobj_t* actor)
{
    Doom::pain(*actor);
}

void fall(mobj_t* actor)
{
    Doom::fall(*actor);
}

void explode(mobj_t* thingy)
{
    Doom::explode(*thingy);
}

void bossDeath(mobj_t* mo)
{
    Doom::bossDeath(*mo);
}

void hoof(mobj_t* mo)
{
    Doom::hoof(*mo);
}

void metal(mobj_t* mo)
{
    Doom::metal(*mo);
}

void babyMetal(mobj_t* mo)
{
    Doom::babyMetal(*mo);
}

void openShotgun2(player_t* player, pspdef_t* psp)
{
    Doom::openShotgun2(player, psp);
}

void loadShotgun2(player_t* player, pspdef_t* psp)
{
    Doom::loadShotgun2(player, psp);
}

void closeShotgun2(player_t* player, pspdef_t* psp)
{
    Doom::closeShotgun2(player, psp);
}

void brainAwake(mobj_t* mo)
{
    Doom::brainAwake(*mo);
}

void brainPain(mobj_t* mo)
{
    Doom::brainPain(*mo);
}

void brainScream(mobj_t* mo)
{
    Doom::brainScream(*mo);
}

void brainExplode(mobj_t* mo)
{
    Doom::brainExplode(*mo);
}

void brainDie(mobj_t* mo)
{
    Doom::brainDie(*mo);
}

void brainSpit(mobj_t* mo)
{
    Doom::brainSpit(*mo);
}

void spawnSound(mobj_t* mo)
{
    Doom::spawnSound(*mo);
}

void spawnFly(mobj_t* mo)
{
    Doom::spawnFly(*mo);
}

void playerScream(mobj_t* mo)
{
    Doom::playerScream(*mo);
}

void weaponReady(player_t* player, pspdef_t* psp)
{
    Doom::weaponReady(*player, *psp);
}

void reFire(player_t* player, pspdef_t* psp)
{
    Doom::reFire(*player, *psp);
}

void checkReload(player_t* player, pspdef_t* psp)
{
    Doom::checkReload(*player, *psp);
}

void lower(player_t* player, pspdef_t* psp)
{
    Doom::lower(*player, *psp);
}

void raise(player_t* player, pspdef_t* psp)
{
    Doom::raise(*player, *psp);
}

void gunFlash(player_t* player, pspdef_t* psp)
{
    Doom::gunFlash(*player, *psp);
}

void punch(player_t* player, pspdef_t* psp)
{
    Doom::punch(*player, *psp);
}

void saw(player_t* player, pspdef_t* psp)
{
    Doom::saw(*player, *psp);
}

void fireMissile(player_t* player, pspdef_t* psp)
{
    Doom::fireMissile(*player, *psp);
}

void fireBFG(player_t* player, pspdef_t* psp)
{
    Doom::fireBFG(*player, *psp);
}

void firePlasma(player_t* player, pspdef_t* psp)
{
    Doom::firePlasma(*player, *psp);
}

void firePistol(player_t* player, pspdef_t* psp)
{
    Doom::firePistol(*player, *psp);
}

void fireShotgun(player_t* player, pspdef_t* psp)
{
    Doom::fireShotgun(*player, *psp);
}

void fireShotgun2(player_t* player, pspdef_t* psp)
{
    Doom::fireShotgun2(*player, *psp);
}

void fireCGun(player_t* player, pspdef_t* psp)
{
    Doom::fireCGun(*player, *psp);
}

void light0(player_t* player, pspdef_t* psp)
{
    Doom::light0(*player, *psp);
}

void light1(player_t* player, pspdef_t* psp)
{
    Doom::light1(*player, *psp);
}

void light2(player_t* player, pspdef_t* psp)
{
    Doom::light2(*player, *psp);
}

void bfgSpray(mobj_t* mo)
{
    Doom::bfgSpray(mo);
}

void bfgSound(player_t* player, pspdef_t* psp)
{
    Doom::bfgSound(*player, *psp);
}
} // namespace Doom::Actions
