#pragma once

#include <MakeASound/MakeASound.h>

#include <DOOM/DOOM.h>

#include <atomic>

namespace PureDoom
{
// One frame of the finished mix, as the device wants it.
struct StereoFrame
{
    float left = 0.0f;
    float right = 0.0f;
};

// DOOM's whole mixing granularity: Doom::soundBuffer() mixes exactly this many
// stereo frames at Doom::DOOM_SAMPLERATE, all at once, and there is no smaller
// unit to ask it for. 512 frames at 11025 Hz is 46ms - longer than a tic - so it
// is the floor under the latency of everything below.
inline constexpr auto doomBlockFrames = 512;

// How far ahead of the device the producer keeps the queue. It runs once a
// display refresh (8-17ms) and the device asks for a block of its own, so this
// has to cover both with room to spare; DOOM's own block granularity sits on top
// of it.
inline constexpr auto queueHeadroomMs = 40;

// The queue's capacity is a compile-time size, so it is sized for the fastest
// device a stream will be opened at. A device preferring more than this is
// opened at the highest rate it offers below it instead.
inline constexpr auto maxSampleRate = 192000;

// The headroom above, plus the one indivisible DOOM block that tops it up: the
// producer stops pulling the moment the queue holds the headroom, so this is the
// most that can ever be in flight.
inline constexpr auto audioQueueFrames =
    maxSampleRate * queueHeadroomMs / 1000
    + (doomBlockFrames * maxSampleRate + Doom::DOOM_SAMPLERATE - 1)
          / Doom::DOOM_SAMPLERATE
    + 1;

// What the virtual MIDI port calls itself in the system's MIDI graph.
inline constexpr auto virtualMidiPortName = "PureDOOM";

// DOOM's audio, driven through MakeASound.
//
// Sound effects are a pull and music is a push, and they are on different
// clocks. Doom::soundBuffer() mixes the next block of samples out of whatever
// the playsim has started, each time it is called. Doom::tickMidi() wants
// draining Doom::DOOM_MIDI_RATE times a second and hands back MIDI messages
// rather than samples - MakeASound carries those to a port, but has no synth of
// its own, so the notes leave the process to be heard.
//
// The mixer is pulled on the main thread rather than from the device's callback.
// Doom::soundBuffer() reaches into the same engine state Doom::updateGame()
// writes, so pulling it from the audio thread would mean holding a lock across
// the mix, on the audio thread. Pulling it from the thread that steps the engine
// removes the lock outright, and what crosses to the audio thread is a wait-free
// queue of frames that are already finished.
struct Audio
{
    Audio();
    ~Audio();

    // Once a display refresh, ahead of anything that might return early: the
    // device wants feeding whether or not a tic is due.
    void pump();

    void openDevice();
    void openMidi();

    // The audio thread's half, and the only member function on it.
    void render(MakeASound::AudioCallbackInfo& info);

    void pumpSound();
    void pumpMidi();

    // Resamples one Doom::soundBuffer() block into the queue.
    void pushBlock(const short* block);

    void sendMidi(unsigned long message);

    MakeASound::DeviceManager devices;
    MakeASound::MidiManager midi;
    MakeASound::SPSCQueue<StereoFrame, audioQueueFrames> frames;

    // What the producer believes is in the queue, which the queue itself does
    // not report. The consumer only ever publishes a count at or above the true
    // one, so a stale reading makes the producer wait a refresh - it cannot make
    // it overrun.
    std::atomic<int> queued {0};

    int sampleRate = 0;
    int targetFrames = 0;
    bool deviceOpen = false;
    bool midiOpen = false;

    // Linear interpolation from DOOM's rate to the device's. `phase` is where
    // the next output frame falls between `previousFrame` and the source frame
    // being read, and both carry across the block boundary - the resampler never
    // restarts, or every block would join with a step in it.
    float step = 1.0f;
    float phase = 0.0f;
    StereoFrame previousFrame;

    bool midiStarted = false;
    double lastMidiTick = 0.0;
};
} // namespace PureDoom
