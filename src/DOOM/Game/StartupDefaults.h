#pragma once

#include "GameDefs.h" // Skill
#include "../doomtype.h" // doom_boolean

namespace Doom
{
// The skill, episode and map a new game starts on when nothing else picks them - the menu's
// defaults, and what -skill / -episode / -warp fill in at launch. autostart is set when the
// command line asked for a specific start, so Doom::doomMain jumps straight into the game rather
// than the title loop. doomstat.h's "Defaults for menu" half of the skill/map section.
//
// A cluster of doomstat.h's game state moved off the loose globals into the Engine
// (REFACTOR.md, Step 5). All four were externed only in doomstat.h and defined in
// Game/DoomMain.cpp above its namespace (a state owner); the vanilla names become references
// onto the members. None is hashed, so the move is golden-neutral like the rest.
struct StartupDefaults
{
    Skill startskill = sk_baby; // default skill (vanilla zero-inits this)
    int startepisode = 0; // default episode
    int startmap = 0; // default map

    doom_boolean autostart = 0; // command line asked for a specific start
};

// The one StartupDefaults, a view onto the Engine's member - the same pattern as
// gameSession(), gameVersion(), launchOptions(), levelStats(), clip(), level() and wad().
StartupDefaults& startupDefaults();
} // namespace Doom
