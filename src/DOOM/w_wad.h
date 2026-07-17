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
//        WAD I/O functions.
//
//-----------------------------------------------------------------------------

#pragma once


// The purge tags W_CacheLumpNum/Name still take but ignore: WadFile owns every
// lump for the process's life now, so the tag decides nothing. They lived in
// z_zone.h until the zone was deleted; kept here so the vanilla call signatures
// (and the many literal PU_STATIC / PU_CACHE arguments) still compile.
#define PU_STATIC 1 // static entire execution time
#define PU_SOUND 2 // static while playing
#define PU_MUSIC 3 // static while playing
#define PU_DAVE 4 // anything else Dave wants static
#define PU_LEVEL 50 // static until level exited
#define PU_LEVSPEC 51 // a special thinker in a level
#define PU_PURGELEVEL 100
#define PU_CACHE 101


//
// TYPES
//
typedef struct
{
    // Should be "IWAD" or "PWAD".
    char identification[4];
    int numlumps;
    int infotableofs;
} wadinfo_t;


typedef struct
{
    int filepos;
    int size;
    char name[8];
} filelump_t;


//
// WADFILE I/O related stuff.
//
typedef struct
{
    char name[8];
    void* handle;
    int position;
    int size;
} lumpinfo_t;


// lumpcache is gone with the zone's lump ownership: Doom::WadFile (Wad/WadFile.h)
// owns the bytes now, and a lump pointer stays valid for as long as the WAD does.
// These two are a view onto its directory, not a second copy of it.
extern lumpinfo_t* lumpinfo;
extern int numlumps;

void W_InitMultipleFiles(char** filenames);
void W_Reload(void);

int W_CheckNumForName(const char* name);
int W_GetNumForName(const char* name);

int W_LumpLength(int lump);
void W_ReadLump(int lump, void* dest);

void* W_CacheLumpNum(int lump, int tag);
void* W_CacheLumpName(const char* name, int tag);



//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
