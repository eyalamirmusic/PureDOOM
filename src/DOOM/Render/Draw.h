#pragma once



#include "../doomtype.h"
// The palette-remap tables the multiplayer player colours are drawn through; an
// aligned view onto DrawState's owned buffer. Was r_draw.h.
extern byte* translationtables;
namespace Doom
{
// Column / span blitting and the view buffer setup; r_draw.cpp keeps the vanilla
// R_ names as shims (r_main stores those shim addresses in the drawer pointers).
void drawColumn();
void drawColumnLow();
void drawFuzzColumn();
void drawTranslatedColumn();
void initTranslationTables();
void drawSpan();
void drawSpanLow();
void initBuffer(int width, int height);
void videoErase(unsigned ofs, int count);
void fillBackScreen();
void drawViewBorder();
} // namespace Doom
