// Rewritten out of vanilla m_argv into namespace Doom.
//
// Command-line argument lookup. m_argv.cpp shims Doom::checkParm; myargc/myargv are
// the argument vector the host fills in and every subsystem reads, so they stay
// defined at file scope here, above the namespace (still ::-scoped).

#include "../Host/Platform.h"
#include "../Host/Text.h"

#include "Args.h"

std::vector<std::string> myargvData;

std::vector<std::string>& myargv()
{
    return myargvData;
}

namespace Doom
{

//
// checkParm
// Checks for the given parameter in the program's command line arguments.
// Returns the argument number (1 to argc-1) or 0 if not present.
//
int checkParm(std::string_view check)
{
    for (int i = 1; i < myargCount(); i++)
    {
        if (equalsIgnoreCase(check, myargv()[i]))
            return i;
    }

    return 0;
}

} // namespace Doom
