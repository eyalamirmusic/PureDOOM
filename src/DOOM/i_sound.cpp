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

void initSoundHost()
{
    Doom::initSoundHost();
}

void updateSound()
{
    Doom::updateSound();
}

void submitSound()
{
    Doom::submitSound();
}

void shutdownSoundHost()
{
    Doom::shutdownSoundHost();
}

void setChannels()
{
    Doom::setChannels();
}

int sfxLumpNum(Doom::SfxInfo* sfxinfo)
{
    return Doom::sfxLumpNum(sfxinfo);
}

int startSoundHost(int id, int vol, int sep, int pitch, int priority)
{
    return Doom::startSoundHost(id, vol, sep, pitch, priority);
}

void stopSoundHost(int handle)
{
    Doom::stopSoundHost(handle);
}

int soundIsPlaying(int handle)
{
    return Doom::soundIsPlaying(handle);
}

void updateSoundParams(int handle, int vol, int sep, int pitch)
{
    Doom::updateSoundParams(handle, vol, sep, pitch);
}

void initMusic()
{
    Doom::initMusic();
}

void shutdownMusic()
{
    Doom::shutdownMusic();
}

void setMusicVolume(int volume)
{
    Doom::setMusicVolume(volume);
}

void pauseSong(int handle)
{
    Doom::pauseSong(handle);
}

void resumeSong(int handle)
{
    Doom::resumeSong(handle);
}

int registerSong(void* data)
{
    return Doom::registerSong(data);
}

void playSong(int handle, int looping)
{
    Doom::playSong(handle, looping);
}

void stopSong(int handle)
{
    Doom::stopSong(handle);
}

void unregisterSong(int handle)
{
    Doom::unregisterSong(handle);
}

unsigned long tickSong()
{
    return Doom::tickSong();
}
