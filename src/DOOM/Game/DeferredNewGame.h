#pragma once

#include "GameDefs.h" // Skill

namespace Doom
{
// The deferred new-game request. Starting a game cannot happen inside the responder that asks
// for it (a level load mid-event would be unsafe), so Doom::deferInitNew - the menu's skill/episode
// pick, or -warp / -skill on the command line - stashes the chosen skill, episode and map here
// and raises gameaction = GameAction::NewGame; Doom::gameTicker replays them into Doom::initNewGame (initNewGame) when the
// tic runs. g_game's own file-scope state, read by no other file.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); the vanilla names
// d_skill/d_episode/d_map become references onto these members. None is hashed or config-backed,
// so the move is golden-neutral. (gameaction itself, the action they are the payload of, is in
// GameFlow; these three are the GameAction::NewGame arguments, kept as their own small cluster so GameFlow
// stays the screen/wipe/action state.)
struct DeferredNewGame
{
    Skill d_skill = Skill::Baby; // pending skill (vanilla zero-inits this)
    int d_episode = 0; // pending episode
    int d_map = 0; // pending map
};

// The one DeferredNewGame, a view onto the Engine's member - the same pattern as the other
// Game/ clusters (gameSession(), gameFlow(), ...).
DeferredNewGame& deferredNewGame();
} // namespace Doom
