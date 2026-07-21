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
// DESCRIPTION:
//        Created by the sound utility written by Dave Taylor.
//        Kept as a sample, DOOM2  sounds. Frozen.
//
//-----------------------------------------------------------------------------

#pragma once

#include "../doomtype.h" // toIndex, for the enum-derived counts below
#include <string_view>

//
// SoundFX struct.
//
namespace Doom
{
struct SfxInfo
{
    // up to 6-character name
    std::string_view name;

    // Sfx singularity (only one at a time)
    int singularity;

    // Sfx priority
    int priority;

    // referenced sound if a link
    SfxInfo* link;

    // pitch if a link
    int pitch;

    // volume if a link
    int volume;

    // sound data
    void* data;

    // this is checked every second to see if sound
    // can be thrown out (if 0, then decrement, if -1,
    // then throw out, if > 0, then it is in use)
    int usefulness;

    // lump number of sfx
    int lumpnum;
};
} // namespace Doom

//
// Doom::MusicInfo struct.
//
namespace Doom
{
struct MusicInfo
{
    // up to 6-character name
    std::string_view name;

    // lump number of music
    int lumpnum;

    // music data
    void* data;

    // music handle once registered
    int handle;
};
} // namespace Doom

// the complete set of sound effects
extern Doom::SfxInfo S_sfx[];

// the complete set of music
extern Doom::MusicInfo S_music[];

//
// Identifiers for all music in game.
//
namespace Doom
{
enum class MusicEnum
{
    None,
    E1m1,
    E1m2,
    E1m3,
    E1m4,
    E1m5,
    E1m6,
    E1m7,
    E1m8,
    E1m9,
    E2m1,
    E2m2,
    E2m3,
    E2m4,
    E2m5,
    E2m6,
    E2m7,
    E2m8,
    E2m9,
    E3m1,
    E3m2,
    E3m3,
    E3m4,
    E3m5,
    E3m6,
    E3m7,
    E3m8,
    E3m9,
    Inter,
    Intro,
    Bunny,
    Victor,
    Introa,
    Runnin,
    Stalks,
    Countd,
    Betwee,
    Doom,
    TheDa,
    Shawn,
    Ddtblu,
    InCit,
    Dead,
    Stlks2,
    Theda2,
    Doom2,
    Ddtbl2,
    Runni2,
    Dead2,
    Stlks3,
    Romero,
    Shawn2,
    Messag,
    Count2,
    Ddtbl3,
    Ampie,
    Theda3,
    Adrian,
    Messg2,
    Romer2,
    Tense,
    Shawn3,
    Openin,
    Evil,
    Ultima,
    ReadM,
    Dm2ttl,
    Dm2int,
    NumMusic
};

// The count, for array sizes and loop bounds; derived from the enum's own
// sentinel so the two cannot drift.
constexpr int numMusic = toIndex(MusicEnum::NumMusic);
} // namespace Doom

//
// Identifiers for all sfx in game.
//
namespace Doom
{
enum class SfxEnum
{
    None,
    Pistol,
    Shotgn,
    Sgcock,
    Dshtgn,
    Dbopn,
    Dbcls,
    Dbload,
    Plasma,
    Bfg,
    Sawup,
    Sawidl,
    Sawful,
    Sawhit,
    Rlaunc,
    Rxplod,
    Firsht,
    Firxpl,
    Pstart,
    Pstop,
    Doropn,
    Dorcls,
    Stnmov,
    Swtchn,
    Swtchx,
    Plpain,
    Dmpain,
    Popain,
    Vipain,
    Mnpain,
    Pepain,
    Slop,
    Itemup,
    Wpnup,
    Oof,
    Telept,
    Posit1,
    Posit2,
    Posit3,
    Bgsit1,
    Bgsit2,
    Sgtsit,
    Cacsit,
    Brssit,
    Cybsit,
    Spisit,
    Bspsit,
    Kntsit,
    Vilsit,
    Mansit,
    Pesit,
    Sklatk,
    Sgtatk,
    Skepch,
    Vilatk,
    Claw,
    Skeswg,
    Pldeth,
    Pdiehi,
    Podth1,
    Podth2,
    Podth3,
    Bgdth1,
    Bgdth2,
    Sgtdth,
    Cacdth,
    Skldth,
    Brsdth,
    Cybdth,
    Spidth,
    Bspdth,
    Vildth,
    Kntdth,
    Pedth,
    Skedth,
    Posact,
    Bgact,
    Dmact,
    Bspact,
    Bspwlk,
    Vilact,
    Noway,
    Barexp,
    Punch,
    Hoof,
    Metal,
    Chgun,
    Tink,
    Bdopn,
    Bdcls,
    Itmbk,
    Flame,
    Flamst,
    Getpow,
    Bospit,
    Boscub,
    Bossit,
    Bospn,
    Bosdth,
    Manatk,
    Mandth,
    Sssit,
    Ssdth,
    Keenpn,
    Keendt,
    Skeact,
    Skesit,
    Skeatk,
    Radio,
    NumSfx
};

// The count, for array sizes and loop bounds; derived from the enum's own
// sentinel so the two cannot drift.
constexpr int numSfx = toIndex(SfxEnum::NumSfx);
} // namespace Doom

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
