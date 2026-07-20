#pragma once

#include <string_view>

// The command line as main() received it. Was m_argv.h.
extern int myargc;
extern char** myargv;
namespace Doom
{
// Command-line argument lookup; m_argv.cpp keeps Doom::checkParm as a shim.
int checkParm(std::string_view check);
} // namespace Doom
