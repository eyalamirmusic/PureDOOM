// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Sound and music control. Rewritten in Game/Sound.{h,cpp}; this keeps the
//        S_ names as shims. The volume/channel globals are defined at file scope
//        in Sound.cpp, so there is nothing to own here.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "s_sound.h"

#include "Game/Sound.h"

void S_Init(int sfxVolume, int musicVolume)
{
    Doom::sInit(sfxVolume, musicVolume);
}

void S_Start(void)
{
    Doom::sStart();
}

void S_StartSound(void* origin, int sound_id)
{
    Doom::sStartSound(origin, sound_id);
}

void S_StartSoundAtVolume(void* origin, int sound_id, int volume)
{
    Doom::sStartSoundAtVolume(origin, sound_id, volume);
}

void S_StopSound(void* origin)
{
    Doom::sStopSound(origin);
}

void S_StartMusic(int music_id)
{
    Doom::sStartMusic(music_id);
}

void S_ChangeMusic(int music_id, int looping)
{
    Doom::sChangeMusic(music_id, looping);
}

void S_StopMusic(void)
{
    Doom::sStopMusic();
}

void S_PauseSound(void)
{
    Doom::sPauseSound();
}

void S_ResumeSound(void)
{
    Doom::sResumeSound();
}

void S_UpdateSounds(void* listener)
{
    Doom::sUpdateSounds(listener);
}

void S_SetMusicVolume(int volume)
{
    Doom::sSetMusicVolume(volume);
}

void S_SetSfxVolume(int volume)
{
    Doom::sSetSfxVolume(volume);
}
