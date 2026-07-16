#pragma once

#include "../m_fixed.h" // fixed_t

namespace Doom
{
// The weapon code's small scratch: the weapon's bob offset this tic (swingx / swingy, set by
// A_WeaponReady from the player's bob) and the auto-aim vertical slope the fire actions shoot along
// (bulletslope, set by P_BulletSlope before a hitscan weapon fires).
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// Sim/Weapon's own namespace-scope private globals, read by no other file. The vanilla names become
// references onto the members. Live simulation-golden-covered - the demos fire the pistol, shotgun
// and chaingun, all of which read bulletslope - so the byte-identical *.hashes are a live
// confirmation.
struct WeaponScratch
{
    fixed_t swingx = 0; // weapon bob x offset this tic
    fixed_t swingy = 0; // weapon bob y offset this tic
    fixed_t bulletslope = 0; // auto-aim vertical slope for a hitscan shot
};

// The one WeaponScratch, a view onto the Engine's member - the same pattern as the other clusters
// (actionScratch(), clip(), ...).
WeaponScratch& weaponScratch();
} // namespace Doom
