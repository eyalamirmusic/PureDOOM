#include "Common.h"

#include <cstdlib>

int main(int argc, char* argv[])
{
    // The engine locates WADs through DOOMWADDIR, falling back to the working
    // directory - which ctest promises nothing about. Not overwritten, so a
    // developer pointing at another WAD still gets it.
#if defined(_WIN32)
    if (!std::getenv("DOOMWADDIR"))
        _putenv_s("DOOMWADDIR", PUREDOOM_ROOT_DIR);
#else
    setenv("DOOMWADDIR", PUREDOOM_ROOT_DIR, 0);
#endif

    return nano::run(argc, argv);
}
