#pragma once

#include "Genmidi.h"

#include <cstdint>
#include <span>

// The emulated chip, forward-declared by its own tag rather than by the
// `opl3_chip` typedef the header spells it with - a typedef cannot be forward
// declared, and declaring the name as a struct collides with it.
struct _opl3_chip;

namespace PureDoom
{
// DOOM's music, played the way it was written: an emulated OPL3 driven by the
// IWAD's own GENMIDI patches.
//
// The engine hands out MIDI messages (Doom::tickMidi), not samples, so something
// has to turn notes into sound. A General MIDI synth needs a sound bank shipped
// alongside it; this needs nothing, because DOOM's music was composed for the
// Adlib and the instrument definitions are already in the WAD. It is also what
// the music actually sounded like.
//
// Everything here happens on the producer thread - the same one that steps the
// engine and pulls the sound mixer. The chip is emulated, so "render" is
// arithmetic, not a device: it produces exactly the frames asked for, when asked.
class OplPlayer
{
public:
    // An OPL3 is two banks of nine 2-operator channels. A note costs one, or two
    // when its instrument is doubled, so this is the polyphony ceiling before
    // stealing begins.
    static constexpr int voiceCount = 18;
    static constexpr int channelsPerBank = 9;

    // General MIDI's drum channel. The engine already maps MUS's percussion
    // channel onto it (Host/Sound.cpp swaps 15 and 9), so by the time a message
    // reaches here the convention is GM's.
    static constexpr int percussionChannel = 9;

    static constexpr int midiChannels = 16;

    // Pitch bend covers two semitones either way, which is General MIDI's default
    // and what DOOM's own MUS conversion assumes.
    static constexpr double pitchBendSemitones = 2.0;

    OplPlayer();
    ~OplPlayer();

    OplPlayer(const OplPlayer&) = delete;
    OplPlayer& operator=(const OplPlayer&) = delete;

    // False, and silent, if the bank will not decode - a WAD with no GENMIDI, or
    // a damaged one. The caller then has no music path and should say so rather
    // than play nothing quietly.
    bool start(int sampleRateToUse, std::span<const std::uint8_t> genmidi);

    bool isRunning() const { return running; }

    // One packed message as Doom::tickMidi() returns it: status in the low byte,
    // then one or two data bytes. Anything it does not recognise is ignored
    // rather than guessed at.
    void handleMessage(unsigned long message);

    // Writes interleaved left/right pairs, replacing whatever was there;
    // `out.size()` must be even and is taken as `size() / 2` frames.
    void render(std::span<float> out);

    // Silences every voice without forgetting the channel state. What a level
    // change does.
    void allNotesOff();

    int activeVoices() const;

private:
    struct Channel
    {
        int program = 0;
        int volume = 127;
        int expression = 127;
        int pan = 64;
        int bend = 8192; // 14-bit, centred
        bool sustain = false;
    };

    struct Voice
    {
        bool active = false;
        bool sustained = false; // released, but held by the sustain pedal
        int channel = 0;
        int note = 0; // the note that started it, for matching note-off
        int velocity = 0;
        int order = 0; // allocation order, so the oldest is stolen first
        const OplInstrument* instrument = nullptr;
        const OplVoice* patch = nullptr;
        bool second = false; // the doubled half of a two-voice instrument
    };

    void writeRegister(int reg, int value);
    void resetChip();

    void noteOn(int channel, int note, int velocity);
    void noteOff(int channel, int note);
    void controllerChange(int channel, int controller, int value);
    void releaseVoice(int index);

    // -1 when every voice is busy and none can be stolen, which cannot happen -
    // stealing always succeeds - but the callers are written not to assume it.
    int allocateVoice();

    void programVoice(int index, const OplVoice& patch);
    void applyVolume(int index);
    void applyFrequency(int index, bool keyOn);

    int modulatorRegister(int index) const;
    int carrierRegister(int index) const;
    int channelRegister(int index) const;

    GenmidiBank bank;

    // Out of line, so the deleter sees the complete chip type in the .cpp - the
    // same pimpl shape MakeASound's facades use.
    OwningPointer<_opl3_chip> chip;

    Array<Channel, midiChannels> channels;
    Array<Voice, voiceCount> voices;

    // The chip emits signed 16-bit pairs; this is the only buffer between it and
    // the caller's floats, and it is kept rather than allocated per block.
    Vector<std::int16_t> scratch;

    int sampleRate = 0;
    int allocationOrder = 0;
    bool running = false;
};
} // namespace PureDoom
