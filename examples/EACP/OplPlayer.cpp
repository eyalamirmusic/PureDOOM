#include "OplPlayer.h"

extern "C"
{
#include <opl3.h>
}

#include <algorithm>
#include <cmath>

namespace PureDoom
{
namespace
{
// The OPL's own operator layout. Within a bank of nine channels the operators
// are not consecutive - channel 3 starts at 0x08, not 0x06 - and the carrier
// always sits three registers past its modulator.
constexpr int operatorOffsets[OplPlayer::channelsPerBank] = {
    0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12};

constexpr int carrierOffset = 3;

// Register groups, each indexed by an operator offset or a channel number.
constexpr int regTremolo = 0x20; // AM / vibrato / EG type / KSR / multiple
constexpr int regLevel = 0x40; // key scale level and total level
constexpr int regAttack = 0x60; // attack and decay
constexpr int regSustain = 0x80; // sustain and release
constexpr int regFrequencyLow = 0xa0; // F-number, low eight bits
constexpr int regKeyOn = 0xb0; // key-on, block, F-number high two bits
constexpr int regFeedback = 0xc0; // feedback, connection and the OPL3 pan bits
constexpr int regWaveform = 0xe0; // waveform select

constexpr int regTest = 0x01; // OPL2's waveform-select enable lives here
constexpr int regNoteSelect = 0x08;
constexpr int regRhythm = 0xbd;
constexpr int regFourOperator = 0x104;
constexpr int regNewMode = 0x105; // the bit that makes it an OPL3 at all

// The second register bank. Nuked takes a 16-bit register number and reads this
// bit as the bank select, which is why voices 9-17 are addressed by adding it.
constexpr int secondBank = 0x100;

// A total level is six bits of attenuation, 0 the loudest.
constexpr int maxAttenuation = 0x3f;

// Each total-level step is 0.75 dB, which is what turns a gain into a number of
// steps to add to the patch's own level.
constexpr double decibelsPerStep = 0.75;

// The OPL's master clock divided down: the frequency formula's denominator, and
// a property of the chip rather than of the rate we resample its output to.
constexpr double oplSampleRate = 49716.0;

constexpr int maxBlock = 7;
constexpr int maxFnum = 1023;

// MIDI controller numbers this player acts on. Everything the engine's MUS
// reader can emit is here; anything else is ignored rather than guessed at.
constexpr int ccVolume = 7;
constexpr int ccPan = 10;
constexpr int ccExpression = 11;
constexpr int ccSustain = 64;
constexpr int ccAllSoundsOff = 120;
constexpr int ccResetAllControllers = 121;
constexpr int ccAllNotesOff = 123;

// Pan is one of three states on an OPL3 - left, both, right - so the continuous
// controller is bucketed rather than scaled.
constexpr int panLeftLimit = 32;
constexpr int panRightLimit = 96;

constexpr int panLeftBit = 0x10;
constexpr int panRightBit = 0x20;

struct OplFrequency
{
    int fnum = 0;
    int block = 0;
};

// The chip names a pitch as a fractional F-number times a power-of-two block, so
// the block is the octave and the F-number the position within it. The smallest
// block that keeps the F-number in range is chosen, which keeps the most
// precision - and matters, because pitch bend moves this by fractions of a
// semitone.
OplFrequency frequencyFor(double note)
{
    auto hertz = 440.0 * std::exp2((note - 69.0) / 12.0);

    for (auto block = 0; block <= maxBlock; ++block)
    {
        auto fnum = hertz * std::exp2(20.0 - block) / oplSampleRate;

        if (fnum <= maxFnum)
            return {static_cast<int>(fnum + 0.5), block};
    }

    // Above the chip's range. Nothing in DOOM's music reaches here; clamping
    // keeps a rogue note in tune with itself rather than wrapping to a low one.
    return {maxFnum, maxBlock};
}

// A gain in 0..1 as a number of attenuation steps to add to a patch's own level.
// Zero gain is silence rather than a very large number of steps.
int attenuationFor(double gain)
{
    if (gain <= 0.0)
        return maxAttenuation;

    auto decibels = -20.0 * std::log10(std::min(gain, 1.0));

    return std::clamp(
        static_cast<int>(decibels / decibelsPerStep + 0.5), 0, maxAttenuation);
}
} // namespace

OplPlayer::OplPlayer() = default;
OplPlayer::~OplPlayer() = default;

int OplPlayer::modulatorRegister(int index) const
{
    auto bank = index / channelsPerBank == 0 ? 0 : secondBank;

    return bank + operatorOffsets[index % channelsPerBank];
}

int OplPlayer::carrierRegister(int index) const
{
    return modulatorRegister(index) + carrierOffset;
}

int OplPlayer::channelRegister(int index) const
{
    auto bank = index / channelsPerBank == 0 ? 0 : secondBank;

    return bank + index % channelsPerBank;
}

void OplPlayer::writeRegister(int reg, int value)
{
    OPL3_WriteRegBuffered(chip.get(),
                          static_cast<std::uint16_t>(reg),
                          static_cast<std::uint8_t>(value));
}

void OplPlayer::resetChip()
{
    OPL3_Reset(chip.get(), static_cast<std::uint32_t>(sampleRate));

    // OPL3 mode first: the second bank of nine channels and the four-waveform
    // set do not exist until this bit is set, and every register written before
    // it would land on an OPL2.
    writeRegister(regNewMode, 0x01);
    writeRegister(regFourOperator, 0x00);
    writeRegister(regTest, 0x20);
    writeRegister(regNoteSelect, 0x00);
    writeRegister(regRhythm, 0x00);

    for (auto index = 0; index < voiceCount; ++index)
    {
        writeRegister(regKeyOn + channelRegister(index), 0x00);
        writeRegister(regLevel + modulatorRegister(index), maxAttenuation);
        writeRegister(regLevel + carrierRegister(index), maxAttenuation);
    }
}

bool OplPlayer::start(int sampleRateToUse, std::span<const std::uint8_t> genmidi)
{
    running = false;

    if (sampleRateToUse <= 0 || !bank.read(genmidi))
        return false;

    if (chip == nullptr)
        chip = eacp::makeOwned<_opl3_chip>();

    sampleRate = sampleRateToUse;
    allocationOrder = 0;

    for (auto& channel: channels)
        channel = Channel {};

    for (auto& voice: voices)
        voice = Voice {};

    resetChip();
    running = true;

    return true;
}

int OplPlayer::activeVoices() const
{
    return static_cast<int>(std::count_if(voices.begin(),
                                          voices.end(),
                                          [](const Voice& voice)
                                          { return voice.active; }));
}

int OplPlayer::allocateVoice()
{
    auto oldest = -1;

    for (auto index = 0; index < voiceCount; ++index)
    {
        if (!voices[index].active)
            return index;

        // A sustained voice has already been released by the score and is only
        // still sounding because the pedal is down, so it goes before a voice
        // that is genuinely held.
        if (oldest < 0 || voices[index].sustained > voices[oldest].sustained
            || (voices[index].sustained == voices[oldest].sustained
                && voices[index].order < voices[oldest].order))
            oldest = index;
    }

    if (oldest >= 0)
        releaseVoice(oldest);

    return oldest;
}

void OplPlayer::releaseVoice(int index)
{
    auto& voice = voices[index];

    writeRegister(regKeyOn + channelRegister(index), 0x00);

    voice.active = false;
    voice.sustained = false;
    voice.instrument = nullptr;
    voice.patch = nullptr;
}

void OplPlayer::programVoice(int index, const OplVoice& patch)
{
    auto modulator = modulatorRegister(index);
    auto carrier = carrierRegister(index);

    writeRegister(regTremolo + modulator, patch.modulatorTremolo);
    writeRegister(regAttack + modulator, patch.modulatorAttack);
    writeRegister(regSustain + modulator, patch.modulatorSustain);
    writeRegister(regWaveform + modulator, patch.modulatorWaveform);

    writeRegister(regTremolo + carrier, patch.carrierTremolo);
    writeRegister(regAttack + carrier, patch.carrierAttack);
    writeRegister(regSustain + carrier, patch.carrierSustain);
    writeRegister(regWaveform + carrier, patch.carrierWaveform);
}

void OplPlayer::applyVolume(int index)
{
    const auto& voice = voices[index];

    if (voice.patch == nullptr)
        return;

    const auto& channel = channels[voice.channel];
    const auto& patch = *voice.patch;

    auto gain = (voice.velocity / 127.0) * (channel.volume / 127.0)
                * (channel.expression / 127.0);
    auto added = attenuationFor(gain);

    auto level = [&](std::uint8_t patchLevel, std::uint8_t scale)
    {
        auto attenuation =
            std::min((patchLevel & maxAttenuation) + added, maxAttenuation);

        // The key-scale bits share the register with the level and belong to the
        // patch, so they are carried through rather than recomputed.
        return (scale & 0xc0) | attenuation;
    };

    writeRegister(regLevel + carrierRegister(index),
                  level(patch.carrierLevel, patch.carrierScale));

    // In FM the modulator shapes the carrier rather than sounding on its own, so
    // attenuating it would change the timbre instead of the volume. Only when the
    // two operators are added does it carry part of the level.
    if (patch.isAdditive())
        writeRegister(regLevel + modulatorRegister(index),
                      level(patch.modulatorLevel, patch.modulatorScale));
    else
        writeRegister(regLevel + modulatorRegister(index),
                      (patch.modulatorScale & 0xc0)
                          | (patch.modulatorLevel & maxAttenuation));

    auto pan = panLeftBit | panRightBit;

    if (channel.pan < panLeftLimit)
        pan = panLeftBit;
    else if (channel.pan > panRightLimit)
        pan = panRightBit;

    writeRegister(regFeedback + channelRegister(index), patch.feedback | pan);
}

void OplPlayer::applyFrequency(int index, bool keyOn)
{
    const auto& voice = voices[index];

    if (voice.instrument == nullptr || voice.patch == nullptr)
        return;

    const auto& channel = channels[voice.channel];

    // A fixed-pitch instrument ignores the note it was played with - every drum,
    // and two of the melodic patches.
    auto note =
        voice.instrument->isFixedPitch() ? voice.instrument->fixedNote : voice.note;

    auto pitch = static_cast<double>(note) + voice.patch->baseNoteOffset;

    // The second voice of a doubled instrument is detuned against the first;
    // 128 is no detune. Without it the pair would merely be louder.
    if (voice.second)
        pitch += (voice.instrument->fineTuning - 128) / 64.0;

    pitch += (channel.bend - 8192) / 8192.0 * pitchBendSemitones;

    auto frequency = frequencyFor(pitch);
    auto reg = channelRegister(index);

    writeRegister(regFrequencyLow + reg, frequency.fnum & 0xff);
    writeRegister(regKeyOn + reg,
                  ((frequency.fnum >> 8) & 0x03) | (frequency.block << 2)
                      | (keyOn ? 0x20 : 0x00));
}

void OplPlayer::noteOn(int channel, int note, int velocity)
{
    if (velocity <= 0)
    {
        // Running-status note-off: a note-on at zero velocity is how most MIDI
        // sources release a note, and DOOM's MUS reader emits it too.
        noteOff(channel, note);
        return;
    }

    const auto& instrument = channel == percussionChannel
                                 ? bank.percussion(note)
                                 : bank.melodic(channels[channel].program);

    auto sounding = instrument.isDoubleVoice() ? 2 : 1;

    for (auto half = 0; half < sounding; ++half)
    {
        auto index = allocateVoice();

        if (index < 0)
            return;

        auto& voice = voices[index];

        voice.active = true;
        voice.sustained = false;
        voice.channel = channel;
        voice.note = note;
        voice.velocity = velocity;
        voice.order = allocationOrder++;
        voice.instrument = &instrument;
        voice.patch = &instrument.voices[half];
        voice.second = half == 1;

        programVoice(index, *voice.patch);
        applyVolume(index);
        applyFrequency(index, true);
    }
}

void OplPlayer::noteOff(int channel, int note)
{
    for (auto index = 0; index < voiceCount; ++index)
    {
        auto& voice = voices[index];

        if (!voice.active || voice.sustained || voice.channel != channel
            || voice.note != note)
            continue;

        if (channels[channel].sustain)
            voice.sustained = true;
        else
            releaseVoice(index);
    }
}

void OplPlayer::controllerChange(int channel, int controller, int value)
{
    auto& state = channels[channel];

    switch (controller)
    {
        case ccVolume:
            state.volume = value;
            break;

        case ccExpression:
            state.expression = value;
            break;

        case ccPan:
            state.pan = value;
            break;

        case ccSustain:
        {
            state.sustain = value >= 64;

            if (state.sustain)
                return;

            // The pedal came up: everything it was holding is released now.
            for (auto index = 0; index < voiceCount; ++index)
                if (voices[index].active && voices[index].sustained
                    && voices[index].channel == channel)
                    releaseVoice(index);

            return;
        }

        case ccResetAllControllers:
            state.volume = 127;
            state.expression = 127;
            state.pan = 64;
            state.bend = 8192;
            state.sustain = false;
            break;

        case ccAllNotesOff:
        case ccAllSoundsOff:
        {
            for (auto index = 0; index < voiceCount; ++index)
                if (voices[index].active && voices[index].channel == channel)
                    releaseVoice(index);

            return;
        }

        default:
            // Modulation, bank select, reverb, chorus and the rest: the engine
            // emits them, the OPL has nowhere to put them.
            return;
    }

    // Volume, expression, pan and a controller reset all change how the voices
    // already sounding on this channel should be heard.
    for (auto index = 0; index < voiceCount; ++index)
        if (voices[index].active && voices[index].channel == channel)
            applyVolume(index);

    if (controller == ccResetAllControllers)
        for (auto index = 0; index < voiceCount; ++index)
            if (voices[index].active && voices[index].channel == channel)
                applyFrequency(index, true);
}

void OplPlayer::handleMessage(unsigned long message)
{
    if (!running)
        return;

    auto status = static_cast<int>(message & 0xff);

    if (status < 0x80)
        return;

    auto command = status & 0xf0;
    auto channel = status & 0x0f;
    auto first = static_cast<int>((message >> 8) & 0x7f);
    auto second = static_cast<int>((message >> 16) & 0x7f);

    switch (command)
    {
        case 0x80:
            noteOff(channel, first);
            break;

        case 0x90:
            noteOn(channel, first, second);
            break;

        case 0xb0:
            controllerChange(channel, first, second);
            break;

        case 0xc0:
            channels[channel].program = first;
            break;

        case 0xe0:
        {
            channels[channel].bend = first | (second << 7);

            for (auto index = 0; index < voiceCount; ++index)
                if (voices[index].active && voices[index].channel == channel)
                    applyFrequency(index, true);

            break;
        }

        default:
            // Aftertouch and system messages. DOOM's MUS reader emits neither.
            break;
    }
}

void OplPlayer::allNotesOff()
{
    if (!running)
        return;

    for (auto index = 0; index < voiceCount; ++index)
        releaseVoice(index);
}

void OplPlayer::render(std::span<float> out)
{
    auto frames = static_cast<int>(out.size() / 2);

    if (!running || frames <= 0)
    {
        std::fill(out.begin(), out.end(), 0.0f);
        return;
    }

    if (scratch.size() < frames * 2)
        scratch.resize(frames * 2);

    OPL3_GenerateStream(
        chip.get(), scratch.data(), static_cast<std::uint32_t>(frames));

    constexpr auto scale = 1.0f / 32768.0f;

    for (auto i = 0; i < frames * 2; ++i)
        out[i] = static_cast<float>(scratch[i]) * scale;
}
} // namespace PureDoom
