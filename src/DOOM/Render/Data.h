#pragma once

#include "../r_defs.h" // column_t, byte

namespace Doom
{
// Renderer data; r_data.cpp keeps the vanilla R_ names as shims.
void drawColumnInCache(column_t* patch, byte* cache, int originy, int cacheheight);
void generateComposite(int texnum);
void generateLookup(int texnum);
byte* getColumn(int tex, int col);
void initTextures(void);
void initFlats(void);
void initSpriteLumps(void);
void initColormaps(void);
void initData(void);
int flatNumForName(char* name);
int checkTextureNumForName(char* name);
int textureNumForName(char* name);
void precacheLevel(void);
} // namespace Doom
