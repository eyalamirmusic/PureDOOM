#include "Genmidi.h"

#include <algorithm>

namespace PureDoom
{
namespace
{
constexpr std::string_view genmidiMagic = "#OPL_II#";

// The lump is little-endian on disk, and read a byte at a time rather than by
// casting a struct onto it - so neither this machine's padding rules nor its
// endianness can change what comes out.
int readU16(std::span<const std::uint8_t> bytes, int at)
{
    return bytes[at] | (bytes[at + 1] << 8);
}

std::int16_t readS16(std::span<const std::uint8_t> bytes, int at)
{
    return static_cast<std::int16_t>(readU16(bytes, at));
}

OplVoice readVoice(std::span<const std::uint8_t> bytes, int at)
{
    auto voice = OplVoice {};

    voice.modulatorTremolo = bytes[at + 0];
    voice.modulatorAttack = bytes[at + 1];
    voice.modulatorSustain = bytes[at + 2];
    voice.modulatorWaveform = bytes[at + 3];
    voice.modulatorScale = bytes[at + 4];
    voice.modulatorLevel = bytes[at + 5];
    voice.feedback = bytes[at + 6];
    voice.carrierTremolo = bytes[at + 7];
    voice.carrierAttack = bytes[at + 8];
    voice.carrierSustain = bytes[at + 9];
    voice.carrierWaveform = bytes[at + 10];
    voice.carrierScale = bytes[at + 11];
    voice.carrierLevel = bytes[at + 12];
    // bytes[at + 13] is unused padding in the record.
    voice.baseNoteOffset = readS16(bytes, at + 14);

    return voice;
}

// The name field is fixed-width and NUL-padded, so it is bounded first and
// trimmed second - the same hazard as an 8-byte WAD directory name, which is not
// NUL-terminated when it fills the field.
std::string_view readName(std::span<const std::uint8_t> bytes, int at)
{
    const auto* start = reinterpret_cast<const char*>(bytes.data() + at);
    auto bounded = std::string_view {start, GenmidiBank::nameBytes};
    auto end = bounded.find('\0');

    return end == std::string_view::npos ? bounded : bounded.substr(0, end);
}
} // namespace

bool GenmidiBank::read(std::span<const std::uint8_t> lump)
{
    loaded = false;

    if (lump.size() != static_cast<std::size_t>(lumpBytes))
        return false;

    if (readName(lump, 0).substr(0, genmidiMagic.size()) != genmidiMagic)
        return false;

    const auto names = headerBytes + instrumentCount * recordBytes;

    for (auto i = 0; i < instrumentCount; ++i)
    {
        const auto record = headerBytes + i * recordBytes;
        auto& instrument = instruments[i];

        instrument.flags = readU16(lump, record);
        instrument.fineTuning = lump[record + 2];
        instrument.fixedNote = lump[record + 3];
        instrument.voices[0] = readVoice(lump, record + 4);
        instrument.voices[1] = readVoice(lump, record + 20);
        instrument.name = readName(lump, names + i * nameBytes);
    }

    loaded = true;

    return true;
}

const OplInstrument& GenmidiBank::at(int index) const
{
    return instruments[std::clamp(index, 0, instrumentCount - 1)];
}

const OplInstrument& GenmidiBank::melodic(int program) const
{
    return at(std::clamp(program, 0, melodicCount - 1));
}

const OplInstrument& GenmidiBank::percussion(int note) const
{
    auto clamped = std::clamp(note, firstPercussionNote, lastPercussionNote);

    return at(melodicCount + clamped - firstPercussionNote);
}
} // namespace PureDoom
