#pragma once

#include "../r_defs.h" // column_t, byte

namespace Doom
{
// Renderer data; r_data.cpp keeps the vanilla R_ names as shims.
void drawColumnInCache(column_t* patch, byte* cache, int originy, int cacheheight);
void generateComposite(int texnum);
void generateLookup(int texnum);
byte* getColumn(int tex, int col);
void initTextures();
void initFlats();
void initSpriteLumps();
void initColormaps();
void initData();
int flatNumForName(const char* name);
int checkTextureNumForName(const char* name);
int textureNumForName(const char* name);
void precacheLevel();
} // namespace Doom
