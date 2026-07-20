#pragma once

#include "Event.h" // Event, MAXEVENTS
#include "DoomMain.h"

#include "../Containers.h"

namespace Doom
{
// The engine's input event ring buffer. The host posts events with Doom::postEvent, which writes
// events[eventhead] and advances eventhead; the game drains them with Doom::processEvents (and the
// netcode's Doom::netUpdate does the same drain), reading from eventtail up to eventhead, both indices
// wrapping modulo MAXEVENTS. It is the asynchronous boundary between the platform and the game -
// keys, mouse and joystick motion arrive here and are dispatched to the responders each tic.
//
// A d_main/d_net cluster moved off the loose globals into the Engine (REFACTOR.md, Step 5). All
// three were externed in d_event.h and defined in Game/DoomMain.cpp; the vanilla names become
// references onto these members, events[] as a reference-to-array. The queue is never hashed or
// reset by the level load, so the move is golden-neutral by construction - a reference reads the
// identical slot.
struct EventQueue
{
    Array<Event, MAXEVENTS> events = {}; // the pending input events
    int eventhead = 0; // where the next posted event lands
    int eventtail = 0; // where the next drained event is read
};

// The one EventQueue, a view onto the Engine's member - the same pattern as the other Game/
// clusters (gameClock(), corpseQueue(), ...).
EventQueue& eventQueue();
} // namespace Doom
