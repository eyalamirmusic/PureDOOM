#pragma once

#include "../Game/Event.h" // Event
#include "../m_fixed.h" // fixed_t
#include "../tables.h" // angle_t

namespace Doom
{
// The automap; am_map.cpp keeps the vanilla AM_ names as shims.
doom_boolean automapResponder(Event* ev);
void automapTicker();
void drawAutomap();
void stopAutomap();
void rotateAutomapPoint(fixed_t* x, fixed_t* y, angle_t a);
void drawAutomapMarks();
} // namespace Doom
