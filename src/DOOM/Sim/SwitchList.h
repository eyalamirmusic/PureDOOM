#pragma once

#include <map>

namespace Doom
{
// What a wall switch flips to, and where in the switch table the pair was found.
struct SwitchPair
{
    int paired = 0; // the texture the pressed one becomes

    // Vanilla walked the switch table once per press, testing the side's top,
    // middle and bottom texture against each entry in turn - so a side carrying
    // two *different* switch textures flips whichever appears earlier in the
    // table, not the higher surface. This is that position, and comparing it is
    // what keeps the choice identical now that the lookup is keyed rather than
    // scanned. Top-over-middle-over-bottom is only the tie-break, for a side
    // wearing the same switch twice.
    int order = 0;
};

// The wall-switch table initSwitchList builds from the WAD and
// Line::changeSwitchTexture looks a pressed switch up in, keyed by texture
// number. Both directions are in it: pressing the off texture yields the on one
// and vice versa.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5).
// Live simulation-golden-covered - the demos press switches on E1M1 - so the
// byte-identical *.hashes are a live confirmation.
//
// The vanilla shape was a flat int array whose *pairing was index parity*
// (switchlist[i ^ 1]), sized by a maxSwitches of 50, counted by a separate
// numswitches, and terminated by a -1 that nothing read - three ways to say the
// length that did not agree, and initSwitchList bounded its walk of the *source*
// table with the *destination's* constant. Only the source's terminator kept that
// read in bounds. A map has one length and no encoding to remember.
struct SwitchList
{
    std::map<int, SwitchPair> pairs;
};

// The one SwitchList, a view onto the Engine's member - the same pattern as the other clusters.
SwitchList& switchList();
} // namespace Doom
