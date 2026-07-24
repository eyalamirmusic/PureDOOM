#pragma once

#include "../Containers.h"
#include "../doomtype.h"

#include <cstdint>

namespace Doom
{
// A view onto Random's fixed 256-entry table, read as rndtable()[i]. Was an
// `extern const unsigned char*` global (m_random.h).
const unsigned char* rndtable();

// DOOM is not random at all. It walks a fixed 256-byte table with an index, and
// keeps two indices into it.
//
// `play` is P_Random's, and it is the one the game world turns on: damage rolls,
// monster decisions, weapon spread. A demo replays byte-identically only because
// this sequence is reproduced exactly, so a change that adds, drops or reorders a
// single call shifts everything after it and the world diverges within a few
// tics. `menu` is M_Random's, for everything outside the simulation, and is kept
// apart on purpose - which is why the screen melt (M_Random) cannot desync a
// demo, and why the status bar's face may twitch differently without the world
// caring.
//
// This is the first piece of engine state to live inside an object rather than in
// a global, and it is deliberately the smallest one: it is the rehearsal for
// Step 5, where the rest follows.
struct Random
{
    static constexpr auto tableSize = 256;

    // `int`, not `std::uint8_t`, and masked by hand. The simulation probe hashes
    // four bytes of the play index every tic (it is the sharpest canary there is
    // for an accidental change to the world), and narrowing the type would change
    // what those hashes are over. The refactor may change how the tests find
    // state; it may never change what they mix.
    int playIndex = 0;
    int menuIndex = 0;

    // Both indices step BEFORE they read, so the first value out is table[1] and
    // not table[0].
    int forPlay()
    {
        playIndex = (playIndex + 1) & 0xff;
        return table()[playIndex];
    }

    int forMenu()
    {
        menuIndex = (menuIndex + 1) & 0xff;
        return table()[menuIndex];
    }

    void clear() { playIndex = menuIndex = 0; }

    static const Array<std::uint8_t, tableSize>& table();
};

// The engine's one instance, for as long as the engine has only one of everything.
Random& randomness();
} // namespace Doom
