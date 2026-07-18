// The game-wide identity globals: which IWAD (shareware / registered / retail),
// which mission, which language, and whether a PWAD has modified the game.
//
// Rewritten from doomstat, whose whole content was these four definitions. They now
// live in Doom::GameVersion (an Engine member); the ::-scoped vanilla names (declared
// in doomstat.h, read by ~17 files) are references onto it (REFACTOR.md, Step 5).
// d_main's D_IdentifyVersion is what fills them at startup.

#include "MapSpawns.h"

#include "GameVersion.h"



