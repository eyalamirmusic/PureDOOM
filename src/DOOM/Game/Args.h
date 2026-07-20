#pragma once

#include <string>
#include <string_view>
#include <vector>

// The command line as main() received it. Was m_argv.h.
//
// It owns its strings. The host hands in a char** it does not promise to keep
// alive, and findResponseFile rebuilds the whole vector from a file's contents -
// which used to mean two doom_mallocs that nothing ever freed, and a fixed
// MAXARGVS ceiling that a long response file walked straight off.
extern std::vector<std::string> myargv;

// The count is the vector's size rather than a second variable beside it: the
// two would be one edit away from disagreeing, and every `i < myargCount() - 1`
// guard below depends on them not.
inline int myargCount()
{
    return static_cast<int>(myargv.size());
}
namespace Doom
{
// Command-line argument lookup; m_argv.cpp keeps Doom::checkParm as a shim.
int checkParm(std::string_view check);
} // namespace Doom
