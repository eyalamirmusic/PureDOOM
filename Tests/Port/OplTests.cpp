// The OPL music player, driven headlessly.
//
// This is the first time anything in the suite has been able to assert on
// *sound*. It works for the same reason Tests/Port can drive the geometry
// builder: the chip is emulated, so rendering is arithmetic rather than a
// device - the player produces exactly the frames asked for, when asked, with no
// audio hardware and no timing.
//
// What that buys is the part of the audio path that was previously unreachable.
// The mixer and the MUS reader still have no gate (nothing hashes a sample, and
// the two defects fixed while wiring audio both survived that blind spot), but
// the synth on top of them now does: a note either makes a sound or it does not,
// and a released note either stops or it does not.
//
// These are deliberately property checks rather than a golden. A golden over
// rendered samples would pin Nuked-OPL3's exact output, which is upstream code
// this repository does not own and must not be measuring.

#include "../Common.h"

#include <EngineAccess.h>
#include <OplPlayer.h>

#include <cmath>
#include <vector>

using namespace nano;
using namespace PureDoom;

namespace
{
constexpr int sampleRate = 48000;

// Long enough for an attack to get going at any patch's envelope rate, and short
// enough that a dozen of them cost nothing.
constexpr int blockFrames = 4096;

// A note-on with a General MIDI program the bank definitely voices.
constexpr unsigned long noteOn(int channel, int note, int velocity)
{
    return 0x90ul | (unsigned long) channel | ((unsigned long) note << 8)
           | ((unsigned long) velocity << 16);
}

constexpr unsigned long noteOff(int channel, int note)
{
    return 0x80ul | (unsigned long) channel | ((unsigned long) note << 8);
}

constexpr unsigned long controller(int channel, int number, int value)
{
    return 0xb0ul | (unsigned long) channel | ((unsigned long) number << 8)
           | ((unsigned long) value << 16);
}

struct Rendered
{
    float peak = 0.0f;
    float left = 0.0f;
    float right = 0.0f;
};

Rendered render(OplPlayer& player, int frames = blockFrames)
{
    auto buffer = std::vector<float>(static_cast<std::size_t>(frames) * 2, 0.0f);
    player.render(buffer);

    auto result = Rendered {};

    for (auto i = 0; i < frames; ++i)
    {
        auto l = std::fabs(buffer[static_cast<std::size_t>(i) * 2]);
        auto r = std::fabs(buffer[static_cast<std::size_t>(i) * 2 + 1]);

        result.left = std::max(result.left, l);
        result.right = std::max(result.right, r);
        result.peak = std::max(result.peak, std::max(l, r));
    }

    return result;
}

// Above the noise floor of an emulated chip that is meant to be idle. The chip
// outputs exact zeroes when nothing is keyed, so this is generous.
constexpr float audible = 0.001f;

bool startPlayer(OplPlayer& player)
{
    return player.start(sampleRate, Engine::genmidiLump());
}

auto tOplStarts = test("Port/oplStartsFromTheWadsBank") = []
{
    check(doomSimBoot() != 0, "the engine booted");

    auto player = OplPlayer {};

    check(startPlayer(player), "the player started from the IWAD's GENMIDI");
    check(player.isRunning(), "and reports itself running");

    // Nothing has been played, so the chip must be silent - which is also what
    // makes every "it made a sound" check below mean something.
    check(render(player).peak < audible, "an idle chip is silent");
};

auto tOplRefusesABadBank = test("Port/oplRefusesABadBank") = []
{
    auto player = OplPlayer {};

    check(!player.start(sampleRate, {}), "no bank, no player");
    check(!player.isRunning(), "and it says so");

    // A player that never started must still be safe to drive and to render.
    player.handleMessage(noteOn(0, 60, 100));

    check(render(player).peak == 0.0f, "a stopped player renders silence");
};

auto tOplNoteSounds = test("Port/oplNoteSounds") = []
{
    check(doomSimBoot() != 0, "the engine booted");

    auto player = OplPlayer {};

    check(startPlayer(player), "the player started");

    player.handleMessage(noteOn(0, 60, 127));

    check(player.activeVoices() > 0, "the note took a voice");
    check(render(player).peak > audible, "and the chip made a sound");
};

auto tOplNoteOffReleases = test("Port/oplNoteOffReleases") = []
{
    check(doomSimBoot() != 0, "the engine booted");

    auto player = OplPlayer {};

    check(startPlayer(player), "the player started");

    player.handleMessage(noteOn(0, 60, 127));
    render(player);

    player.handleMessage(noteOff(0, 60));

    check(player.activeVoices() == 0, "the note gave its voice back");

    // The release is an envelope, not a cut, so the sound has to be allowed to
    // decay before silence is a fair thing to ask for.
    for (auto block = 0; block < 8; ++block)
        render(player);

    check(render(player).peak < audible, "and the sound decayed to silence");
};

auto tOplZeroVelocityIsANoteOff = test("Port/oplZeroVelocityReleases") = []
{
    check(doomSimBoot() != 0, "the engine booted");

    auto player = OplPlayer {};

    check(startPlayer(player), "the player started");

    player.handleMessage(noteOn(0, 60, 100));

    check(player.activeVoices() > 0, "the note took a voice");

    // Every MIDI source releases notes this way at least some of the time, and
    // treating it as a note-on at silent volume leaves the voice stuck on
    // forever - which is audible as a synth that fills up and then goes quiet.
    player.handleMessage(noteOn(0, 60, 0));

    check(player.activeVoices() == 0, "a zero-velocity note-on released it");
};

auto tOplStealsVoices = test("Port/oplStealsVoices") = []
{
    check(doomSimBoot() != 0, "the engine booted");

    auto player = OplPlayer {};

    check(startPlayer(player), "the player started");

    // Far more notes than the chip has voices for. The point is not that they all
    // sound - they cannot - but that asking for them never exceeds the chip's 18
    // channels, and never wedges the allocator.
    for (auto note = 36; note < 96; ++note)
        player.handleMessage(noteOn(0, note, 100));

    check(player.activeVoices() > 0, "voices are in use");
    check(player.activeVoices() <= OplPlayer::voiceCount,
          "and never more than the chip has");
    check(render(player).peak > audible, "the chip is still sounding");

    player.handleMessage(controller(0, 123, 0));

    check(player.activeVoices() == 0, "all-notes-off cleared every voice");
};

auto tOplDoubleVoiceInstrumentsTakeTwo = test("Port/oplDoubleVoiceTakesTwo") = []
{
    check(doomSimBoot() != 0, "the engine booted");

    auto bank = GenmidiBank {};

    check(bank.read(Engine::genmidiLump()), "the bank decoded");

    // Find a program the bank marks as doubled - 32 of the 128 are - so this
    // follows the WAD rather than hard-coding an instrument number.
    auto doubled = -1;

    for (auto program = 0; program < GenmidiBank::melodicCount && doubled < 0;
         ++program)
        if (bank.melodic(program).isDoubleVoice())
            doubled = program;

    check(doubled >= 0, "the bank has a doubled instrument");

    auto player = OplPlayer {};

    check(startPlayer(player), "the player started");

    player.handleMessage(0xc0ul | ((unsigned long) doubled << 8));
    player.handleMessage(noteOn(0, 60, 127));

    check(player.activeVoices() == 2, "one note took both of its voices");

    player.handleMessage(noteOff(0, 60));

    check(player.activeVoices() == 0, "and gave both back");
};

auto tOplPercussionSounds = test("Port/oplPercussionSounds") = []
{
    check(doomSimBoot() != 0, "the engine booted");

    auto player = OplPlayer {};

    check(startPlayer(player), "the player started");

    // The drum channel selects its instrument by note rather than by program, so
    // this reaches a different half of the bank than every check above.
    player.handleMessage(
        noteOn(OplPlayer::percussionChannel, GenmidiBank::firstPercussionNote, 127));

    check(player.activeVoices() > 0, "the drum took a voice");
    check(render(player).peak > audible, "and made a sound");
};

auto tOplPansAcrossTheStereoField = test("Port/oplPans") = []
{
    check(doomSimBoot() != 0, "the engine booted");

    auto player = OplPlayer {};

    check(startPlayer(player), "the player started");

    // Hard left. An OPL3 pans by enabling one output or the other, so this is
    // exact rather than a balance - and it is the one check that would catch the
    // pan bits being written into the wrong register, which is otherwise
    // inaudible on a mono listen.
    player.handleMessage(controller(0, 10, 0));
    player.handleMessage(noteOn(0, 60, 127));

    auto left = render(player);

    check(left.left > audible, "the left channel sounds");
    check(left.right < audible, "and the right is silent");

    player.handleMessage(controller(0, 123, 0));

    for (auto block = 0; block < 8; ++block)
        render(player);

    player.handleMessage(controller(0, 10, 127));
    player.handleMessage(noteOn(0, 62, 127));

    auto right = render(player);

    check(right.right > audible, "hard right sounds on the right");
    check(right.left < audible, "and is silent on the left");
};

auto tOplVolumeChangesLoudness = test("Port/oplVolumeChangesLoudness") = []
{
    check(doomSimBoot() != 0, "the engine booted");

    auto player = OplPlayer {};

    check(startPlayer(player), "the player started");

    player.handleMessage(controller(0, 7, 127));
    player.handleMessage(noteOn(0, 60, 127));

    auto loud = render(player).peak;

    player.handleMessage(controller(0, 123, 0));

    for (auto block = 0; block < 8; ++block)
        render(player);

    player.handleMessage(controller(0, 7, 16));
    player.handleMessage(noteOn(0, 60, 127));

    auto quiet = render(player).peak;

    check(loud > audible, "the loud note sounded");
    check(quiet < loud, "and channel volume made the same note quieter");
};
} // namespace
