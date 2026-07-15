#pragma once

#include "../d_player.h" // wbstartstruct_t

namespace Doom
{
// Level-completion intermission; wi_stuff.cpp keeps the vanilla WI_ names as shims.
void wiTicker(void);
void wiDrawer(void);
void wiStart(wbstartstruct_t* wbstartstruct);
} // namespace Doom
