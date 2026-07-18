// The virtual tick() bodies for the polymorphic thinker types.
//
// Each forwards to the namespaced thinker routine (Doom::mobjThinker for a mobj,
// the Doom::* functions for the specials) - the same routine Doom::runThinkers used
// to reach through the old `thinker_t.function` union. They are defined together, at
// global scope, because the types they belong to (Doom::Mobj and the p_spec.h
// specials) live in the global namespace, and out of line because those routines
// take the concrete type and so are only declared after it.

#include "../p_local.h" // Doom::mobjThinker, Mobj
#include "../p_spec.h" // the specials
#include "Ceilings.h" // Doom::moveCeiling
#include "Doors.h" // Doom::verticalDoor
#include "Floors.h" // Doom::moveFloor
#include "Lights.h" // Doom::fireFlicker, lightFlash, strobeFlash, glow
#include "Mobj.h"
#include "Plats.h" // Doom::platRaise

void Doom::Mobj::tick()
{
    Doom::mobjThinker(this);
}
void Doom::Door::tick()
{
    Doom::verticalDoor(*this);
}
void Doom::Ceiling::tick()
{
    Doom::moveCeiling(*this);
}
void Doom::FloorMove::tick()
{
    Doom::moveFloor(*this);
}
void Doom::Plat::tick()
{
    Doom::platRaise(*this);
}
void Doom::FireFlicker::tick()
{
    Doom::fireFlicker(*this);
}
void Doom::LightFlash::tick()
{
    Doom::lightFlash(*this);
}
void Doom::Strobe::tick()
{
    Doom::strobeFlash(*this);
}
void Doom::Glow::tick()
{
    Doom::glow(*this);
}
