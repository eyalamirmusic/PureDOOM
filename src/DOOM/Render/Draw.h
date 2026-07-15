#pragma once

namespace Doom
{
// Column / span blitting and the view buffer setup; r_draw.cpp keeps the vanilla
// R_ names as shims (r_main stores those shim addresses in the drawer pointers).
void drawColumn(void);
void drawColumnLow(void);
void drawFuzzColumn(void);
void drawTranslatedColumn(void);
void initTranslationTables(void);
void drawSpan(void);
void drawSpanLow(void);
void initBuffer(int width, int height);
void videoErase(unsigned ofs, int count);
void fillBackScreen(void);
void drawViewBorder(void);
} // namespace Doom
