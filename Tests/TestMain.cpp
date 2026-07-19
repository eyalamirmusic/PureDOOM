#include "Common.h"

#include <eacp/Core/Utils/Environment.h>

#if defined(_WIN32) && defined(_DEBUG)
#include <crtdbg.h>
#endif

int main(int argc, char* argv[])
{
#if defined(_WIN32) && defined(_DEBUG)
    // Microsoft's debug CRT reports a failed assertion or a corrupted heap
    // through a modal dialog box. Under ctest there is nobody to click it, and
    // the dialog does not even reach a desktop: the binary simply stops, with no
    // output, no exit code and no CPU, and the run hangs until someone notices.
    //
    // That is not hypothetical - it is how a one-byte heap overflow in the WAD
    // path building presented on Windows arm64. Release segfaulted, which at
    // least says something; Debug hung silently, which says nothing and looks
    // like an infinite loop in the engine. Send the reports to stderr so a broken
    // build fails, promptly, with the reason attached.
    for (auto mode: {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT})
    {
        _CrtSetReportMode(mode, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(mode, _CRTDBG_FILE_STDERR);
    }
#endif

    // The engine locates WADs through DOOMWADDIR, falling back to the working
    // directory - which ctest promises nothing about. Not overwritten, so a
    // developer pointing at another WAD still gets it.
    if (!eacp::getEnv("DOOMWADDIR"))
        eacp::setEnv("DOOMWADDIR", PUREDOOM_ROOT_DIR);

    return nano::run(argc, argv);
}
