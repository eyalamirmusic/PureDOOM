#pragma once

#include "../doomtype.h"

namespace Doom
{
// Column / span blitting and the view buffer setup. executeSetViewSize picks which
// of these the renderer runs per column and per span; the selection lives on the
// Drawers cluster (Render/Drawers.h).
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
