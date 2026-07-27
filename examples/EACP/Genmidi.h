#pragma once

#include <eacp/Core/Utils/Containers.h>

#include <cstdint>
#include <span>
#include <string_view>

namespace PureDoom
{
// The container vocabulary, re-exported the way EngineAccess.h does it. Said
// here too so the bank stands on its own: it decodes bytes and knows nothing
// about the engine.
using eacp::Array;
using eacp::OwningPointer;
using eacp::Vector;

// DOOM's own OPL2 instrument bank, decoded from the IWAD's GENMIDI lump.
//
// This is why the OPL music path ships no sound bank: the patches the music was
// composed against are in the file the game already boots from. The lump is
// `#OPL_II#`, then 175 fixed-size instrument records, then 175 32-byte names -
// 128 General MIDI melodic instruments followed by 47 percussion voices, keyed
// by note rather than by program.
//
// The record layout is the on-disk one and is *not* read by casting onto the
// bytes: the fields are pulled out by offset, so the decode does not depend on
// this machine's struct padding or endianness.

// A single OPL 2-operator voice: the register values for its modulator and its
// carrier, plus how far the note is transposed when played through it.
struct OplVoice
{
    // Register bytes, in the order the chip's banks take them. Each is written
    // verbatim to its operator's register; nothing here is decoded further,
    // because the bank was authored as register values in the first place.
    std::uint8_t modulatorTremolo =
        0; // 0x20 - AM / vibrato / EG type / KSR / multiple
    std::uint8_t modulatorAttack = 0; // 0x60 - attack / decay
    std::uint8_t modulatorSustain = 0; // 0x80 - sustain / release
    std::uint8_t modulatorWaveform = 0; // 0xE0 - waveform select
    std::uint8_t modulatorScale = 0; // 0x40 - key scale level (bits 6-7)
    std::uint8_t modulatorLevel = 0; // 0x40 - total level (bits 0-5)

    // 0xC0 - feedback and connection. Bit 0 clear is FM (the carrier alone
    // sounds), set is additive (both operators do), which is what decides how
    // many operators a volume change has to touch.
    std::uint8_t feedback = 0;

    std::uint8_t carrierTremolo = 0;
    std::uint8_t carrierAttack = 0;
    std::uint8_t carrierSustain = 0;
    std::uint8_t carrierWaveform = 0;
    std::uint8_t carrierScale = 0;
    std::uint8_t carrierLevel = 0;

    // Signed, and applied to the MIDI note before it becomes a frequency.
    std::int16_t baseNoteOffset = 0;

    bool isAdditive() const { return (feedback & 0x01) != 0; }
};

struct OplInstrument
{
    // Bit 0: the instrument plays one fixed pitch whatever note asked for it -
    // every percussion voice, and a handful of melodic ones. Bit 2: two voices
    // sound together, which costs two of the chip's channels per note.
    static constexpr int fixedPitchFlag = 0x0001;
    static constexpr int doubleVoiceFlag = 0x0004;

    bool isFixedPitch() const { return (flags & fixedPitchFlag) != 0; }
    bool isDoubleVoice() const { return (flags & doubleVoiceFlag) != 0; }

    int flags = 0;

    // Detune for the second voice, 128 being none. Only meaningful when
    // isDoubleVoice(); it is what makes a doubled instrument beat rather than
    // just sound twice as loud.
    int fineTuning = 128;

    // The note a fixed-pitch instrument always plays.
    int fixedNote = 0;

    Array<OplVoice, 2> voices;

    // From the lump's name table. A view into the caller's lump bytes, so it
    // lives exactly as long as the bank does.
    std::string_view name;
};

// The whole bank. Empty until read() succeeds, and read() is the only way to
// fill it - a half-decoded bank is not a state anything here can reach.
class GenmidiBank
{
public:
    static constexpr int melodicCount = 128;
    static constexpr int percussionCount = 47;
    static constexpr int instrumentCount = melodicCount + percussionCount;

    // Percussion is keyed by MIDI note, not by program, and the bank's 47 entries
    // cover notes 35..81 - General MIDI's own percussion range.
    static constexpr int firstPercussionNote = 35;
    static constexpr int lastPercussionNote =
        firstPercussionNote + percussionCount - 1;

    static constexpr int recordBytes = 36;
    static constexpr int nameBytes = 32;
    static constexpr int headerBytes = 8;
    static constexpr int lumpBytes =
        headerBytes + instrumentCount * (recordBytes + nameBytes);

    // False - leaving the bank empty - if the lump is the wrong size or does not
    // begin `#OPL_II#`. Both are checked before a single byte is read out of it.
    bool read(std::span<const std::uint8_t> lump);

    bool isLoaded() const { return loaded; }

    // The melodic instrument a General MIDI program selects.
    const OplInstrument& melodic(int program) const;

    // The percussion instrument a note on the drum channel selects. Notes
    // outside the bank's range clamp into it, which is what the engine's own
    // percussion mapping does.
    const OplInstrument& percussion(int note) const;

    const OplInstrument& at(int index) const;

private:
    bool loaded = false;
    Array<OplInstrument, instrumentCount> instruments;
};
} // namespace PureDoom
