#pragma once

#include "../i_sound.h" // vanilla I_ sound interface + sfxinfo_t

namespace Doom
{
// The engine's sound/music host seam. PureDOOM ships no audio backend yet
// (gap-log item 1), so the mixer and MIDI tick run but nothing is heard; these
// are the I_ entry points s_sound / Game/Sound drives. i_sound.cpp keeps the
// vanilla I_ names as shims over these. The mixing buffer stays ::-scoped in
// Sound.cpp (DOOM.cpp hands it out); everything else is file-local there.
void initSoundHost();
void updateSound();
void submitSound();
void shutdownSoundHost();
void setChannels();
int sfxLumpNum(sfxinfo_t* sfxinfo);
int startSoundHost(int id, int vol, int sep, int pitch, int priority);
void stopSoundHost(int handle);
int soundIsPlaying(int handle);
void updateSoundParams(int handle, int vol, int sep, int pitch);
void initMusic();
void shutdownMusic();
void setMusicVolume(int volume);
void pauseSong(int handle);
void resumeSong(int handle);
int registerSong(void* data);
void playSong(int handle, int looping);
void stopSong(int handle);
void unregisterSong(int handle);
unsigned long tickSong();
} // namespace Doom
