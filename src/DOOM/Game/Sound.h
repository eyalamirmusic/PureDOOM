#pragma once

namespace Doom
{
// Sound / music control; s_sound.cpp keeps the vanilla S_ names as shims.
void sInit(int sfxVolume, int musicVolume);
void sStart();
void sStartSound(void* origin, int sound_id);
void sStartSoundAtVolume(void* origin, int sound_id, int volume);
void sStopSound(void* origin);
void sStartMusic(int music_id);
void sChangeMusic(int music_id, int looping);
void sStopMusic();
void sPauseSound();
void sResumeSound();
void sUpdateSounds(void* listener);
void sSetMusicVolume(int volume);
void sSetSfxVolume(int volume);
} // namespace Doom
