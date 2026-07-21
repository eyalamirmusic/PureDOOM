#pragma once

#include "GameDefs.h" // GameMode, Language

namespace Doom
{
// The identity of the loaded game data, fixed at startup by D_IdentifyVersion and read
// wherever content differs by it: gamemode is the IWAD's edition (shareware / registered /
// retail / commercial), modifiedgame records whether a PWAD has been layered on, and
// language selects the string table the HUD draws from. doomstat.h's "Game Mode" and
// "Language" sections.
//
// A cluster of doomstat.h's game state moved off the loose globals into the Engine
// (REFACTOR.md, Step 5). All three were externed only in doomstat.h and defined in
// Game/State.cpp (whose whole content they were); the vanilla names become references onto
// these members. None is hashed, so the move is golden-neutral like the rest.
//
// No gamemission: the mission-pack identifier (Doom / Doom II / the add-ons) had
// zero reads *and* zero writes beyond its own default member initializer, in this
// rewrite or in vanilla doomstat.c - unlike gamemode, which is fully wired and
// stays. Verified against the 1993 source in this repository's history; deleted
// rather than carried, as no read was lost.
struct GameVersion
{
    GameMode gamemode = GameMode::
        Indetermined; // GameMode::Shareware / GameMode::Registered / GameMode::Retail / GameMode::Commercial

    Language language = Language::English; // string-table language

    bool modifiedgame = false; // a PWAD has modified the game
};

// The one GameVersion, a view onto the Engine's member - the same pattern as
// launchOptions(), levelStats(), viewPoint(), clip(), level(), wad() and randomness().
GameVersion& gameVersion();
} // namespace Doom
