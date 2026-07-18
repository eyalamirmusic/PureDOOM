#pragma once

namespace Doom
{
// Sound / music control; s_sound.cpp keeps the vanilla S_ names as shims.
void initSound(int sfxVolume, int musicVolume);
void startLevelSound();
void startSound(void* origin, int sound_id);
void startSoundAtVolume(void* origin, int sound_id, int volume);
void stopSound(void* origin);
void startMusic(int music_id);
void changeMusic(int music_id, int looping);
void stopMusic();
void pauseSound();
void resumeSound();
void updateSounds(void* listener);
void setMusicVolumeLevel(int volume);
void setSfxVolume(int volume);
} // namespace Doom
