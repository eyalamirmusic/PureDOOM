#include "Audio.h"

#include "EngineAccess.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <string>

namespace PureDoom
{
namespace
{
// DOOM mixes signed 16-bit.
constexpr auto sampleScale = 1.0f / 32768.0f;

// The synth against the sound effects, measured over an attract demo rather than
// guessed: the OPL peaks around 0.28 of full scale where DOOM's mixer reaches
// 0.82, so this leaves the two summing to about full scale at their loudest and
// the clamp below to catch the rest. DOOM's own music volume setting is *not*
// here - the engine applies it as MIDI channel volume, which the synth honours
// like any other.
constexpr auto musicGain = 0.8f;

// One MUS tick can hand back a run of messages. The cap is a backstop against a
// malformed score, not a real limit - a tick never legitimately reaches it.
constexpr auto maxMidiMessagesPerTick = 256;

// After a stall - a level load, a window drag - the music resyncs to the clock
// rather than replaying at speed everything it missed. Only the MIDI-port path
// needs this; the synth is paced by the stream, which cannot stall.
constexpr auto maxMidiTicksPerPump = 16;

void report(std::string_view text)
{
    Doom::host().print(text);
}

int chooseSampleRate(const MakeASound::DeviceInfo& output)
{
    if (output.preferredSampleRate > 0
        && output.preferredSampleRate <= maxSampleRate)
        return output.preferredSampleRate;

    auto best = 0;

    for (auto rate: output.sampleRates)
        if (rate > best && rate <= maxSampleRate)
            best = rate;

    // A device that reports no rates at all still has one; miniaudio converts to
    // whatever it is, so asking for the commonest is better than not opening.
    if (best == 0)
        best = 48000;

    return best;
}

bool namesASynth(const std::string& name)
{
    auto lowered = name;
    std::ranges::transform(lowered,
                           lowered.begin(),
                           [](unsigned char c) { return (char) std::tolower(c); });

    return lowered.find("synth") != std::string::npos
           || lowered.find("wavetable") != std::string::npos;
}
} // namespace

Audio::Audio()
{
    openDevice();
    openMusic();
}

Audio::~Audio()
{
    // Explicitly, and first: members are destroyed in reverse order of
    // declaration, so `devices` - and the callback thread it owns - would
    // otherwise outlive the queue the callback reads.
    devices.stop();
}

void Audio::openDevice()
{
    try
    {
        auto output = devices.getDefaultOutputDevice();

        if (output.outputChannels <= 0)
        {
            report("PureDoom: no audio output device; the game runs silent\n");
            return;
        }

        sampleRate = chooseSampleRate(output);

        // Built by hand rather than taken from getDefaultConfig(), which also
        // opens the default *input* - and asking for a capture device is what
        // puts a microphone permission prompt in front of a game that never
        // records anything.
        auto config = MakeASound::StreamConfig {};
        config.output = MakeASound::StreamParameters(output, false, 2);
        config.sampleRate = sampleRate;
        config.maxBlockSize = doomBlockFrames;
        config.options = MakeASound::StreamOptions {};

        devices.start(config,
                      [this](MakeASound::AudioCallbackInfo& info) { render(info); });

        // The device may have settled on a rate other than the one asked for.
        auto opened = devices.getStreamSampleRate();

        if (opened > 0)
            sampleRate = opened;

        step = (float) Doom::DOOM_SAMPLERATE / (float) sampleRate;
        targetFrames = sampleRate * queueHeadroomMs / 1000;
        deviceOpen = true;

        report("PureDoom: audio on " + output.name + " at "
               + std::to_string(sampleRate) + " Hz\n");
    }
    catch (const std::exception& error)
    {
        report(std::string("PureDoom: could not open an audio device: ")
               + error.what() + "\n");
    }
}

void Audio::openMusic()
{
    // The OPL first, and it needs nothing shipped with it: DOOM's music was
    // written for the Adlib, and the instrument definitions are in the GENMIDI
    // lump of the WAD the game already booted from.
    if (deviceOpen && opl.start(sampleRate, Engine::genmidiLump()))
    {
        musicIsOpl = true;

        report("PureDoom: music on an emulated OPL3, voiced from the IWAD's "
               "GENMIDI bank\n");
        return;
    }

    // No bank, or nowhere to render it to. The score can still leave the process
    // as MIDI for something else to voice.
    openMidiPort();
}

void Audio::openMidiPort()
{
    // A virtual port is the polite answer: it appears in the system's MIDI graph
    // for the player to route into a synth of their choosing, and it plays to
    // nothing at all until they do.
    try
    {
        midi.openVirtualOutput(virtualMidiPortName);
        midiPortOpen = true;

        report(
            std::string("PureDoom: no OPL bank; music on the virtual MIDI port \"")
            + virtualMidiPortName + "\"\n");
        return;
    }
    catch (const std::exception&)
    {
        // RtMidi offers no virtual ports on Windows - which is also the one
        // platform that ships a General MIDI synth as an ordinary output port,
        // so a real port is the fallback rather than a lesser option.
    }

    try
    {
        auto ports = midi.getOutputPorts();

        if (ports.empty())
        {
            report("PureDoom: no MIDI output port; the music is not played\n");
            return;
        }

        auto chosen = ports[0];

        for (const auto& port: ports)
        {
            if (namesASynth(port.name))
            {
                chosen = port;
                break;
            }
        }

        midi.openOutput(chosen.id);
        midiPortOpen = true;

        report("PureDoom: music to MIDI port \"" + chosen.name + "\"\n");
    }
    catch (const std::exception& error)
    {
        report(std::string("PureDoom: could not open a MIDI output: ") + error.what()
               + "\n");
    }
}

void Audio::pump()
{
    produce();
    pumpMidiPort();
}

void Audio::produce()
{
    if (!deviceOpen)
        return;

    // Top up a chunk at a time until the queue holds the headroom the device
    // needs to reach the next refresh.
    while (queued.load(std::memory_order_acquire) < targetFrames)
        produceChunk();
}

void Audio::produceChunk()
{
    // A MIDI tick is sampleRate/140 frames, which is not a whole number: the
    // remainder is carried so 140 chunks come to exactly one second.
    chunkRemainder += sampleRate;

    auto frames = chunkRemainder / Doom::DOOM_MIDI_RATE;
    chunkRemainder -= frames * Doom::DOOM_MIDI_RATE;

    if (frames <= 0)
        return;

    // The score advances first, so its register writes take effect from the start
    // of the frames they belong to.
    advanceMusic();

    if (musicIsOpl)
    {
        if (musicScratch.size() < frames * 2)
            musicScratch.resize(frames * 2);

        opl.render({musicScratch.data(), static_cast<std::size_t>(frames) * 2});
    }

    auto pushed = 0;

    for (auto i = 0; i < frames; ++i)
    {
        auto frame = nextSoundFrame();

        if (musicIsOpl)
        {
            frame.left += musicScratch[i * 2] * musicGain;
            frame.right += musicScratch[i * 2 + 1] * musicGain;
        }

        // Two independent sources summed can exceed full scale where neither
        // did; clamping is what keeps that a loud moment rather than a wrap.
        frame.left = std::clamp(frame.left, -1.0f, 1.0f);
        frame.right = std::clamp(frame.right, -1.0f, 1.0f);

        if (queue.push(frame))
            ++pushed;
    }

    queued.fetch_add(pushed, std::memory_order_release);
}

void Audio::advanceMusic()
{
    if (!musicIsOpl)
        return;

    for (auto message = 0; message < maxMidiMessagesPerTick; ++message)
    {
        auto midiMessage = Doom::tickMidi();

        if (midiMessage == 0)
            break;

        opl.handleMessage(midiMessage);
    }
}

StereoFrame Audio::nextSoundFrame()
{
    if (stagingRead >= staging.size())
    {
        staging.clear();
        stagingRead = 0;

        // Mixed on the call, on this thread, out of whatever the playsim has
        // started since the last block.
        stageBlock(Doom::soundBuffer());
    }

    if (staging.empty())
        return {};

    return staging[stagingRead++];
}

void Audio::stageBlock(const short* block)
{
    for (auto sample = 0; sample < doomBlockFrames; ++sample)
    {
        auto next = StereoFrame {(float) block[sample * 2] * sampleScale,
                                 (float) block[sample * 2 + 1] * sampleScale};

        while (phase < 1.0f)
        {
            staging.add(StereoFrame {
                previousFrame.left + (next.left - previousFrame.left) * phase,
                previousFrame.right + (next.right - previousFrame.right) * phase});

            phase += step;
        }

        phase -= 1.0f;
        previousFrame = next;
    }
}

void Audio::render(MakeASound::AudioCallbackInfo& info)
{
    auto output = info.getOutput();
    auto channels = output.getNumChannels();

    if (channels <= 0)
        return;

    auto* left = output.getChannelPointer(0);
    auto* right = channels > 1 ? output.getChannelPointer(1) : nullptr;
    auto taken = 0;

    for (auto sample = 0; sample < output.getNumSamples(); ++sample)
    {
        auto frame = StereoFrame {};

        // A frame the producer has not reached yet stays zero: a gap in the
        // queue is silence, which is what an underrun should sound like.
        if (queue.pop(frame))
            ++taken;

        if (right != nullptr)
        {
            left[sample] = frame.left;
            right[sample] = frame.right;
        }
        else
        {
            left[sample] = 0.5f * (frame.left + frame.right);
        }
    }

    queued.fetch_sub(taken, std::memory_order_release);
}

void Audio::pumpMidiPort()
{
    if (!midiPortOpen)
        return;

    auto now = Engine::midiTime();

    if (!midiStarted)
    {
        midiStarted = true;
        lastMidiTick = now;
    }

    auto due = (int) (now - lastMidiTick);

    if (due <= 0)
        return;

    if (due > maxMidiTicksPerPump)
    {
        due = maxMidiTicksPerPump;
        lastMidiTick = now;
    }
    else
    {
        // By whole ticks rather than to `now`, so the fraction left over is the
        // next pump's and the music neither drifts nor loses a tick.
        lastMidiTick += due;
    }

    for (auto tick = 0; tick < due; ++tick)
    {
        for (auto message = 0; message < maxMidiMessagesPerTick; ++message)
        {
            auto midiMessage = Doom::tickMidi();

            if (midiMessage == 0)
                break;

            sendMidi(midiMessage);
        }
    }
}

void Audio::sendMidi(unsigned long message)
{
    // Doom::tickMidi packs a channel message little-end first: status, then the
    // one or two data bytes.
    auto status = (std::uint8_t) (message & 0xff);

    if (status < 0x80)
        return;

    auto bytes = Array<std::uint8_t, 3> {status,
                                         (std::uint8_t) ((message >> 8) & 0x7f),
                                         (std::uint8_t) ((message >> 16) & 0x7f)};

    // Program change and channel pressure carry one data byte; everything else
    // Doom::tickMidi emits carries two.
    auto command = status & 0xf0;
    auto length = (command == 0xc0 || command == 0xd0) ? 2u : 3u;

    midi.sendMessage(bytes.data(), length);
}
} // namespace PureDoom
