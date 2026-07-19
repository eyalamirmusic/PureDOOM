#pragma once

namespace Doom
{
// The wall-switch texture table Doom::initSwitchList builds from the WAD and Doom::changeSwitchTexture
// looks a pressed switch up in: switchlist holds the on/off texture-number pairs (a switch flips
// between switchlist[i] and switchlist[i^1]) terminated by a -1, and numswitches counts the pairs.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// Sim/Switches' own namespace-scope private globals, read by no other file. initSwitchList and
// changeSwitchTexture each hoist switchList() once and reach its members through it, rather than
// through file-scope reference aliases (REFACTOR.md, Step 9 strand (a)). Live simulation-golden-
// covered - the demos press switches on E1M1 - so the byte-identical *.hashes are a live confirmation.
struct SwitchList
{
    static constexpr int maxSwitches = 50; // MAXSWITCHES in p_spec.h

    int switchlist[maxSwitches * 2] =
        {}; // on/off texture-number pairs, -1 terminated
    int numswitches = 0; // # of switch pairs found
};

// The one SwitchList, a view onto the Engine's member - the same pattern as the other clusters.
SwitchList& switchList();
} // namespace Doom
