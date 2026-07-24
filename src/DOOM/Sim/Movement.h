#pragma once

#include "SimDefs.h" // Mobj, Doom::Fixed

// The movement-clipping core of vanilla p_map is now Mobj methods, declared on the
// struct in Thinkers/Mobj.h and defined in Sim/Movement.cpp:
//   checkPosition(x, y)  - is this thing clear to occupy (x, y)? Sets the tm*
//                          clipping results in Clip as a side effect.
//   tryMove(x, y)        - try to move there, crossing special lines; false if blocked.
//   teleportMove(x, y)   - move there unconditionally, telefragging what is in the way.
//   thingHeightClip()    - re-seat floorz/ceilingz after the sector under it moved.
// The clipping state they read and write lives in Clip (Sim/Clip.h). The PIT_*
// blockmap callbacks (stompThing / checkLine / checkThing) stay free functions in
// Movement.cpp - the blockmap iterator takes their address.
//
// Pinned directly by Tests/Sim/ScenarioTests.cpp (a barrel on the player start flips
// checkPosition, a blocked tryMove leaves the mobj put) and in aggregate by every
// demo. Golden-neutral: the clip scratch is never hashed.
