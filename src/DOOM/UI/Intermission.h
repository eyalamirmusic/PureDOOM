#pragma once

#include "../d_player.h" // IntermissionStart

namespace Doom
{
// Level-completion intermission; wi_stuff.cpp keeps the vanilla WI_ names as shims.
void intermissionTicker();
void drawIntermission();
void startIntermission(IntermissionStart* wbstartstruct);
} // namespace Doom
