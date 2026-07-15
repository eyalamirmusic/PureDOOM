// The Engine composition root: one owner of the subsystems already rewritten
// (Doom::Random, Doom::WadFile, Doom::Level), with the vanilla free functions as
// views onto its members.
//
// These need no doom_init - the Engine's members are trivially constructible, so
// engine() builds a blank one on first touch. They pin two things: that the free
// accessors really do reach the one Engine's members (a future change that made
// randomness() a separate singleton again would silently split the world in two,
// and the demos might not notice), and that an Engine is a real value with no
// hidden shared state - the property that will let a test own its own world once
// the globals have all moved in.

#include "../Common.h"

#include <DOOM/Engine/Engine.h>
#include <DOOM/Sim/Level.h>
#include <DOOM/Sim/Random.h>
#include <DOOM/Wad/WadFile.h>

using namespace nano;
using namespace Doom;

namespace
{
auto tAccessorsViewTheEngine = test("Engine/freeAccessorsViewTheOneEngine") = []
{
    check(&randomness() == &engine().random, "randomness() is engine().random");
    check(&wad() == &engine().wad, "wad() is engine().wad");
    check(&level() == &engine().level, "level() is engine().level");
};

auto tEngineIsAValue = test("Engine/aFreshEngineIsIndependent") = []
{
    auto other = Engine {};

    // Different storage from the global one.
    check(&other.random != &engine().random, "a second Engine has its own random");
    check(&other.wad != &engine().wad, "a second Engine has its own wad");
    check(&other.level != &engine().level, "a second Engine has its own level");

    // And no shared sequence: advancing one leaves the other where it was.
    other.random.clear();
    engine().random.clear();

    other.random.forPlay();
    other.random.forPlay();

    check(other.random.playIndex == 2, "the second Engine's random advanced");
    check(engine().random.playIndex == 0, "the global engine's random did not");
};
} // namespace
