// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:  none
//
//-----------------------------------------------------------------------------

#include "../Host/Platform.h"

#include "SoundData.h"
#include "../Sim/Random.h"
#include "../Wad/WadFile.h"
#include "GameDefs.h"
#include "../Sim/SimDefs.h"
#include "MapSpawns.h"

#include "GameSession.h"
#include "GameVersion.h"
#include "PlayerState.h"
#include "Sound.h"
#include "SoundSettings.h"
#include "SoundState.h"

#include <ea_data_structures/Structures/Array.h>

#include "../Host/Sound.h"
#include "../Host/System.h"
#include "../Render/Main.h"
#include "../Sim/Random.h"
#define S_MAX_VOLUME 127

// Adjustable by menu.
#define NORM_VOLUME snd_MaxVolume

#define S_PITCH_PERTURB 1

// percent attenuation from front to back
#define S_IFRACVOL 30

#define NA 0
#define S_NUMCHANNELS 2

// The snd_*Device selectors that were externed here were always dead - no definition, no
// reader - and are dropped (with their doomstat.h declarations), not moved.

// Doom::SoundChannel moved to Game/SoundState.h (the RAII sweep, Step 9, makes SoundState own the
// channel array by value, which needs the complete type there).

// The engine-side sound bookkeeping now lives on the Engine (Game/SoundState.h, moved
// by the file-scope-statics sweep - REFACTOR.md, Step 5), and is read straight off
// Doom::soundState(). channels_s_sound is a plain-pointer VIEW onto SoundState's owned
// channels vector (RAII, Step 9), refreshed by initSound after the resize.
static Doom::SoundChannel* channels_s_sound = nullptr;

// The sfx/music volumes and the channel count are config-backed, and used to
// resist the Engine migration because Config.cpp's defaults[] captured their
// addresses at static-init time (a reference-alias raced that capture and
// segfaulted). Config.cpp now binds those defaults[] entries to the members at
// runtime (bindEngineDefaults) instead, so these are ordinary references onto
// the Engine's SoundSettings cluster like any other migrated global (Game/
// SoundSettings.h, REFACTOR.md Step 5).
// Both are still externed as references in doomstat.h and read by UI/Menu and Host/Sound,
// so the two definitions stay until those readers go through soundSettings() too.

namespace Doom
{

// when to clip out sounds
// Does not fit the large outdoor areas.
constexpr fixed_t S_CLIPPING_DIST = 1200 * FRACUNIT;

// Distance tp origin when sounds should be maxed out.
// This should relate to movement clipping resolution
// (see BLOCKMAP handling).
// Originally: (200*0x10000).
constexpr fixed_t S_CLOSE_DIST = 160 * FRACUNIT;

// A plain integer divisor in whole map units (1040), not a fixed value: the volume
// ramp below divides a whole-unit distance by it.
constexpr int S_ATTENUATOR = (S_CLIPPING_DIST - S_CLOSE_DIST).toInt();

constexpr int NORM_PITCH = 128;
constexpr int NORM_PRIORITY = 64;
constexpr int NORM_SEP = 128;

constexpr fixed_t S_STEREO_SWING = 96 * FRACUNIT;

//
// Prototypes
//
int getChannel(void* origin, SfxInfo* sfxinfo);
int adjustSoundParams(Mobj* listener, Mobj* source, int* vol, int* sep, int* pitch);
void stopChannel(int cnum);

//
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//  allocates channel buffer, sets S_sfx lookup.
//
void initSound(int sfxVolume, int musicVolume)
{
    //doom_print("initSound: default sfx volume %d\n", sfxVolume);
    doom_print("initSound: default sfx volume ");
    doom_print(doom_itoa(sfxVolume, 10));
    doom_print("\n");

    // Whatever these did with DMX, these are rather dummies now.
    setChannels();

    setSfxVolume(sfxVolume);
    // No music with Linux - another dummy.
    setMusicVolumeLevel(musicVolume);

    // Allocating the internal channels for mixing
    // (the maximum numer of sounds rendered
    // simultaneously). RAII now (Step 9): SoundState owns the vector; channels_s_sound
    // is the view onto its data(). resize value-initialises each SoundChannel (sfxinfo
    // null), so the explicit clear below is kept only to match vanilla verbatim.
    auto& snd = Doom::soundState();
    auto& sndset = soundSettings();
    snd.channels.resize(sndset.numChannels);
    channels_s_sound = snd.channels.data();

    // Free all channels for use
    for (int i = 0; i < sndset.numChannels; i++)
        channels_s_sound[i].sfxinfo = nullptr;

    // no sounds are playing, and they are not mus_paused
    snd.mus_paused = false;

    // Note that sounds have not been cached (yet).
    for (int i = 1; i < NUMSFX; i++)
        S_sfx[i].lumpnum = S_sfx[i].usefulness = -1;
}

//
// Per level startup code.
// Kills playing sounds at start of level,
//  determines music if any, changes music.
//
void startLevelSound()
{
    int mnum;

    auto& sound = soundState();
    auto& session = gameSession();

    // kill all playing sounds at start of level
    //  (trust me - a good idea)
    for (int cnum = 0; cnum < soundSettings().numChannels; cnum++)
        if (channels_s_sound[cnum].sfxinfo)
            stopChannel(cnum);

    // start new music for the level
    sound.mus_paused = false;

    if (gameVersion().gamemode == commercial)
        mnum = mus_runnin + session.gamemap - 1;
    else
    {
        int spmus[] = {
            // Song - Who? - Where?

            mus_e3m4, // American        e4m1
            mus_e3m2, // Romero        e4m2
            mus_e3m3, // Shawn        e4m3
            mus_e1m5, // American        e4m4
            mus_e2m7, // Tim         e4m5
            mus_e2m4, // Romero        e4m6
            mus_e2m6, // J.Anderson        e4m7 CHIRON.WAD
            mus_e2m5, // Shawn        e4m8
            mus_e1m9 // Tim                e4m9
        };

        if (session.gameepisode < 4)
            mnum = mus_e1m1 + (session.gameepisode - 1) * 9 + session.gamemap - 1;
        else
            mnum = spmus[session.gamemap - 1];
    }

    // HACK FOR COMMERCIAL
    //  if (commercial && mnum > mus_e3m9)
    //      mnum -= mus_e3m9;

    changeMusic(mnum, true);
}

void startSoundAtVolume(void* origin_p, int sfx_id, int volume)
{
    int rc;
    int sep;
    int pitch;
    int priority;
    SfxInfo* sfx;
    int cnum;

    Mobj* origin = static_cast<Mobj*>(origin_p);

    auto& sndset = soundSettings();

    // check for bogus sound #
    if (sfx_id < 1 || sfx_id > NUMSFX)
    {
        //fatalError("Error: Bad sfx #: %d", sfx_id);
        doom_strcpy(error_buf, "Error: Bad sfx #: ");
        doom_concat(error_buf, doom_itoa(sfx_id, 10));
        fatalError(error_buf);
    }

    sfx = &S_sfx[sfx_id];

    // Initialize sound parameters
    if (sfx->link)
    {
        pitch = sfx->pitch;
        priority = sfx->priority;
        volume += sfx->volume;

        if (volume < 1)
            return;

        if (volume > sndset.sfxVolume)
            volume = sndset.sfxVolume;
    }
    else
    {
        pitch = NORM_PITCH;
        priority = NORM_PRIORITY;
    }

    auto& state = playerState();

    // Check to see if it is audible,
    //  and if not, modify the params
    if (origin && origin != state.players[state.consoleplayer].mo)
    {
        rc = adjustSoundParams(
            state.players[state.consoleplayer].mo, origin, &volume, &sep, &pitch);

        if (origin->x == state.players[state.consoleplayer].mo->x
            && origin->y == state.players[state.consoleplayer].mo->y)
        {
            sep = NORM_SEP;
        }

        if (!rc)
            return;
    }
    else
    {
        sep = NORM_SEP;
    }

    // hacks to vary the sfx pitches
    if (sfx_id >= sfx_sawup && sfx_id <= sfx_sawhit)
    {
        pitch += 8 - (Doom::randomness().forMenu() & 15);

        if (pitch < 0)
            pitch = 0;
        else if (pitch > 255)
            pitch = 255;
    }
    else if (sfx_id != sfx_itemup && sfx_id != sfx_tink)
    {
        pitch += 16 - (Doom::randomness().forMenu() & 31);

        if (pitch < 0)
            pitch = 0;
        else if (pitch > 255)
            pitch = 255;
    }

    // kill old sound
    stopSound(origin);

    // try to find a channel
    cnum = getChannel(origin, sfx);

    if (cnum < 0)
        return;

    //
    // This is supposed to handle the loading/caching.
    // For some odd reason, the caching is done nearly
    //  each time the sound is needed?
    //

    // get lumpnum if necessary
    if (sfx->lumpnum < 0)
        sfx->lumpnum = sfxLumpNum(sfx);

#ifndef SNDSRV
    // cache data if necessary
    if (!sfx->data)
    {
        doom_print("startSoundAtVolume: 16bit and not pre-cached - wtf?\n");

        // DOS remains, 8bit handling
        //sfx->data = (void *) Doom::cacheLumpNum(sfx->lumpnum);
        // fprintf( stderr,
        //             "startSoundAtVolume: loading %d (lump %d) : 0x%x\n",
        //       sfx_id, sfx->lumpnum, (int)sfx->data );
    }
#endif

    // increase the usefulness
    if (sfx->usefulness++ < 0)
        sfx->usefulness = 1;

    // Assigns the handle to one of the channels in the
    //  mix/output buffer.
    channels_s_sound[cnum].handle = startSoundHost(sfx_id,
                                                   /*sfx->data,*/
                                                   volume,
                                                   sep,
                                                   pitch,
                                                   priority);
}

void startSound(void* origin, int sfx_id)
{
    startSoundAtVolume(origin, sfx_id, soundSettings().sfxVolume);
}

void stopSound(void* origin)
{
    for (int cnum = 0; cnum < soundSettings().numChannels; cnum++)
    {
        if (channels_s_sound[cnum].sfxinfo
            && channels_s_sound[cnum].origin == origin)
        {
            stopChannel(cnum);
            break;
        }
    }
}

//
// Stop and resume music, during game PAUSE.
//
void pauseSound()
{
    auto& sound = soundState();

    if (sound.mus_playing && !sound.mus_paused)
    {
        pauseSong(sound.mus_playing->handle);
        sound.mus_paused = true;
    }
}

void resumeSound()
{
    auto& sound = soundState();

    if (sound.mus_playing && sound.mus_paused)
    {
        resumeSong(sound.mus_playing->handle);
        sound.mus_paused = false;
    }
}

//
// Updates music & sounds
//
void updateSounds(void* listener_p)
{
    int audible;
    int volume;
    int sep;
    int pitch;
    SfxInfo* sfx;
    SoundChannel* c;

    Mobj* listener = static_cast<Mobj*>(listener_p);

    auto& sndset = soundSettings();

    for (int cnum = 0; cnum < sndset.numChannels; cnum++)
    {
        c = &channels_s_sound[cnum];
        sfx = c->sfxinfo;

        if (c->sfxinfo)
        {
            if (soundIsPlaying(c->handle))
            {
                // initialize parameters
                volume = sndset.sfxVolume;
                pitch = NORM_PITCH;
                sep = NORM_SEP;

                if (sfx->link)
                {
                    pitch = sfx->pitch;
                    volume += sfx->volume;
                    if (volume < 1)
                    {
                        stopChannel(cnum);
                        continue;
                    }
                    else if (volume > sndset.sfxVolume)
                    {
                        volume = sndset.sfxVolume;
                    }
                }

                // check non-local sounds for distance clipping
                //  or modify their params
                if (c->origin && listener_p != c->origin)
                {
                    audible = adjustSoundParams(listener,
                                                static_cast<Mobj*>((c->origin)),
                                                &volume,
                                                &sep,
                                                &pitch);

                    if (!audible)
                    {
                        stopChannel(cnum);
                    }
                    else
                        updateSoundParams(c->handle, volume, sep, pitch);
                }
            }
            else
            {
                // if channel is allocated but sound has stopped,
                //  free it
                stopChannel(cnum);
            }
        }
    }
}

void setMusicVolumeLevel(int volume)
{
    if (volume < 0 || volume > 127)
    {
        //fatalError("Error: Attempt to set music volume at %d",
        //        volume);
        doom_strcpy(error_buf, "Error: Attempt to set music volume at ");
        doom_concat(error_buf, doom_itoa(volume, 10));
        fatalError(error_buf);
    }

    setMusicVolume(127);
    setMusicVolume(volume);
    soundSettings().musicVolume = volume;
}

void setSfxVolume(int volume)
{
    if (volume < 0 || volume > 127)
    {
        //fatalError("Error: Attempt to set sfx volume at %d", volume);
        doom_strcpy(error_buf, "Error: Attempt to set sfx volume at ");
        doom_concat(error_buf, doom_itoa(volume, 10));
        fatalError(error_buf);
    }

    soundSettings().sfxVolume = volume;
}

//
// Starts some music with the music id found in sounds.h.
//
void startMusic(int m_id)
{
    changeMusic(m_id, false);
}

void changeMusic(int musicnum, int looping)
{
    MusicInfo* music = nullptr;
    EA::Array<char, 9> namebuf;

    auto& sound = soundState();

    if ((musicnum <= mus_None) || (musicnum >= NUMMUSIC))
    {
        //fatalError("Error: Bad music number %d", musicnum);
        doom_strcpy(error_buf, "Error: Bad music number ");
        doom_concat(error_buf, doom_itoa(musicnum, 10));
        fatalError(error_buf);
    }
    else
        music = &S_music[musicnum];

    if (sound.mus_playing == music)
        return;

    // shutdown old music
    stopMusic();

    // get lumpnum if neccessary
    if (!music->lumpnum)
    {
        //doom_sprintf(namebuf, "d_%s", music->name);
        doom_strcpy(namebuf.data(), "d_");
        doom_concat(namebuf.data(), music->name);
        music->lumpnum = Doom::wad().number(namebuf.data());
    }

    // load & it
    music->data = (void*) Doom::cacheLumpNum(music->lumpnum);
    music->handle = registerSong(music->data);

    // play it
    playSong(music->handle, looping);

    sound.mus_playing = music;
}

void stopMusic()
{
    auto& sound = soundState();

    if (sound.mus_playing)
    {
        if (sound.mus_paused)
            resumeSong(sound.mus_playing->handle);

        stopSong(sound.mus_playing->handle);
        unregisterSong(sound.mus_playing->handle);

        sound.mus_playing->data = nullptr;
        sound.mus_playing = nullptr;
    }
}

void stopChannel(int cnum)
{
    SoundChannel& c = channels_s_sound[cnum];

    if (c.sfxinfo)
    {
        // stop the sound playing
        if (soundIsPlaying(c.handle))
        {
#ifdef SAWDEBUG
            if (c.sfxinfo == &S_sfx[sfx_sawful])
                doom_print("stopped\n");
#endif
            stopSoundHost(c.handle);
        }

        // check to see
        //  if other channels are playing the sound
        for (int i = 0; i < soundSettings().numChannels; i++)
        {
            if (cnum != i && c.sfxinfo == channels_s_sound[i].sfxinfo)
            {
                break;
            }
        }

        // degrade usefulness of sound data
        c.sfxinfo->usefulness--;

        c.sfxinfo = nullptr;
    }
}

//
// Changes volume, stereo-separation, and pitch variables
//  from the norm of a sound effect to be played.
// If the sound is not audible, returns a 0.
// Otherwise, modifies parameters and returns 1.
//
// pitch is unused: vanilla leaves its pitch adjustment commented out.
int adjustSoundParams(
    Mobj* listener, Mobj* source, int* vol, int* sep, [[maybe_unused]] int* pitch)
{
    fixed_t approx_dist;
    fixed_t adx;
    fixed_t ady;
    angle_t angle;

    auto& sndset = soundSettings();
    auto& session = gameSession();

    // calculate the distance to sound origin
    //  and clip it if necessary
    adx = doom_abs(listener->x - source->x);
    ady = doom_abs(listener->y - source->y);

    // From _GG1_ p.428. Appox. eucledian distance fast.
    approx_dist = adx + ady - ((adx < ady ? adx : ady) >> 1);

    if (session.gamemap != 8 && approx_dist > S_CLIPPING_DIST)
    {
        return 0;
    }

    // angle of source to listener
    angle = Doom::pointToAngle2(listener->x, listener->y, source->x, source->y);

    if (angle > listener->angle)
        angle = angle - listener->angle;
    else
        angle = angle + (angle_t {0xffffffff} - listener->angle);

    const auto angleFine = angle.fineIndex();

    // stereo separation
    *sep = 128 - FixedMul(S_STEREO_SWING, finesine[angleFine]).toInt();

    // volume calculation
    if (approx_dist < S_CLOSE_DIST)
    {
        *vol = sndset.sfxVolume;
    }
    else if (session.gamemap == 8)
    {
        if (approx_dist > S_CLIPPING_DIST)
            approx_dist = S_CLIPPING_DIST;

        *vol = 15
               + ((sndset.sfxVolume - 15) * (S_CLIPPING_DIST - approx_dist).toInt())
                     / S_ATTENUATOR;
    }
    else
    {
        // distance effect
        *vol = (sndset.sfxVolume * (S_CLIPPING_DIST - approx_dist).toInt())
               / S_ATTENUATOR;
    }

    return (*vol > 0);
}

//
// getChannel :
// If none available, return -1.  Otherwise channel #.
//
int getChannel(void* origin, SfxInfo* sfxinfo)
{
    // channel number to use
    int cnum;

    SoundChannel* c;

    auto& sndset = soundSettings();

    // Find an open channel
    for (cnum = 0; cnum < sndset.numChannels; cnum++)
    {
        if (!channels_s_sound[cnum].sfxinfo)
            break;
        else if (origin && channels_s_sound[cnum].origin == origin)
        {
            stopChannel(cnum);
            break;
        }
    }

    // None available
    if (cnum == sndset.numChannels)
    {
        // Look for lower priority
        for (cnum = 0; cnum < sndset.numChannels; cnum++)
            if (channels_s_sound[cnum].sfxinfo->priority >= sfxinfo->priority)
                break;

        if (cnum == sndset.numChannels)
        {
            // FUCK!  No lower priority.  Sorry, Charlie.
            return -1;
        }
        else
        {
            // Otherwise, kick out lower priority.
            stopChannel(cnum);
        }
    }

    c = &channels_s_sound[cnum];

    // channel is decided to be cnum.
    c->sfxinfo = sfxinfo;
    c->origin = origin;

    return cnum;
}

} // namespace Doom
