// The game-wide identity globals: which IWAD (shareware / registered / retail),
// which mission, which language, and whether a PWAD has modified the game.
//
// Rewritten from doomstat, whose whole content was these four definitions. They
// stay ::-scoped (declared in doomstat.h, read by ~17 files) rather than moving
// into namespace Doom; d_main's D_IdentifyVersion is what fills them at startup.

#include "../doomstat.h"

GameMode_t gamemode = indetermined;
GameMission_t gamemission = doom;

Language_t language = english;

doom_boolean modifiedgame;
