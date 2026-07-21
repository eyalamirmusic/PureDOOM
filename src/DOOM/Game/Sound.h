#pragma once

namespace Doom
{
// Only the id types are needed here, not their enumerators, and a scoped enum has a
// known underlying type - so opaque declarations keep this header free of SoundData.h.
enum class SfxEnum;
enum class MusicEnum;

// Sound / music control; s_sound.cpp keeps the vanilla S_ names as shims.
void initSound(int sfxVolume, int musicVolume);
void startLevelSound();
void startSound(void* origin, SfxEnum sound_id);
void startSoundAtVolume(void* origin, SfxEnum sound_id, int volume);
void stopSound(void* origin);
void startMusic(MusicEnum music_id);
void changeMusic(MusicEnum music_id, int looping);
void stopMusic();
void pauseSound();
void resumeSound();
void updateSounds(void* listener);
void setMusicVolumeLevel(int volume);
void setSfxVolume(int volume);
} // namespace Doom
