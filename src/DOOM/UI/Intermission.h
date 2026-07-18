#pragma once

#include "../d_player.h" // wbstartstruct_t

namespace Doom
{
// Level-completion intermission; wi_stuff.cpp keeps the vanilla WI_ names as shims.
void intermissionTicker();
void drawIntermission();
void startIntermission(wbstartstruct_t* wbstartstruct);
} // namespace Doom
