#pragma once

#include "SpecialTypes.h" // Plat, Ceiling, Button, MAXPLATS, MAXCEILINGS, MAXBUTTONS

#include "../Containers.h"

namespace Doom
{
// The level's active special-effect registries. activeplats and activeceilings hold the
// moving-sector thinkers currently running - the ones Doom::doPlat/Doom::doCeiling add and the
// stasis lines pause and resume - so the crush/stop specials can find and act on an already-
// moving sector; buttonlist holds the switch textures counting down to revert (Doom::startButton
// arms one, Doom::updateSpecials ticks the timers). Doom::spawnSpecials clears all three together at
// the top of every level ("Init other misc stuff"), which is what makes them one cluster: the
// active-special state a level starts empty and fills as its machinery runs. p_spec.h's
// buttonlist / activeplats / activeceilings.
//
// A p_spec cluster moved off the loose globals into the Engine (REFACTOR.md, Step 5). Each was
// externed in p_spec.h and defined in its flat special shim (p_plats.cpp, p_ceilng.cpp,
// p_switch.cpp), which still owns the vanilla name - now a reference-to-array onto these
// members. activeceilings is read by the save code (Sim/SaveGame archives active ceilings by
// slot), so the p_saveg net covers this move as well as the demos; a reference reads the
// identical slot, so both hold byte-identical.
struct ActiveSpecials
{
    Array<Plat*, MAXPLATS> activeplats = {}; // the running platform/lift thinkers
    Array<Ceiling*, MAXCEILINGS> activeceilings =
        {}; // the running ceiling/crusher thinkers
    Array<Button, MAXBUTTONS> buttonlist =
        {}; // switch textures counting down to revert
};

// The one ActiveSpecials, a view onto the Engine's member - the same pattern as
// itemRespawnQueue(), clip(), level() and the Game/ clusters.
ActiveSpecials& activeSpecials();
} // namespace Doom
