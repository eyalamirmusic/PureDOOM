#pragma once

#include "../m_cheat.h" // CheatSequence

namespace Doom
{
// Cheat-sequence matching; m_cheat.cpp keeps the vanilla cht_ names as shims.
int checkCheat(CheatSequence* cht, char key);
void getParam(CheatSequence* cht, char* buffer);
} // namespace Doom
