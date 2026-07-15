#pragma once

#include "../d_event.h" // event_t

namespace Doom
{
// End-of-episode finale (text screen, cast call, bunny scroll); f_finale.cpp keeps
// the vanilla F_ names as shims.
doom_boolean fResponder(event_t* ev);
void fTicker(void);
void fDrawer(void);
void fStartFinale(void);
} // namespace Doom
