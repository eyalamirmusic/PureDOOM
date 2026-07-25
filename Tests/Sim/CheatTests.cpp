// The cheat-sequence matcher: CheatSequence::check / CheatSequence::param.
//
// This exists because nothing else covers it. The demos never enter a cheat, and
// the menu/automap/finale/intermission harnesses never type one either, so
// CheatSequence::check and ::param were the one piece of live engine code with
// no gate at all over them - which only mattered once the CheatSequence stopped
// being two raw pointers and became a span plus an index.
//
// What is being pinned is the in-band protocol, because it is easy to describe
// wrongly and impossible to notice: the stored sequence is scrambled key codes
// terminated by 0xff, a 1 marks where typed parameters begin, and the zero slots
// after it are written by check() and read back (and re-zeroed) by param().
// A matcher that merely compared strings would pass a "does idclev match" test
// and still break the parameter path.
//
// Pure functions over values, so no Doom::initGame - they share the
// PrimitiveTests process.

#include "../Common.h"

#include <DOOM/UI/CheatTypes.h>

#include <DOOM/Containers.h>

#include <string_view>

using namespace nano;
using namespace Doom;

namespace
{
// "idclev", then the 1 marker, two slots for the episode/map digits, and the
// end-of-sequence 0xff. Copied from UI/StatusBar.cpp's cheat_clev_seq: the bytes
// are the scrambled forms of the letters, which is what check() compares the
// typed key against.
using ClevSequence = Doom::Array<unsigned char, 10>;

ClevSequence clevSeq()
{
    return {0xb2, 0x26, 0xe2, 0x36, 0xa6, 0x6e, 1, 0, 0, 0xff};
}

// Types a string at the cheat, returning what the last key answered. Every key
// but the last must answer false, or the sequence fired early.
bool type(CheatSequence& cheat, std::string_view keys)
{
    auto result = false;

    for (auto key: keys)
        result = cheat.check(key);

    return result;
}

auto tCheatMatches = test("Cheat/matchesAndYieldsParam") = []
{
    auto sequence = clevSeq();
    auto cheat = CheatSequence {{sequence}};

    check(!type(cheat, "idcle"), "an incomplete sequence does not fire");
    check(!type(cheat, "v"), "the letters alone do not fire - the params follow");
    check(!type(cheat, "3"), "nor does the first parameter digit");
    check(type(cheat, "4"), "the second digit completes it");

    check(cheat.param() == "34", "the parameter is the two digits that were typed");
};

// param() clears the slots on the way out, which is what lets the same cheat be
// entered twice in one game. If it did not, the second attempt would compare the
// typed digit against the *previous* one instead of finding an empty slot.
auto tCheatRepeats = test("Cheat/canBeEnteredTwice") = []
{
    auto sequence = clevSeq();
    auto cheat = CheatSequence {{sequence}};

    check(type(cheat, "idclev12"), "first entry fires");
    check(cheat.param() == "12");

    check(type(cheat, "idclev99"), "second entry fires too");
    check(cheat.param() == "99", "and yields its own parameter, not the first's");
};

// A wrong key restarts the match at the beginning rather than merely failing the
// one comparison, so a near-miss followed by the real thing still fires.
auto tCheatRestarts = test("Cheat/wrongKeyRestartsTheMatch") = []
{
    auto sequence = clevSeq();
    auto cheat = CheatSequence {{sequence}};

    check(!type(cheat, "idcx"), "a wrong key breaks the run");
    check(!type(cheat, "lev12"),
          "and the rest of the letters do not fire from where it broke");

    check(type(cheat, "idclev12"), "typing it cleanly from the start does");
};

// The sequence is matched by scrambled key code, so an unrelated key stream must
// not walk it - this is the case that would pass trivially if the comparison had
// been flattened to plain text.
auto tCheatIgnoresOtherKeys = test("Cheat/unrelatedKeysDoNotFire") = []
{
    auto sequence = clevSeq();
    auto cheat = CheatSequence {{sequence}};

    check(!type(cheat, "the quick brown fox jumps over the lazy dog"),
          "ordinary typing does not enter a cheat");
};
} // namespace
