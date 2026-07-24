#pragma once

#include "SimDefs.h" // Mobj, Doom::Fixed

namespace Doom
{
// The movement-clipping core of vanilla p_map: does a mobj fit at a position, and
// the commit of a move if it does. The clipping state it reads and writes lives in
// Clip (Sim/Clip.h) - tmthing, the tm bounding box, tmfloorz/tmceilingz, the spechit
// list. p_map.cpp keeps the vanilla names (checkPosition, tryMove, ...) as shims
// forwarding here, and the externally-read results (floatok, tmfloorz, ceilingline,
// spechit, numspechit) stay as references onto Clip for p_enemy/p_mobj until those
// files are rewritten.
//
// Pinned directly by Tests/Sim/ScenarioTests.cpp (a barrel on the player start flips
// checkPosition, a blocked tryMove leaves the mobj put) and in aggregate by every
// demo. Golden-neutral: the clip scratch is never hashed.

// Is `thing` clear to occupy (x, y)? Sets the tm* clipping results in Clip as a
// side effect (floor/ceiling window, spechit list); picks up flagBits(MobjFlag::Special) things it
// overlaps if the mover has flagBits(MobjFlag::Pickup). Nothing else is modified.
bool checkPosition(Mobj& thing, Fixed x, Fixed y);

// Try to move `thing` to (x, y), crossing any special lines unless flagBits(MobjFlag::Teleport).
// Returns false and leaves the mobj where it was if blocked; on success links it
// into its new cell and runs the crossed specials.
bool tryMove(Mobj& thing, Fixed x, Fixed y);

// Move `thing` to (x, y) unconditionally, telefragging any shootable thing already
// there (monsters only on the boss map). Always succeeds.
bool teleportMove(Mobj& thing, Fixed x, Fixed y);

// Re-seat a thing's floorz/ceilingz (and z, if it stood on the floor) after the
// sector under it changed height. Returns false if it no longer fits.
bool thingHeightClip(Mobj& thing);
} // namespace Doom
