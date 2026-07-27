// The OPL instrument bank, decoded from the IWAD and checked against the lump's
// own arithmetic.
//
// This is the one part of the audio path a test can reach. Nothing in the suite
// hashes a sample, so the mixer, the MUS reader and the synth are all invisible
// to every other gate - which is exactly how the two defects fixed while wiring
// audio survived: they were correct-looking code nobody could measure. The bank
// decode is different, because it is a pure function from bytes the WAD already
// pins (Tests/Goldens/doom1.lumps hashes GENMIDI along with every other lump) to
// structure. So it gets a gate.
//
// What the checks are worth: the lump's size is not a magic number here, it is
// `8 + 175 * (36 + 32)`, so a mis-sized record would have to be wrong in a way
// that still balances the whole lump. And the melodic instruments are held
// against General MIDI's own program list by name, which is what catches the
// decode reading the right number of bytes from the wrong offset.

#include "../Common.h"

#include <EngineAccess.h>
#include <Genmidi.h>

#include <string_view>

using namespace nano;
using namespace PureDoom;

namespace
{
GenmidiBank loadBank()
{
    auto bank = GenmidiBank {};
    bank.read(Engine::genmidiLump());

    return bank;
}

auto tGenmidiLumpIsTheSizeItsLayoutImplies =
    test("Port/genmidiLumpIsWellFormed") = []
{
    check(doomSimBoot() != 0, "the engine booted");

    auto lump = Engine::genmidiLump();

    check(!lump.empty(), "doom1.wad carries a GENMIDI lump");
    check(lump.size() == static_cast<std::size_t>(GenmidiBank::lumpBytes),
          "the lump is exactly its header, records and names");

    auto magic = std::string_view {reinterpret_cast<const char*>(lump.data()), 8};

    check(magic == "#OPL_II#", "the lump identifies itself as an OPL bank");
};

auto tGenmidiDecodes = test("Port/genmidiDecodes") = []
{
    check(doomSimBoot() != 0, "the engine booted");

    auto bank = loadBank();

    check(bank.isLoaded(), "the bank decoded");

    // Program 0 and program 127 are General MIDI's first and last melodic
    // instruments. Reading either by the wrong stride lands on a neighbour, and
    // the names say so.
    check(bank.melodic(0).name == "Acoustic Grand Piano",
          "program 0 is the acoustic grand");
    check(bank.melodic(127).name == "Gun Shot", "program 127 is the gun shot");

    // A percussion voice is keyed by note. 35 is the first the bank covers.
    check(!bank.percussion(GenmidiBank::firstPercussionNote).name.empty(),
          "the first percussion note names an instrument");

    // Every name is bounded by the 32-byte field. A view that ran off the end of
    // the lump would show up as a length at or over the field width.
    for (auto i = 0; i < GenmidiBank::instrumentCount; ++i)
        check(bank.at(i).name.size()
                  < static_cast<std::size_t>(GenmidiBank::nameBytes),
              "the instrument's name is bounded by its field");
};

auto tGenmidiFlagsLandAtTheRightOffset = test("Port/genmidiFlagsDecode") = []
{
    check(doomSimBoot() != 0, "the engine booted");

    auto bank = loadBank();

    check(bank.isLoaded(), "the bank decoded");

    // Exact counts, because they pin the two-byte flags word landing at the right
    // offset in a way a range check cannot. Read one byte late, the word would
    // come out of fine-tuning and fixed-note (0x2680 for the first drum), and
    // every one of these would collapse to zero.
    //
    // They are counts from doom1.wad, which is the file every other golden here
    // is taken against.
    auto fixedPercussion = 0;

    for (auto note = GenmidiBank::firstPercussionNote;
         note <= GenmidiBank::lastPercussionNote;
         ++note)
    {
        const auto& instrument = bank.percussion(note);

        if (instrument.isFixedPitch())
        {
            ++fixedPercussion;
            check(instrument.fixedNote > 0,
                  "a fixed-pitch instrument names the note it plays");
        }
    }

    // Not all 47 - three of the bank's percussion voices are pitched, which is
    // worth knowing before writing a player that assumes otherwise.
    check(fixedPercussion == 44, "44 of the 47 percussion voices are fixed-pitch");

    auto doubleVoice = 0;
    auto fixedMelodic = 0;

    for (auto program = 0; program < GenmidiBank::melodicCount; ++program)
    {
        doubleVoice += bank.melodic(program).isDoubleVoice() ? 1 : 0;
        fixedMelodic += bank.melodic(program).isFixedPitch() ? 1 : 0;
    }

    // Bit 2, checked separately from bit 0 so a flags word that decoded as a
    // whole-byte shift could not satisfy both at once.
    check(doubleVoice == 32, "32 melodic instruments sound two voices");
    check(fixedMelodic == 2, "two melodic instruments are fixed-pitch");
};

auto tGenmidiVoicesAreRegisterShaped =
    test("Port/genmidiVoicesAreRegisterShaped") = []
{
    check(doomSimBoot() != 0, "the engine booted");

    auto bank = loadBank();

    check(bank.isLoaded(), "the bank decoded");

    // The bank was authored as OPL register values, so two of the fields have
    // ranges the chip itself imposes: a total level is six bits, and a waveform
    // select is three (and only two on an OPL2, which is what this bank predates
    // the OPL3 for). Bytes read from the wrong offset would land outside them.
    for (auto i = 0; i < GenmidiBank::instrumentCount; ++i)
    {
        for (const auto& voice: bank.at(i).voices)
        {
            check((voice.modulatorLevel & 0xc0) == 0,
                  "the modulator's total level is six bits");
            check((voice.carrierLevel & 0xc0) == 0,
                  "the carrier's total level is six bits");
            check(voice.modulatorWaveform <= 7,
                  "the modulator's waveform select is three bits");
            check(voice.carrierWaveform <= 7,
                  "the carrier's waveform select is three bits");
        }
    }
};

auto tGenmidiRejectsRubbish = test("Port/genmidiRejectsRubbish") = []
{
    auto bank = GenmidiBank {};

    check(!bank.read({}), "an empty lump is refused");
    check(!bank.isLoaded(), "and leaves the bank empty");

    // Right size, wrong magic: the size check alone would pass this.
    auto wrongMagic = std::vector<std::uint8_t>(GenmidiBank::lumpBytes, 0);

    check(!bank.read(wrongMagic), "a lump that is not an OPL bank is refused");
    check(!bank.isLoaded(), "and still leaves the bank empty");
};
} // namespace
