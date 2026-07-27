#pragma once

#include "OplPlayer.h"

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
// is the floor under the latency of a sound effect. It waits in a staging buffer
// rather than in the queue, so it does not size the queue.
inline constexpr auto doomBlockFrames = 512;

// How far ahead of the device the producer keeps the queue. It runs once a
// display refresh (8-17ms) and the device asks for a block of its own, so this
// has to cover both with room to spare.
inline constexpr auto queueHeadroomMs = 40;

// The queue's capacity is a compile-time size, so it is sized for the fastest
// device a stream will be opened at. A device preferring more than this is
// opened at the highest rate it offers below it instead.
inline constexpr auto maxSampleRate = 192000;

// The headroom above, plus the one chunk that tops it up - the producer stops
// the moment the queue holds the headroom, so this is the most that can ever be
// in flight. A chunk is one MIDI tick, which is what makes it far smaller than
// DOOM's own 46ms block.
inline constexpr auto audioQueueFrames =
    maxSampleRate * queueHeadroomMs / 1000
    + (maxSampleRate + Doom::DOOM_MIDI_RATE - 1) / Doom::DOOM_MIDI_RATE + 1;

// What the virtual MIDI port calls itself in the system's MIDI graph, on the
// fallback path where the music leaves the process.
inline constexpr auto virtualMidiPortName = "PureDOOM";

// DOOM's audio, driven through MakeASound.
//
// Sound effects are a pull: Doom::soundBuffer() mixes the next block of samples
// out of whatever the playsim has started, each time it is called. Music is a
// push: Doom::tickMidi() hands back MIDI messages Doom::DOOM_MIDI_RATE times a
// second, and OplPlayer turns them into sound with the IWAD's own patches.
//
// **The mixer is pulled on the main thread**, not from the device's callback.
// Doom::soundBuffer() reaches into the same engine state Doom::updateGame()
// writes, so pulling it from the audio thread would mean holding a lock across
// the mix, on the audio thread. Pulling it from the thread that steps the engine
// removes the lock outright, and what crosses to the audio thread is a wait-free
// queue of frames that are already finished. The synth is emulated rather than a
// device, so it renders on the same side of that boundary.
//
// **The producer's clock is the stream, not the wall.** Frames are produced one
// MIDI tick at a time, and each tick's register writes land at the start of the
// frames they affect. Timing the music by the display's refresh instead would
// scatter it by up to a whole tick, because the producer runs ahead of the
// device by design.
struct Audio
{
    Audio();
    ~Audio();

    // Once a display refresh, ahead of anything that might return early: the
    // device wants feeding whether or not a tic is due.
    void pump();

    void openDevice();
    void openMusic();
    void openMidiPort();

    // The audio thread's half, and the only member function on it.
    void render(MakeASound::AudioCallbackInfo& info);

    void produce();

    // One MIDI tick's worth of device frames: the music for it, the sound
    // effects that line up with it, mixed and pushed.
    void produceChunk();

    // Drains this tick's MIDI messages into the synth.
    void advanceMusic();

    // The next resampled sound-effect frame, pulling and resampling another DOOM
    // block when the staging buffer runs dry.
    StereoFrame nextSoundFrame();

    // Resamples one Doom::soundBuffer() block into the staging buffer.
    void stageBlock(const short* block);

    // The fallback music path: MIDI out of the process, on the wall clock, for a
    // WAD that carries no GENMIDI bank.
    void pumpMidiPort();
    void sendMidi(unsigned long message);

    MakeASound::DeviceManager devices;
    MakeASound::MidiManager midi;
    // Named for what it is rather than what it holds: `frames` would shadow the
    // frame count every producer function computes.
    MakeASound::SPSCQueue<StereoFrame, audioQueueFrames> queue;

    OplPlayer opl;

    // What the producer believes is in the queue, which the queue itself does
    // not report. The consumer only ever publishes a count at or above the true
    // one, so a stale reading makes the producer wait a refresh - it cannot make
    // it overrun.
    std::atomic<int> queued {0};

    int sampleRate = 0;
    int targetFrames = 0;
    bool deviceOpen = false;
    bool musicIsOpl = false;
    bool midiPortOpen = false;

    // Sound effects resampled to the device's rate, waiting to be mixed. A DOOM
    // block is indivisible and far longer than a chunk, so it is drained across
    // several of them.
    Vector<StereoFrame> staging;
    int stagingRead = 0;

    // The synth's output for the chunk being built, interleaved.
    Vector<float> musicScratch;

    // Linear interpolation from DOOM's rate to the device's. `phase` is where
    // the next output frame falls between `previousFrame` and the source frame
    // being read, and both carry across the block boundary - the resampler never
    // restarts, or every block would join with a step in it.
    float step = 1.0f;
    float phase = 0.0f;
    StereoFrame previousFrame;

    // Carries the fraction of a frame a MIDI tick is worth, so 140 chunks come to
    // exactly one second's frames rather than drifting by the remainder.
    int chunkRemainder = 0;

    bool midiStarted = false;
    double lastMidiTick = 0.0;
};
} // namespace PureDoom
