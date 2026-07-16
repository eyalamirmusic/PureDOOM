#pragma once

#include "../doomtype.h" // byte, doom_boolean

namespace Doom
{
// Config I/O, file I/O, screenshots, M_DrawText; m_misc.cpp keeps the M_ names
// as shims.
int mDrawText(int x, int y, doom_boolean direct, char* string);
doom_boolean mWriteFile(char const* name, void* source, int length);
int mReadFile(char const* name, byte** buffer);
void mSaveDefaults();
void mLoadDefaults();
void mScreenShot();
} // namespace Doom
