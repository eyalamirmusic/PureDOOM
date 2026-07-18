#pragma once

#include "../p_local.h" // mobj_t, fixed_t

namespace Doom
{
// The movement-clipping core of vanilla p_map: does a mobj fit at a position, and
// the commit of a move if it does. The clipping state it reads and writes lives in
// Clip (Sim/Clip.h) - tmthing, the tm bounding box, tmfloorz/tmceilingz, the spechit
// list. p_map.cpp keeps the vanilla names (Doom::checkPosition, Doom::tryMove, ...) as shims
// forwarding here, and the externally-read results (floatok, tmfloorz, ceilingline,
// spechit, numspechit) stay as references onto Clip for p_enemy/p_mobj until those
// files are rewritten.
//
// Pinned directly by Tests/Sim/ScenarioTests.cpp (a barrel on the player start flips
// checkPosition, a blocked tryMove leaves the mobj put) and in aggregate by every
// demo. Golden-neutral: the clip scratch is never hashed.

// Is `thing` clear to occupy (x, y)? Sets the tm* clipping results in Clip as a
// side effect (floor/ceiling window, spechit list); picks up MF_SPECIAL things it
// overlaps if the mover has MF_PICKUP. Nothing else is modified.
bool checkPosition(mobj_t* thing, fixed_t x, fixed_t y);

// Try to move `thing` to (x, y), crossing any special lines unless MF_TELEPORT.
// Returns false and leaves the mobj where it was if blocked; on success links it
// into its new cell and runs the crossed specials.
bool tryMove(mobj_t* thing, fixed_t x, fixed_t y);

// Move `thing` to (x, y) unconditionally, telefragging any shootable thing already
// there (monsters only on the boss map). Always succeeds.
bool teleportMove(mobj_t* thing, fixed_t x, fixed_t y);

// Re-seat a thing's floorz/ceilingz (and z, if it stood on the floor) after the
// sector under it changed height. Returns false if it no longer fits.
bool thingHeightClip(mobj_t* thing);
} // namespace Doom
