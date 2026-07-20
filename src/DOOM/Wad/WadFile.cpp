#include "WadFile.h"

#include "../Host/Platform.h"
#include "../Math/Swap.h"

#include <ea_data_structures/Structures/Array.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "../Host/System.h"
namespace Doom
{
namespace
{
// The on-disk directory entry. The file stores sizes and offsets little-endian,
// which littleEndian() swaps on a big-endian host and leaves alone everywhere else.
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

// A file that is not a .wad becomes a single lump named after itself: the base
// name, up to eight characters, upper-cased.
std::string extractFileBase(std::string_view path)
{
    auto lastSlash = path.find_last_of("/\\");
    auto base =
        lastSlash == std::string_view::npos ? path : path.substr(lastSlash + 1);

    auto name = std::string {};

    for (auto character: base)
    {
        if (character == '.')
            break;

        if (name.size() == 8)
            fatalError("Error: Filename base of  >8 chars");

        name += static_cast<char>(toUpper(character));
    }

    return name;
}

// The lump name as eight upper-case bytes, zero-padded - which is how the
// directory stores it, and how the engine compares it. A name that fills all
// eight is NOT terminated.
struct LumpName
{
    EA::Array<char, 9> text = {};

    explicit LumpName(std::string_view name)
    {
        auto length = std::min<int>(static_cast<int>(name.size()), 8);

        for (auto i = 0; i < length; ++i)
            text[i] = static_cast<char>(toUpper(name[i]));
    }

    bool matches(const char* directoryName) const
    {
        return std::memcmp(text.data(), directoryName, 8) == 0;
    }
};

bool endsInWad(std::string_view path)
{
    if (path.size() < 3)
        return false;

    return equalsIgnoreCase(path.substr(path.size() - 3), "wad");
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

void WadFile::addFile(std::string_view path)
{
    auto reloadable = !path.empty() && path.front() == '~';

    if (reloadable)
    {
        path.remove_prefix(1);
        reloadName = path;
        reloadLump = count();
    }

    auto owned = std::string {path};
    auto* handle = doom_open(owned.c_str(), "rb");

    if (handle == nullptr)
    {
        print(" couldn't open ", path, "\n");
        return;
    }

    print(" adding ", path, "\n");

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

void WadFile::addSingleLump(std::string_view path, void* handle)
{
    doom_seek(handle, 0, DOOM_SEEK_END);

    auto lump = Lump {};
    lump.handle = reloadName.empty() ? handle : nullptr;
    lump.position = 0;
    lump.size = doom_tell(handle);

    doom_seek(handle, 0, DOOM_SEEK_SET);

    // The lump is named after the file it came from. Lump::name is eight bytes,
    // zero-padded (value-initialised above), and NOT terminated when full.
    auto base = extractFileBase(path);
    std::copy(base.begin(), base.end(), lump.name.begin());

    lumps.push_back(lump);
}

void WadFile::addDirectory(std::string_view path, void* handle)
{
    auto header = WadHeader {};
    doom_read(handle, &header, sizeof(header));

    if (std::memcmp(header.identification, "IWAD", 4) != 0
        && std::memcmp(header.identification, "PWAD", 4) != 0)
    {
        fatalError("Error: Wad file ", path);
    }

    auto lumpCount = littleEndian(header.numlumps);
    auto entries = EA::Vector<FileLump>(lumpCount);

    doom_seek(handle, littleEndian(header.infotableofs), DOOM_SEEK_SET);
    doom_read(
        handle, entries.data(), static_cast<int>(entries.size() * sizeof(FileLump)));

    for (const auto& entry: entries)
    {
        auto lump = Lump {};

        // A reloadable file's lumps carry no handle: read() re-opens the file
        // every time, which is what makes the reload hack work at all.
        lump.handle = reloadName.empty() ? handle : nullptr;
        lump.position = littleEndian(entry.filepos);
        lump.size = littleEndian(entry.size);

        // Up to eight bytes, stopping at a NUL as the retired doom_strncpy
        // did - a short on-disk name may carry garbage after its terminator,
        // and that garbage must not become part of the name.
        for (auto i = 0; i < 8 && entry.name[i] != '\0'; ++i)
            lump.name[i] = entry.name[i];

        lumps.push_back(lump);
    }
}

int WadFile::find(std::string_view name) const
{
    const auto wanted = LumpName {name};

    // Backwards, so that a lump from a later file takes precedence over the same
    // name in an earlier one. That is how a PWAD overrides the IWAD.
    for (auto lump = count() - 1; lump >= 0; --lump)
        if (wanted.matches(lumps[lump].name.data()))
            return lump;

    return -1;
}

int WadFile::number(std::string_view name) const
{
    auto lump = find(name);

    if (lump < 0)
        fatalError("Error: W_GetNumForName: not found: ", name);

    return lump;
}

int WadFile::length(int lump) const
{
    if (lump < 0 || lump >= count())
        fatalError("Error: W_LumpLength: no such lump: ", lump);

    return lumps[lump].size;
}

void WadFile::read(int lump, void* destination) const
{
    if (lump < 0 || lump >= count())
        fatalError("Error: W_ReadLump: no such lump: ", lump);

    const auto& entry = lumps[lump];
    auto* handle = entry.handle;
    auto reopened = handle == nullptr;

    if (reopened)
    {
        handle = doom_open(reloadName.c_str(), "rb");

        if (handle == nullptr)
            fatalError("Error: W_ReadLump: couldn't open ", reloadName);
    }

    doom_seek(handle, entry.position, DOOM_SEEK_SET);
    auto read = doom_read(handle, destination, entry.size);

    if (reopened)
        doom_close(handle);

    if (read < entry.size)
        fatalError("Error: W_ReadLump: only read part of lump ", lump);
}

const std::byte* WadFile::data(int lump)
{
    if (lump < 0 || lump >= count())
        fatalError("Error: W_CacheLumpNum: no such lump: ", lump);

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
        // How far past the end can it read? Not a number to measure and hope -
        // the drawers say. drawColumn and drawColumnLow index dc_source with
        // `frac.toInt() & 127`, so neither can reach beyond 128 bytes.
        // drawTranslatedColumn is the loose one: its index is unmasked, bounded
        // only by the column it walks, and a patch post carries its length in a
        // single byte, so 255. 256 covers every drawer in Render/Draw.cpp with
        // the bound taken from the code rather than from a sample.
        //
        // It was 64, with a comment saying the over-read was "under that,
        // measured". It is not: 128 is reachable by inspection, and demo1 at tic
        // 5328 and demo2 at tic 392 do reach past 64. That measurement was taken
        // on macOS, where the bytes beyond the guard happened to be zero anyway,
        // so the frame goldens recorded there were *correct* - they simply could
        // not tell the guard from the allocator's luck. On Windows arm64 the same
        // over-read found non-zero heap and those two frames drew differently,
        // which is what a golden recorded on one platform and run on another is
        // for. AddressSanitizer names the read; the size comes from the mask.
        constexpr auto overReadGuard = 256;

        bytes.resize(length(lump) + overReadGuard, std::byte {0});
        read(lump, bytes.data());
    }

    return bytes.data();
}

void WadFile::reload()
{
    if (reloadName.empty())
        return;

    auto* handle = doom_open(reloadName.c_str(), "rb");

    if (handle == nullptr)
        fatalError("Error: W_Reload: couldn't open ", reloadName);

    auto header = WadHeader {};
    doom_read(handle, &header, sizeof(header));

    auto lumpCount = littleEndian(header.numlumps);
    auto entries = EA::Vector<FileLump>(lumpCount);

    doom_seek(handle, littleEndian(header.infotableofs), DOOM_SEEK_SET);
    doom_read(
        handle, entries.data(), static_cast<int>(entries.size() * sizeof(FileLump)));
    doom_close(handle);

    for (auto i = 0; i < lumpCount; ++i)
    {
        auto lump = reloadLump + i;

        lumps[lump].position = littleEndian(entries[i].filepos);
        lumps[lump].size = littleEndian(entries[i].size);

        // Whatever was cached is now the old file's bytes. Drop it; the next
        // caller re-reads.
        cache[lump].clear();
    }
}

void initWadFiles(const EA::Vector<std::string>& filenames)
{
    for (const auto& filename: filenames)
        wad().addFile(filename);

    if (wad().count() == 0)
        fatalError("Error: initWadFiles: no files found");
}

void* cacheLumpNum(int lump)
{
    return const_cast<std::byte*>(wad().data(lump));
}

void* cacheLumpName(std::string_view name)
{
    return cacheLumpNum(wad().number(name));
}
} // namespace Doom
