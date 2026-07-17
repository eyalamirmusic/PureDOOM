// The virtual tick() bodies for the polymorphic thinker types.
//
// Each forwards to the vanilla-named thinker routine (P_MobjThinker for a mobj,
// the T_* functions for the specials) - the same routine P_RunThinkers used to
// reach through the old `thinker_t.function` union. They are defined together, at
// global scope, because the types they belong to (mobj_t and the p_spec.h
// specials) live in the global namespace, and out of line because those routines
// take the concrete type and so are only declared after it.

#include "../p_local.h" // P_MobjThinker, mobj_t
#include "../p_spec.h" // the specials and their T_* thinkers

// T_FireFlicker has no shared-header declaration (p_saveg never serialises a
// fireflicker, so nothing outside Lights.cpp needed it); declare it to reach it.
void T_FireFlicker(fireflicker_t* flick);

void mobj_t::tick()
{
    P_MobjThinker(this);
}
void vldoor_t::tick()
{
    T_VerticalDoor(this);
}
void ceiling_t::tick()
{
    T_MoveCeiling(this);
}
void floormove_t::tick()
{
    T_MoveFloor(this);
}
void plat_t::tick()
{
    T_PlatRaise(this);
}
void fireflicker_t::tick()
{
    T_FireFlicker(this);
}
void lightflash_t::tick()
{
    T_LightFlash(this);
}
void strobe_t::tick()
{
    T_StrobeFlash(this);
}
void glow_t::tick()
{
    T_Glow(this);
}
