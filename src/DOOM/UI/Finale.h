#pragma once

#include "../d_event.h" // event_t

namespace Doom
{
// End-of-episode finale (text screen, cast call, bunny scroll); f_finale.cpp keeps
// the vanilla F_ names as shims.
doom_boolean finaleResponder(event_t* ev);
void finaleTicker();
void drawFinale();
void startFinale();
} // namespace Doom
