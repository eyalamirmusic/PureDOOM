#pragma once

#include "../Game/Event.h" // Event
#include "../Math/FixedPoint.h" // fixed_t
#include "../Math/TrigTables.h" // angle_t

namespace Doom
{
// The automap; am_map.cpp keeps the vanilla AM_ names as shims.
bool automapResponder(Event* ev);
void automapTicker();
void drawAutomap();
void stopAutomap();
void rotateAutomapPoint(fixed_t* x, fixed_t* y, angle_t a);
void drawAutomapMarks();
} // namespace Doom
