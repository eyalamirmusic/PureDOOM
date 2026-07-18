#pragma once

// Forward declaration at global scope (that is where p_mobj.h declares it) - the cluster holds a
// pointer to it, not its layout, so declaring it inside namespace Doom would make a distinct Doom::
// type that would not bind to the vanilla mobj_t the reference alias points at.
struct mobj_t; // mobj_t

namespace Doom
{
// The last thing that made noise, propagated to nearby monsters as Doom::noiseAlert / P_RecursiveSound
// walk outward from it (each sector's own soundtarget member is then set to this). Vanilla kept it a
// global so the recursion could read it without threading it through every call.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5). The old comment on it
// said "global because p_saveg archives it" - but p_saveg only ever touches Sector::soundtarget
// (the per-sector member, reset to 0 on load), never this global, so it was free to migrate. The
// p_enemy.cpp definition and Sim/Enemy's extern become references onto this member. Live
// simulation-golden-covered - the demos alert monsters by gunfire, so the byte-identical *.hashes
// confirm it.
struct SoundTarget
{
    mobj_t* soundtarget = nullptr; // the noise source the alert is propagating from
};

SoundTarget& soundTarget();
} // namespace Doom
