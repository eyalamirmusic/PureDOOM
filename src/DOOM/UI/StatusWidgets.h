#pragma once

#include "../st_lib.h" // st_number_t / st_percent_t / st_multicon_t / st_binicon_t

namespace Doom
{
// Status-bar widgets; st_lib.cpp keeps the vanilla STlib_ names as shims.
void initStatusWidgets(void);
void initNum(st_number_t* n,
             int x,
             int y,
             patch_t** pl,
             int* num,
             doom_boolean* on,
             int width);
void updateNum(st_number_t* n, doom_boolean refresh);
void initPercent(st_percent_t* p,
                 int x,
                 int y,
                 patch_t** pl,
                 int* num,
                 doom_boolean* on,
                 patch_t* percent);
void updatePercent(st_percent_t* per, int refresh);
void initMultIcon(
    st_multicon_t* i, int x, int y, patch_t** il, int* inum, doom_boolean* on);
void updateMultIcon(st_multicon_t* mi, doom_boolean refresh);
void initBinIcon(
    st_binicon_t* b, int x, int y, patch_t* i, doom_boolean* val, doom_boolean* on);
void updateBinIcon(st_binicon_t* bi, doom_boolean refresh);
} // namespace Doom
