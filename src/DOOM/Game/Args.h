#pragma once

namespace Doom
{
// Command-line argument lookup; m_argv.cpp keeps Doom::checkParm as a shim.
int checkParm(const char* check);
} // namespace Doom
