#include "WadFile.h"

#include "../doom_config.h"
#include "../m_swap.h"

#include <ea_data_structures/Structures/Array.h>

#include <cstring>

#include "../Host/System.h"
namespace Doom
{
namespace
{
// The on-disk directory entry. The file stores sizes and offsets little-endian,
// which LONG() swaps on a big-endian host and leaves alone everywhere else.
struct FileLump
{
    int filepos;
    int size;
    char name[8];
};

struct WadHeader
{
    char identification[4]; // "IWAD" or "PWAD"
    int numlumps;
    int infotableofs;
};

void upperCase(char* text)
{
    for (; *text; ++text)
        *text = static_cast<char>(doom_toupper(*text));
}

// A file that is not a .wad becomes a single lump named after itself: the base
// name, up to eight characters, upper-cased.
void extractFileBase(const char* path, char* destination)
{
    const auto* source = path + doom_strlen(path) - 1;

    while (source != path && *(source - 1) != '\\' && *(source - 1) != '/')
        --source;

    doom_memset(destination, 0, 8);

    auto length = 0;

    while (*source && *source != '.')
    {
        if (++length == 9)
            fatalError("Error: Filename base of  >8 chars");

        *destination++ = static_cast<char>(doom_toupper(*source++));
    }
}

// The lump name as eight upper-case bytes, zero-padded - which is how the
// directory stores it, and how the engine compares it. A name that fills all
// eight is NOT terminated.
struct LumpName
{
    char text[9] = {};

    explicit LumpName(const char* name)
    {
        doom_strncpy(text, name, 8);
        text[8] = 0;
        upperCase(text);
    }

    bool matches(const char* directoryName) const
    {
        return std::memcmp(text, directoryName, 8) == 0;
    }
};

void fail(const char* what, const char* detail)
{
    doom_strcpy(error_buf, what);
    doom_concat(error_buf, detail);
    fatalError(error_buf);
}

bool endsInWad(const char* path)
{
    return doom_strcasecmp(path + doom_strlen(path) - 3, "wad") == 0;
}
} // namespace

// wad() is defined in Engine/Engine.cpp now - a view onto the one Engine's `wad`
// member rather than a singleton of its own.

WadFile::~WadFile()
{
    // Vanilla never closed them - it had no shutdown at all, which is the whole
    // reason the engine could not be booted twice.
    for (auto* handle: handles)
        doom_close(handle);
}

void WadFile::addFile(const char* path)
{
    auto reloadable = path[0] == '~';

    if (reloadable)
    {
        ++path;
        reloadName = path;
        reloadLump = count();
    }

    auto* handle = doom_open(path, "rb");

    if (handle == nullptr)
    {
        doom_print(" couldn't open ");
        doom_print(path);
        doom_print("\n");
        return;
    }

    doom_print(" adding ");
    doom_print(path);
    doom_print("\n");

    if (endsInWad(path))
        addDirectory(path, handle);
    else
        addSingleLump(path, handle);

    // A reloadable file is re-opened on every read, so its lumps carry no handle
    // and this one is done with.
    if (reloadable)
        doom_close(handle);
    else
        handles.push_back(handle);

    cache.resize(lumps.size());
}

void WadFile::addSingleLump(const char* path, void* handle)
{
    doom_seek(handle, 0, DOOM_SEEK_END);

    auto lump = Lump {};
    lump.handle = reloadName ? nullptr : handle;
    lump.position = 0;
    lump.size = doom_tell(handle);

    doom_seek(handle, 0, DOOM_SEEK_SET);

    // The lump is named after the file it came from.
    EA::Array<char, 9> base = {};
    extractFileBase(path, base.data());
    doom_strncpy(lump.name, base.data(), 8);

    lumps.push_back(lump);
}

void WadFile::addDirectory(const char* path, void* handle)
{
    auto header = WadHeader {};
    doom_read(handle, &header, sizeof(header));

    if (doom_strncmp(header.identification, "IWAD", 4) != 0
        && doom_strncmp(header.identification, "PWAD", 4) != 0)
    {
        fail("Error: Wad file ", path);
    }

    auto lumpCount = LONG(header.numlumps);
    auto entries = EA::Vector<FileLump>(lumpCount);

    doom_seek(handle, LONG(header.infotableofs), DOOM_SEEK_SET);
    doom_read(
        handle, entries.data(), static_cast<int>(entries.size() * sizeof(FileLump)));

    for (const auto& entry: entries)
    {
        auto lump = Lump {};

        // A reloadable file's lumps carry no handle: read() re-opens the file
        // every time, which is what makes the reload hack work at all.
        lump.handle = reloadName ? nullptr : handle;
        lump.position = LONG(entry.filepos);
        lump.size = LONG(entry.size);
        doom_strncpy(lump.name, entry.name, 8);

        lumps.push_back(lump);
    }
}

int WadFile::find(const char* name) const
{
    const auto wanted = LumpName {name};

    // Backwards, so that a lump from a later file takes precedence over the same
    // name in an earlier one. That is how a PWAD overrides the IWAD.
    for (auto lump = count() - 1; lump >= 0; --lump)
        if (wanted.matches(lumps[lump].name))
            return lump;

    return -1;
}

int WadFile::number(const char* name) const
{
    auto lump = find(name);

    if (lump < 0)
        fail("Error: W_GetNumForName: not found: ", name);

    return lump;
}

int WadFile::length(int lump) const
{
    if (lump < 0 || lump >= count())
        fail("Error: W_LumpLength: no such lump: ", doom_itoa(lump, 10));

    return lumps[lump].size;
}

void WadFile::read(int lump, void* destination) const
{
    if (lump < 0 || lump >= count())
        fail("Error: W_ReadLump: no such lump: ", doom_itoa(lump, 10));

    const auto& entry = lumps[lump];
    auto* handle = entry.handle;
    auto reopened = handle == nullptr;

    if (reopened)
    {
        handle = doom_open(reloadName, "rb");

        if (handle == nullptr)
            fail("Error: W_ReadLump: couldn't open ", reloadName);
    }

    doom_seek(handle, entry.position, DOOM_SEEK_SET);
    auto read = doom_read(handle, destination, entry.size);

    if (reopened)
        doom_close(handle);

    if (read < entry.size)
        fail("Error: W_ReadLump: only read part of lump ", doom_itoa(lump, 10));
}

const std::byte* WadFile::data(int lump)
{
    if (lump < 0 || lump >= count())
        fail("Error: W_CacheLumpNum: no such lump: ", doom_itoa(lump, 10));

    auto& bytes = cache[lump];

    if (bytes.empty() && length(lump) > 0)
    {
        // A zero tail past the lump's own bytes, and it is load-bearing.
        //
        // DOOM reads a few bytes past the end of a lump when a wall texture is
        // shorter than the column it fills - the tutti-frutti effect, a genuine
        // 1993 quirk that draws whatever memory follows the patch. On DOS, and in
        // PureDOOM's old zone allocator, "whatever follows" was another block in
        // one contiguous arena: garbage, but the *same* garbage on every machine.
        // A per-lump vector has no such neighbour, so the over-read would hit
        // heap memory that differs from platform to platform, and the rendered
        // frame would stop being reproducible.
        //
        // Sixty-four zero bytes is enough (the over-read is under that, measured),
        // so the quirk still happens - a wall still tutti-fruttis - but it now
        // draws a deterministic zero everywhere instead of undefined heap.
        constexpr auto overReadGuard = 64;

        bytes.resize(length(lump) + overReadGuard, std::byte {0});
        read(lump, bytes.data());
    }

    return bytes.data();
}

void WadFile::reload()
{
    if (reloadName == nullptr)
        return;

    auto* handle = doom_open(reloadName, "rb");

    if (handle == nullptr)
        fail("Error: W_Reload: couldn't open ", reloadName);

    auto header = WadHeader {};
    doom_read(handle, &header, sizeof(header));

    auto lumpCount = LONG(header.numlumps);
    auto entries = EA::Vector<FileLump>(lumpCount);

    doom_seek(handle, LONG(header.infotableofs), DOOM_SEEK_SET);
    doom_read(
        handle, entries.data(), static_cast<int>(entries.size() * sizeof(FileLump)));
    doom_close(handle);

    for (auto i = 0; i < lumpCount; ++i)
    {
        auto lump = reloadLump + i;

        lumps[lump].position = LONG(entries[i].filepos);
        lumps[lump].size = LONG(entries[i].size);

        // Whatever was cached is now the old file's bytes. Drop it; the next
        // caller re-reads.
        cache[lump].clear();
    }
}

void initWadFiles(char** filenames)
{
    for (; *filenames; ++filenames)
        wad().addFile(*filenames);

    if (wad().count() == 0)
        fatalError("Error: initWadFiles: no files found");
}

void* cacheLumpNum(int lump)
{
    return const_cast<std::byte*>(wad().data(lump));
}

void* cacheLumpName(const char* name)
{
    return cacheLumpNum(wad().number(name));
}
} // namespace Doom
