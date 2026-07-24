#pragma once

#include "../Game/Event.h" // Event
#include "../Math/FixedPoint.h" // Fixed
#include "../Math/TrigTables.h" // Angle

namespace Doom
{
// The automap; am_map.cpp keeps the vanilla AM_ names as shims.
bool automapResponder(Event& ev);
void automapTicker();
void drawAutomap();
void stopAutomap();
void rotateAutomapPoint(Fixed& x, Fixed& y, Angle a);
void drawAutomapMarks();
} // namespace Doom
