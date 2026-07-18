#pragma once

namespace Doom
{
// The machine-independent sound parameters the config persists: the sfx/music
// volumes (0-15) the mixer scales by and the channel count Doom::initSound sizes its
// channel array to.
//
// These are config-backed - Game/Config.cpp's defaults[] table used to capture
// &snd_SfxVolume at static-init time, which is why they resisted the Engine
// migration (a reference-alias would race that capture across translation units
// and segfault). The doom_config->Host rework removed that: Config.cpp now binds
// the defaults[] entries to these members at *runtime* (bindEngineDefaults), so
// there is no static address capture left and they can be owned here like any
// other cluster (REFACTOR.md, Step 5).
//
// Golden-neutral: audio is neither simulated nor hashed, and the config resets
// the volumes from the file before anything reads them.
struct SoundSettings
{
    int sfxVolume = 15;   // sound-effect volume, 0-15
    int musicVolume = 15; // music volume, 0-15
    int numChannels = 0;  // # of mixing channels, set by the defaults code
};

SoundSettings& soundSettings();
} // namespace Doom
