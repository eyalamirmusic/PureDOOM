#pragma once

#include "../doomdata.h" // mapthing_t
#include "../p_local.h" // ITEMQUESIZE

namespace Doom
{
// The ring of picked-up items waiting to respawn, used only in deathmatch. When an item is
// collected, P_RemoveMobj records its map thing in itemrespawnque and the leveltime in
// itemrespawntime; P_RespawnSpecials walks the ring from iquetail, and once an entry has aged
// past its respawn delay it spawns the item back and advances the tail. iquehead is the append
// cursor, both indices wrapping modulo ITEMQUESIZE. p_local.h's item-respawn queue.
//
// A p_local.h cluster moved off the loose globals into the Engine (REFACTOR.md, Step 5). All
// four were externed only in p_local.h and defined in p_mobj.cpp (a flat playsim shim); the
// vanilla names become references onto the members, the two arrays as references-to-array. The
// demos are single-player, so the deathmatch respawn walk is not exercised (nor hashed) - but
// Doom::setupLevel's `iquehead = iquetail = 0` reset is on the level-load path they all take, and
// the reference bindings are mechanical, so the move is golden-neutral.
struct ItemRespawnQueue
{
    mapthing_t itemrespawnque[ITEMQUESIZE] =
        {}; // the picked-up items awaiting respawn
    int itemrespawntime[ITEMQUESIZE] = {}; // the leveltime each was collected at

    int iquehead = 0; // append cursor
    int iquetail = 0; // respawn cursor
};

// The one ItemRespawnQueue, a view onto the Engine's member - the same pattern as
// clip(), level(), random() and the Game/ clusters.
ItemRespawnQueue& itemRespawnQueue();
} // namespace Doom
