// _WIN32, not WIN32 - see the note in Platform.h. Bare WIN32 is not a compiler
// macro; it reaches this file only because CMake adds -DWIN32 for MSVC-style
// drivers. It is load-bearing: the default file I/O below is fopen/fread/fseek,
// which Microsoft's CRT deprecates in favour of its _s variants.
#if defined(_WIN32)
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE
#endif

#include "Host.h"

#include "../doomtype.h" // toIndex

#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
// <windows.h>, not <winsock.h>. This wants SYSTEMTIME/FILETIME/GetSystemTime,
// which are plain Win32 - winsock.h reached them only by including windows.h on
// its way past, while also declaring Winsock *1*, which then conflicts with the
// <WinSock2.h> in Host/Net.cpp if the two ever meet in one translation unit.
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include <eacp/Core/Utils/Environment.h>

namespace Doom
{
namespace
{
void defaultGetTime(int* sec, int* usec)
{
#if defined(_WIN32)
    static const unsigned long long EPOCH =
        ((unsigned long long) 116444736000000000ULL);
    SYSTEMTIME system_time;
    FILETIME file_time;
    unsigned long long time;
    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);
    time = ((unsigned long long) file_time.dwLowDateTime);
    time += ((unsigned long long) file_time.dwHighDateTime) << 32;
    *sec = (int) ((time - EPOCH) / 10000000L);
    *usec = (int) (system_time.wMilliseconds * 1000);
#else
    struct timeval tp;

#ifdef __linux__
    gettimeofday(&tp, nullptr);
#else
    struct timezone tzp;
    gettimeofday(&tp, &tzp);
#endif

    *sec = static_cast<int>(tp.tv_sec);
    *usec = static_cast<int>(tp.tv_usec);
#endif
}
} // namespace

// Every hook starts out working (eacp style: non-null defaults, so no call
// site ever null-checks). An embedder overrides what its platform needs.
Host::Host()
{
    print = [](std::string_view text)
    { std::fwrite(text.data(), 1, text.size(), stdout); };

    malloc = [](int size) { return std::malloc(static_cast<size_t>(size)); };
    free = [](void* pointer) { std::free(pointer); };

    open = [](std::string_view filename, std::string_view mode) -> void*
    {
        return std::fopen(std::string {filename}.c_str(),
                          std::string {mode}.c_str());
    };

    close = [](void* handle) { std::fclose(static_cast<FILE*>(handle)); };

    read = [](void* handle, void* buffer, int count)
    {
        return static_cast<int>(
            std::fread(buffer, 1, count, static_cast<FILE*>(handle)));
    };

    write = [](void* handle, const void* buffer, int count)
    {
        return static_cast<int>(
            std::fwrite(buffer, 1, count, static_cast<FILE*>(handle)));
    };

    // SeekOrigin's values are passed straight to fseek, which vanilla relied on
    // without saying so. Pin it: if a platform ever numbered SEEK_* differently,
    // every WAD read would seek to the wrong place and nothing would say why.
    static_assert(toIndex(SeekOrigin::Set) == SEEK_SET
                      && toIndex(SeekOrigin::Current) == SEEK_CUR
                      && toIndex(SeekOrigin::End) == SEEK_END,
                  "Doom::SeekOrigin must match the C library's SEEK_* values");

    seek = [](void* handle, int offset, SeekOrigin origin)
    { return std::fseek(static_cast<FILE*>(handle), offset, toIndex(origin)); };

    tell = [](void* handle)
    { return static_cast<int>(std::ftell(static_cast<FILE*>(handle))); };

    eof = [](void* handle) { return std::feof(static_cast<FILE*>(handle)); };

    gettime = defaultGetTime;

    exit = [](int code) { std::exit(code); };

    // eacp::getEnv answers the platform question - std::getenv is deprecated
    // by Microsoft's CRT, and there is no portable spelling of the setter at
    // all. Re-read every call, so this stays a view of the environment as it
    // stands and not a snapshot of it at first use.
    getenv = [](std::string_view name) { return eacp::getEnv(name); };
}

// A function-local static, so it is constructed on the first call whoever makes
// it - which is Api.cpp binding `PrintHandler& doom_print = host().print` (and
// the other twelve) at static-init time, before main(). The same shape as
// engine(), and deliberately a *separate* singleton: the host callbacks are
// platform state that must outlive any one constructed world.
Host& host()
{
    static auto instance = Host {};
    return instance;
}
} // namespace Doom
