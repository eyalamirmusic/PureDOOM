#pragma once

#include "../doomdef.h" // GameMode, GameMission, Language
#include "../doomtype.h" // doom_boolean

namespace Doom
{
// The identity of the loaded game data, fixed at startup by D_IdentifyVersion and read
// wherever content differs by it: gamemode is the IWAD's edition (shareware / registered /
// retail / commercial), gamemission the mission pack (Doom / Doom II / the add-ons),
// modifiedgame records whether a PWAD has been layered on, and language selects the string
// table the HUD draws from. doomstat.h's "Game Mode" and "Language" sections.
//
// A cluster of doomstat.h's game state moved off the loose globals into the Engine
// (REFACTOR.md, Step 5). All four were externed only in doomstat.h and defined in
// Game/State.cpp (whose whole content they were); the vanilla names become references onto
// these members. None is hashed, so the move is golden-neutral like the rest.
struct GameVersion
{
    GameMode gamemode =
        indetermined; // shareware / registered / retail / commercial
    GameMission gamemission = doom; // which mission pack

    Language language = english; // string-table language

    doom_boolean modifiedgame = 0; // a PWAD has modified the game
};

// The one GameVersion, a view onto the Engine's member - the same pattern as
// launchOptions(), levelStats(), viewPoint(), clip(), level(), wad() and randomness().
GameVersion& gameVersion();
} // namespace Doom
