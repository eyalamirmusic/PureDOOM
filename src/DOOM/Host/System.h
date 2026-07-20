#pragma once

#include "../doomtype.h"
#include "../Game/Ticcmd.h"
#include "Text.h"

namespace Doom
{
// The engine's system seam: timing (currentTic), startup and teardown
// (initHost / quitGame) and the fatal path (fatalError, which the tests catch by
// way of doom_set_exit). Most of the rest are host stubs; emptycmd is file-local
// to System.cpp.
void tactileFeedback(int on, int off, int total);
Ticcmd* baseTiccmd();
int currentTic();
void initHost();
void quitGame();
void waitVBlank(int count);
void fatalError(const std::string& error);

template <typename... Parts>
    requires(sizeof...(Parts) >= 2)
void fatalError(const Parts&... parts)
{
    fatalError(concat(parts...));
}
} // namespace Doom
