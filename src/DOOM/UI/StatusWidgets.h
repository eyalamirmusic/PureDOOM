#pragma once

#include "StatusWidgetTypes.h" // StatusNumber / StatusPercent / StatusMultIcon / StatusBinIcon

namespace Doom
{
// Status-bar widgets. The vanilla STlib_ names that used to shim these have
// been retired; call sites use these Doom:: names directly.
void initStatusWidgets();
void initNum(
    StatusNumber& n, int x, int y, Patch** pl, int* num, bool* on, int width);
void updateNum(StatusNumber& n, bool refresh);
void initPercent(
    StatusPercent& p, int x, int y, Patch** pl, int* num, bool* on, Patch* percent);
void updatePercent(StatusPercent& per, int refresh);
void initMultIcon(StatusMultIcon& i, int x, int y, Patch** il, int* inum, bool* on);
void updateMultIcon(StatusMultIcon& mi, bool refresh);
void initBinIcon(StatusBinIcon& b, int x, int y, Patch* i, bool* val, bool* on);
void updateBinIcon(StatusBinIcon& bi, bool refresh);
} // namespace Doom
