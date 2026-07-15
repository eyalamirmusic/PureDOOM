#pragma once

#include "../m_cheat.h" // cheatseq_t

namespace Doom
{
// Cheat-sequence matching; m_cheat.cpp keeps the vanilla cht_ names as shims.
int checkCheat(cheatseq_t* cht, char key);
void getParam(cheatseq_t* cht, char* buffer);
} // namespace Doom
