#pragma once

#include "../Math/FixedPoint.h" // fixed_t

namespace Doom
{
// The weapon code's small scratch: the auto-aim vertical slope the fire actions shoot along
// (bulletslope, set by Doom::computeBulletSlope before a hitscan weapon fires).
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); this was
// Sim/Weapon's own namespace-scope private global, read by no other file. computeBulletSlope
// hoists weaponScratch() once and reaches it through it, rather than through a file-scope
// reference alias (REFACTOR.md, Step 9 strand (a)); gunShot and fireShotgun2 each touch it once
// and reach it inline. Live simulation-golden-covered - the demos fire the pistol, shotgun and
// chaingun, all of which read bulletslope - so the byte-identical *.hashes are a live
// confirmation. (swingx/swingy - a weapon bob offset the vanilla comment described as set by
// A_WeaponReady - were dropped outright in the same sweep: A_WeaponReady computes the bob directly
// into psp->sx/sy from player->bob, and no reader anywhere ever set or read these two.)
struct WeaponScratch
{
    fixed_t bulletslope {}; // auto-aim vertical slope for a hitscan shot
};

// The one WeaponScratch, a view onto the Engine's member - the same pattern as the other clusters
// (actionScratch(), clip(), ...).
WeaponScratch& weaponScratch();
} // namespace Doom
