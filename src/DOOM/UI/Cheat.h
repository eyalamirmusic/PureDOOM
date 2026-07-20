#pragma once

#include "CheatTypes.h" // CheatSequence

#include <string>

namespace Doom
{
// Cheat-sequence matching; m_cheat.cpp keeps the vanilla cht_ names as shims.
int checkCheat(CheatSequence& cht, char key);
std::string getParam(CheatSequence& cht);
} // namespace Doom
