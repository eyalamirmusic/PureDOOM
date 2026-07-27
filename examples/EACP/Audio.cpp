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

// One MUS tick can hand back a run of messages. The cap is a backstop against a
// malformed score, not a real limit - a tick never legitimately reaches it.
constexpr auto maxMidiMessagesPerTick = 256;

// After a stall - a level load, a window drag - the music resyncs to the clock
// rather than replaying at speed everything it missed.
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
    openMidi();
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

void Audio::openMidi()
{
    // A virtual port is the polite answer: it appears in the system's MIDI graph
    // for the player to route into a synth of their choosing, and it plays to
    // nothing at all until they do.
    try
    {
        midi.openVirtualOutput(virtualMidiPortName);
        midiOpen = true;

        report(std::string("PureDoom: music on the virtual MIDI port \"")
               + virtualMidiPortName + "\"; route it into a synth to hear it\n");
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
        midiOpen = true;

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
    pumpSound();
    pumpMidi();
}

void Audio::pumpSound()
{
    if (!deviceOpen)
        return;

    // Top up in whole DOOM blocks - the mixer has no smaller unit - until the
    // queue holds the headroom the device needs to reach the next refresh.
    while (queued.load(std::memory_order_acquire) < targetFrames)
        pushBlock(Doom::soundBuffer());
}

void Audio::pushBlock(const short* block)
{
    auto pushed = 0;

    for (auto sample = 0; sample < doomBlockFrames; ++sample)
    {
        auto next = StereoFrame {(float) block[sample * 2] * sampleScale,
                                 (float) block[sample * 2 + 1] * sampleScale};

        while (phase < 1.0f)
        {
            auto blend = StereoFrame {
                previousFrame.left + (next.left - previousFrame.left) * phase,
                previousFrame.right + (next.right - previousFrame.right) * phase};

            if (frames.push(blend))
                ++pushed;

            phase += step;
        }

        phase -= 1.0f;
        previousFrame = next;
    }

    queued.fetch_add(pushed, std::memory_order_release);
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
        if (frames.pop(frame))
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

void Audio::pumpMidi()
{
    if (!midiOpen)
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
