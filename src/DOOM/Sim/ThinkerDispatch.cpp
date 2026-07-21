// The virtual tick() bodies for the polymorphic thinker types.
//
// Each forwards to the namespaced thinker routine (Doom::mobjThinker for a mobj,
// the Doom::* functions for the specials) - the same routine Doom::runThinkers used
// to reach through the old `Doom::Thinker.function` union. They are defined together, at
// global scope, because the types they belong to (Doom::Mobj and the p_spec.h
// specials) live in the global namespace, and out of line because those routines
// take the concrete type and so are only declared after it.

#include "SimDefs.h" // Doom::mobjThinker, Mobj
#include "SpecialTypes.h" // the specials
#include "Ceilings.h" // Doom::moveCeiling
#include "Doors.h" // Doom::verticalDoor
#include "Floors.h" // Doom::moveFloor
#include "Lights.h" // Doom::fireFlicker, lightFlash, strobeFlash, glow
#include "Mobj.h"
#include "Plats.h" // Doom::platRaise

void Doom::Mobj::tick()
{
    mobjThinker(*this);
}
void Doom::Door::tick()
{
    verticalDoor(*this);
}
void Doom::Ceiling::tick()
{
    moveCeiling(*this);
}
void Doom::FloorMove::tick()
{
    moveFloor(*this);
}
void Doom::Plat::tick()
{
    platRaise(*this);
}
void Doom::FireFlicker::tick()
{
    fireFlicker(*this);
}
void Doom::LightFlash::tick()
{
    lightFlash(*this);
}
void Doom::Strobe::tick()
{
    strobeFlash(*this);
}
void Doom::Glow::tick()
{
    glow(*this);
}
