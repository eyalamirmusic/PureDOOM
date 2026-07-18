#pragma once

#include "StatusWidgetTypes.h" // StatusNumber / StatusPercent / StatusMultIcon / StatusBinIcon

namespace Doom
{
// Status-bar widgets. The vanilla STlib_ names that used to shim these have
// been retired; call sites use these Doom:: names directly.
void initStatusWidgets();
void initNum(StatusNumber& n,
             int x,
             int y,
             Patch** pl,
             int* num,
             doom_boolean* on,
             int width);
void updateNum(StatusNumber& n, doom_boolean refresh);
void initPercent(StatusPercent& p,
                 int x,
                 int y,
                 Patch** pl,
                 int* num,
                 doom_boolean* on,
                 Patch* percent);
void updatePercent(StatusPercent& per, int refresh);
void initMultIcon(
    StatusMultIcon& i, int x, int y, Patch** il, int* inum, doom_boolean* on);
void updateMultIcon(StatusMultIcon& mi, doom_boolean refresh);
void initBinIcon(
    StatusBinIcon& b, int x, int y, Patch* i, doom_boolean* val, doom_boolean* on);
void updateBinIcon(StatusBinIcon& bi, doom_boolean refresh);
} // namespace Doom
