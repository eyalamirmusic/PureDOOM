// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        DOOM sound/music seam. Rewritten in Host/Sound.{h,cpp}; this keeps the
//        vanilla I_ names as shims. mixbuffer is defined at ::-file-scope in
//        Sound.cpp for its one external reader (DOOM.cpp), so nothing is owned
//        here.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "i_sound.h"

#include "Host/Sound.h"

void I_InitSound()
{
    Doom::I_InitSound();
}

void I_UpdateSound(void)
{
    Doom::I_UpdateSound();
}

void I_SubmitSound(void)
{
    Doom::I_SubmitSound();
}

void I_ShutdownSound(void)
{
    Doom::I_ShutdownSound();
}

void I_SetChannels()
{
    Doom::I_SetChannels();
}

int I_GetSfxLumpNum(sfxinfo_t* sfxinfo)
{
    return Doom::I_GetSfxLumpNum(sfxinfo);
}

int I_StartSound(int id, int vol, int sep, int pitch, int priority)
{
    return Doom::I_StartSound(id, vol, sep, pitch, priority);
}

void I_StopSound(int handle)
{
    Doom::I_StopSound(handle);
}

int I_SoundIsPlaying(int handle)
{
    return Doom::I_SoundIsPlaying(handle);
}

void I_UpdateSoundParams(int handle, int vol, int sep, int pitch)
{
    Doom::I_UpdateSoundParams(handle, vol, sep, pitch);
}

void I_InitMusic(void)
{
    Doom::I_InitMusic();
}

void I_ShutdownMusic(void)
{
    Doom::I_ShutdownMusic();
}

void I_SetMusicVolume(int volume)
{
    Doom::I_SetMusicVolume(volume);
}

void I_PauseSong(int handle)
{
    Doom::I_PauseSong(handle);
}

void I_ResumeSong(int handle)
{
    Doom::I_ResumeSong(handle);
}

int I_RegisterSong(void* data)
{
    return Doom::I_RegisterSong(data);
}

void I_PlaySong(int handle, int looping)
{
    Doom::I_PlaySong(handle, looping);
}

void I_StopSong(int handle)
{
    Doom::I_StopSong(handle);
}

void I_UnRegisterSong(int handle)
{
    Doom::I_UnRegisterSong(handle);
}

unsigned long I_TickSong()
{
    return Doom::I_TickSong();
}
