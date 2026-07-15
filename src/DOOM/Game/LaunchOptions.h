#pragma once

#include "../doomtype.h" // doom_boolean

namespace Doom
{
// The command-line parameters that modify the game, checked long after startup. The three
// gameplay modifiers are read deep in the playsim - nomonsters (-nomonsters) suppresses the
// map's monster spawns, respawnparm (-respawn) has the dead come back, fastparm (-fast)
// speeds the demons and their missiles - and devparm (-devparm) is the developer flag the
// startup and error paths test. doomstat.h's first section, "Command line parameters".
//
// A cluster of doomstat.h's game state moved off the loose globals into the Engine
// (REFACTOR.md, Step 5). All four were externed only in doomstat.h and defined in
// Game/DoomMain.cpp above its namespace; the vanilla names become references onto these
// members. nomonsters/respawnparm/fastparm feed the simulation and so are indirectly on the
// hash's path, but a reference reads the identical value, so the move is golden-neutral.
struct LaunchOptions
{
    doom_boolean nomonsters = 0; // -nomonsters: no monsters at all
    doom_boolean respawnparm = 0; // -respawn: monsters respawn
    doom_boolean fastparm = 0; // -fast: fast monsters and missiles
    doom_boolean devparm = 0; // -devparm: developer mode
};

// The one LaunchOptions, a view onto the Engine's member - the same pattern as
// levelStats(), renderScratch(), viewPoint(), clip(), level(), wad() and randomness().
LaunchOptions& launchOptions();
} // namespace Doom
