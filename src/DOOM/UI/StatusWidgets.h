#pragma once

#include "../st_lib.h" // st_number_t / st_percent_t / st_multicon_t / st_binicon_t

namespace Doom
{
// Status-bar widgets. The vanilla STlib_ names that used to shim these have
// been retired; call sites use these Doom:: names directly.
void initStatusWidgets();
void initNum(st_number_t& n,
             int x,
             int y,
             Patch** pl,
             int* num,
             doom_boolean* on,
             int width);
void updateNum(st_number_t& n, doom_boolean refresh);
void initPercent(st_percent_t& p,
                 int x,
                 int y,
                 Patch** pl,
                 int* num,
                 doom_boolean* on,
                 Patch* percent);
void updatePercent(st_percent_t& per, int refresh);
void initMultIcon(
    st_multicon_t& i, int x, int y, Patch** il, int* inum, doom_boolean* on);
void updateMultIcon(st_multicon_t& mi, doom_boolean refresh);
void initBinIcon(
    st_binicon_t& b, int x, int y, Patch* i, doom_boolean* val, doom_boolean* on);
void updateBinIcon(st_binicon_t& bi, doom_boolean refresh);
} // namespace Doom
