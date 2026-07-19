#pragma once

#include "../doomtype.h" // byte, doom_boolean

#include <ea_data_structures/Structures/Vector.h>

namespace Doom
{
// Config I/O, file I/O, screenshots, Doom::drawText; m_misc.cpp keeps the M_ names
// as shims.
int drawText(int x, int y, doom_boolean direct, char* string);
doom_boolean writeFile(char const* name, void* source, int length);

// Reads the whole file into buffer, sized to fit, and returns its length. Takes the
// owner rather than the old `byte** buffer` out-parameter, which handed a
// doom_malloc'd block back for the caller to free by hand (REFACTOR.md, Step 9
// strand (b)). Fatal, not false, if the file cannot be read - as it always was.
int readFile(char const* name, EA::Vector<byte>& buffer);
void saveDefaults();
void loadDefaults();
void writeScreenshot();
} // namespace Doom
