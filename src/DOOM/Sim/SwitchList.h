#pragma once

#include "../Containers.h"

namespace Doom
{
// The wall-switch texture table initSwitchList builds from the WAD and changeSwitchTexture
// looks a pressed switch up in: switchlist holds the on/off texture-number pairs, a switch flipping
// between switchlist[i] and switchlist[i ^ 1].
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// Sim/Switches' own namespace-scope private globals, read by no other file. initSwitchList and
// changeSwitchTexture each hoist switchList() once and reach its members through it, rather than
// through file-scope reference aliases (REFACTOR.md, Step 9 strand (a)). Live simulation-golden-
// covered - the demos press switches on E1M1 - so the byte-identical *.hashes are a live confirmation.
//
// A Vector, because the table is built from whichever of Sim/Switches' switch
// textures this game mode owns - shareware keeps episode 1's, retail all three -
// so the length is data, and size() is now the only place it is written down.
// The vanilla shape had three ways to say it and they did not agree: a
// maxSwitches of 50 sizing the destination, a separate numswitches counting what
// was live, and a -1 terminator that nothing ever read (changeSwitchTexture
// bounded its scan by numswitches). Worse, initSwitchList bounded its walk of the
// *source* table with the *destination's* constant - the "guard and array bound
// are not the same token" hazard - and the source has 41 rows, so only its
// terminator kept the read in bounds. It appends now and cannot overrun.
struct SwitchList
{
    Vector<int> switchlist; // on/off texture-number pairs
};

// The one SwitchList, a view onto the Engine's member - the same pattern as the other clusters.
SwitchList& switchList();
} // namespace Doom
