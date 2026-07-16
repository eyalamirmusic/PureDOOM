#pragma once

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
