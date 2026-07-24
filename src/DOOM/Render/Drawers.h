#pragma once

#include <functional>

namespace Doom
{
// The column and span drawers the renderer switches between, for detail mode and
// for the spectre fuzz effect.
//
// These were four raw `void (*)()` globals externed from Render/Main.h plus a
// fifth (transcolfunc) that was file-local to Render/Main.cpp - the last
// function-pointer dispatch left in the engine. They are std::functions on a
// state cluster now, reached through drawers(), so the renderer's drawer
// selection is Engine state like every other cluster rather than link-time
// mutable globals.
//
// They are called once per column and once per span, which is the hottest loop in
// the engine, so the indirection was measured rather than assumed: a std::function
// call is one extra pointer hop over a raw function pointer, and each call runs a
// whole column or span - a couple of hundred pixels - underneath it, so the cost
// amortises away. The demo suite's wall clock does not move.
//
// Defaults are no-ops rather than empty, per the house rule: a call site invokes
// them directly with no null check. Nothing should reach a drawer before
// executeSetViewSize has chosen one - the raw pointers were uninitialised until
// then, so an early call used to be undefined behaviour rather than a crash.
using Drawer = std::function<void()>;

struct Drawers
{
    Drawer column = [] {}; // was colfunc
    Drawer baseColumn = [] {}; // was basecolfunc
    Drawer fuzzColumn = [] {}; // was fuzzcolfunc
    Drawer translatedColumn = [] {}; // was transcolfunc
    Drawer span = [] {}; // was spanfunc
};

// The one Drawers, a view onto the Engine's member - the same pattern as the
// other clusters.
Drawers& drawers();
} // namespace Doom
