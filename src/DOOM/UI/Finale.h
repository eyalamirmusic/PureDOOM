#pragma once

#include "../Game/Event.h" // Event

namespace Doom
{
// End-of-episode finale (text screen, cast call, bunny scroll); f_finale.cpp keeps
// the vanilla F_ names as shims.
bool finaleResponder(Event& ev);
void finaleTicker();
void drawFinale();
void startFinale();

// The DOOM II / episode-3 character cast call's per-tic step. Exported so the
// tests can drive it directly: no attract demo or shareware finale ever reaches
// finalestage 2, so this is the one finale path a golden cannot cover (see
// Tests/Sim/CastCallTests.cpp).
void castTicker();
} // namespace Doom
