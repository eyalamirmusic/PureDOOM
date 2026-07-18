#pragma once

#include "../Game/Event.h" // Event

namespace Doom
{
// End-of-episode finale (text screen, cast call, bunny scroll); f_finale.cpp keeps
// the vanilla F_ names as shims.
doom_boolean finaleResponder(Event* ev);
void finaleTicker();
void drawFinale();
void startFinale();
} // namespace Doom
