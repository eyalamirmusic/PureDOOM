#pragma once

#include "../d_event.h" // event_t
#include "../m_fixed.h" // fixed_t
#include "../tables.h"  // angle_t

namespace Doom
{
// The automap; am_map.cpp keeps the vanilla AM_ names as shims.
doom_boolean amResponder(event_t* ev);
void amTicker(void);
void amDrawer(void);
void amStop(void);
void amRotate(fixed_t* x, fixed_t* y, angle_t a);
void amDrawMarks(void);
} // namespace Doom
