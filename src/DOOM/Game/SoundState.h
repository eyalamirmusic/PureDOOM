#pragma once

#include "SoundData.h" // MusicInfo (an anonymous-struct typedef, so it cannot be
// forward-declared - only a pointer is held here regardless); also Doom::SfxInfo

#include "../Containers.h"

// A mixing channel: which sound occupies it (null = available), the sound's origin and
// the handle of the sound being played. This was Game/Sound's file-local struct - a
// forward declaration sufficed while SoundState held only a pointer to the array - but the
// RAII sweep (Step 9) makes SoundState own the channel array by value (Vector<Doom::SoundChannel>),
// which needs the complete type here. Still used by no file but Game/Sound.
namespace Doom
{
// (definition kept here rather than in Game/Sound so SoundState can own it by value)
struct SoundChannel
{
    SfxInfo* sfxinfo; // sound occupying the channel (null = available)
    void* origin; // origin of the sound
    int handle; // handle of the sound being played
};
} // namespace Doom

namespace Doom
{
// Game/Sound's engine-side sound bookkeeping - which sounds occupy the mixing
// channels, whether music is paused, and the music currently playing. This is the
// *engine* side of sound (s_sound), the game deciding what should be heard; the actual
// mixing/MIDI runtime (Host/Sound's mus_offset, handlenums, ...) is host state and
// deliberately stays out of the Engine, the same split every Host-layer static keeps.
//
// The volumes and channel count that used to sit beside these are config-backed and
// already migrated (Game/SoundSettings.h); this is the pure runtime remainder, moved
// in by the file-scope-statics sweep (REFACTOR.md, Step 5). It was file-local to
// Game/Sound and read by no other file, so the vanilla names become references onto
// these members. Nothing here is hashed - sound is not part of the deterministic
// simulation and the frame goldens see the picture, not the channels - so gathering
// it is golden-neutral, a reference alias being pure storage relocation.
//
// No nextcleanup: startLevelSound set it to 15 as a "gametic the next channel
// cleanup is due" schedule that nothing ever checked, in this rewrite or in
// vanilla s_sound.c. Verified against the 1993 source in this repository's
// history; deleted rather than carried, as no read was lost.
struct SoundState
{
    // the set of mixing channels (numChannels of them, sized at initSound); a channel's
    // sfxinfo is null when it is available. RAII-owned (Step 9) - what was a boot-once
    // doom_malloc; Game/Sound's vanilla name channels_s_sound is a plain-pointer view
    // onto data(), refreshed after the resize (the same owner/view split screens[] and
    // GraphicsData's arrays use).
    Vector<SoundChannel> channels;

    bool mus_paused = false; // whether songs are paused
    MusicInfo* mus_playing = nullptr; // music currently being played
};

// The one SoundState, a view onto the Engine's member - the same pattern as the other
// clusters (wipeState(), hudState(), ...).
SoundState& soundState();
} // namespace Doom
