#pragma once

#include "../doomtype.h" // doom_boolean
#include "../sounds.h" // musicinfo_t (an anonymous-struct typedef, so it cannot be
// forward-declared - only a pointer is held here regardless)

// channel_t is Game/Sound's own file-local struct (the definition never left the .cpp);
// only a pointer to it lives here, so a forward declaration is enough.
struct channel_t;

namespace Doom
{
// Game/Sound's engine-side sound bookkeeping - which sounds occupy the mixing
// channels, whether music is paused, the music currently playing, and when the next
// stale-channel cleanup is due. This is the *engine* side of sound (s_sound), the
// game deciding what should be heard; the actual mixing/MIDI runtime (Host/Sound's
// mus_offset, handlenums, ...) is host state and deliberately stays out of the Engine,
// the same split every Host-layer static keeps.
//
// The volumes and channel count that used to sit beside these are config-backed and
// already migrated (Game/SoundSettings.h); this is the pure runtime remainder, moved
// in by the file-scope-statics sweep (REFACTOR.md, Step 5). It was file-local to
// Game/Sound and read by no other file, so the vanilla names become references onto
// these members. Nothing here is hashed - sound is not part of the deterministic
// simulation and the frame goldens see the picture, not the channels - so gathering
// it is golden-neutral, a reference alias being pure storage relocation.
struct SoundState
{
    // the set of mixing channels (numChannels of them, allocated at S_Init);
    // null when a channel is available
    ::channel_t* channels = nullptr;

    doom_boolean mus_paused = false; // whether songs are paused
    ::musicinfo_t* mus_playing = nullptr; // music currently being played
    int nextcleanup = 0; // gametic the next channel cleanup is due
};

// The one SoundState, a view onto the Engine's member - the same pattern as the other
// clusters (wipeState(), hudState(), ...).
SoundState& soundState();
} // namespace Doom
