// Rewritten out of vanilla m_argv into namespace Doom.
//
// Command-line argument lookup. m_argv.cpp shims M_CheckParm; myargc/myargv are
// the argument vector the host fills in and every subsystem reads, so they stay
// defined at file scope here, above the namespace (still ::-scoped).

#include "../doom_config.h"

#include "Args.h"

int myargc;
char** myargv;

namespace Doom
{

//
// M_CheckParm
// Checks for the given parameter in the program's command line arguments.
// Returns the argument number (1 to argc-1) or 0 if not present.
//
int checkParm(const char* check)
{
    for (int i = 1; i < myargc; i++)
    {
        if (!doom_strcasecmp(check, myargv[i]))
            return i;
    }

    return 0;
}

} // namespace Doom
