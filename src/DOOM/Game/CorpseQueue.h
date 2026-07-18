#pragma once

// Doom::Mobj is used only by pointer here, so a forward declaration is enough - no need to pull
// the whole mobj definition into everyone who includes the Engine.
namespace Doom
{
struct Mobj;
} // namespace Doom

namespace Doom
{
// The rotating queue of player corpses. So old bodies do not pile up forever, G_QueueBody
// keeps only the last BODYQUESIZE of them: each reborn player's old mobj is stored at
// bodyque[bodyqueslot % BODYQUESIZE] and the one it displaces is removed, with bodyqueslot the
// ever-incrementing insert counter. bodyqueslot is doomstat.h's ("Internal parameters, used
// for engine"); bodyque[] is g_game's own array - together they are one mechanism.
//
// Moved off the loose globals into the Engine (REFACTOR.md, Step 5). Both were defined in
// Game/Game.cpp (bodyqueslot externed in doomstat.h, bodyque[] file-local there); the vanilla
// names become references onto these members, bodyque[] as a reference-to-array. bodyqueslot's
// Doom::setupLevel reset (`bodyqueslot = 0`) is on the demos' level-load path, so a reference
// reads the identical value and the move is golden-neutral.
struct CorpseQueue
{
    static constexpr int size = 32; // BODYQUESIZE: how many corpses persist

    Mobj* bodyque[size] = {}; // the retained corpse mobjs
    int bodyqueslot = 0; // ever-incrementing insert counter
};

// The one CorpseQueue, a view onto the Engine's member - the same pattern as the other
// Game/ clusters (gameClock(), intermissionInfo(), ...).
CorpseQueue& corpseQueue();
} // namespace Doom
