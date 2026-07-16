#pragma once

#include "../i_sound.h" // vanilla I_ sound interface + sfxinfo_t

namespace Doom
{
// The engine's sound/music host seam. PureDOOM ships no audio backend yet
// (gap-log item 1), so the mixer and MIDI tick run but nothing is heard; these
// are the I_ entry points s_sound / Game/Sound drives. i_sound.cpp keeps the
// vanilla I_ names as shims over these. The mixing buffer stays ::-scoped in
// Sound.cpp (DOOM.cpp hands it out); everything else is file-local there.
void I_InitSound();
void I_UpdateSound();
void I_SubmitSound();
void I_ShutdownSound();
void I_SetChannels();
int I_GetSfxLumpNum(sfxinfo_t* sfxinfo);
int I_StartSound(int id, int vol, int sep, int pitch, int priority);
void I_StopSound(int handle);
int I_SoundIsPlaying(int handle);
void I_UpdateSoundParams(int handle, int vol, int sep, int pitch);
void I_InitMusic();
void I_ShutdownMusic();
void I_SetMusicVolume(int volume);
void I_PauseSong(int handle);
void I_ResumeSong(int handle);
int I_RegisterSong(void* data);
void I_PlaySong(int handle, int looping);
void I_StopSong(int handle);
void I_UnRegisterSong(int handle);
unsigned long I_TickSong();
} // namespace Doom
