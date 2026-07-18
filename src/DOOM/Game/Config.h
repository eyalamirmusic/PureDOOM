#pragma once

#include "../doomtype.h" // byte, doom_boolean

namespace Doom
{
// Config I/O, file I/O, screenshots, Doom::drawText; m_misc.cpp keeps the M_ names
// as shims.
int drawText(int x, int y, doom_boolean direct, char* string);
doom_boolean writeFile(char const* name, void* source, int length);
int readFile(char const* name, byte** buffer);
void saveDefaults();
void loadDefaults();
void writeScreenshot();
} // namespace Doom
