#pragma once

#include "../p_mobj.h" // Mobj
#include "../r_defs.h" // Line

namespace Doom
{
// Teleport `thing` to the MT_TELEPORTMAN in a sector tagged by `line`, with fog and
// sound. Returns 1 if it teleported, 0 otherwise. p_telept.cpp keeps the vanilla
// name Doom::teleport as a shim (p_spec/p_switch call it). Golden-neutral; the demos
// walk teleporters.
int teleport(Line* line, int side, Mobj* thing);
} // namespace Doom
