#pragma once

#include <ea_data_structures/Structures/Array.h>
#include <ea_data_structures/Structures/Vector.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace Doom
{
// One entry of a WAD's directory: vanilla's lumpinfo_t, and the only one now -
// the `lumpinfo` / `numlumps` view onto it went with w_wad.cpp. Render/Things
// still reads sprite names straight out of the directory (it parses TROOA1 into a
// frame and a rotation) and Render/Data adds up lump sizes; both ask info() for
// the entry rather than indexing a parallel array.
struct Lump
{
    EA::Array<char, 8>
        name; // eight bytes, and NOT null-terminated when it fills them
    void* handle; // the file it came from
    int position;
    int size;
};

// The WAD, and everything in it.
//
// This replaces the zone allocator's role as the lump cache, which was the
// tangled half of z_zone: PU_CACHE blocks were purgeable, so W_CacheLumpNum had
// to hand Z_Malloc a back-pointer (`&lumpcache[lump]`) for the allocator to null
// out behind the caller's back, and every user of a lump then had to say
// Z_ChangeTag when it was done. All of that is gone. A WadFile owns its lumps for
// as long as it lives, so **a pointer to lump data stays valid**, and there is
// nothing to tag, purge or release.
//
// It costs what the WAD costs. doom1.wad is 4MB and the zone was 12MB, so the
// zone got smaller by more than this class holds.
class WadFile
{
public:
    ~WadFile();

    // Vanilla's rules: a `.wad` is a directory of lumps, anything else is a
    // single lump named after the file, and a leading `~` marks the file
    // reloadable (see reload()).
    void addFile(std::string_view path);

    int count() const { return lumps.size(); }

    // -1 when there is no such lump, which is a question several callers ask -
    // the engine checks for TEXTURE2 before deciding it is playing DOOM II.
    // A directory Lump's own 8-byte name is NOT NUL-terminated when it fills
    // them: wrap it in nameView(name.data(), 8) rather than letting the
    // string_view constructor run off its end.
    int find(std::string_view name) const;

    // The same, but a missing lump is fatal. Most callers want this one: a WAD
    // without PLAYPAL is not a WAD anyone can render.
    int number(std::string_view name) const;

    int length(int lump) const;

    // Straight into the caller's buffer, uncached.
    void read(int lump, void* destination) const;

    // Cached, and owned. The bytes stay put for the life of the WadFile, so a
    // caller may hold this pointer for as long as it likes.
    const std::byte* data(int lump);

    const Lump& info(int lump) const { return lumps[lump]; }
    const EA::Vector<Lump>& directory() const { return lumps; }

    // Vanilla's `~file` hack: re-read the reloadable file's directory and drop
    // whatever it had cached, so a level can be edited and reloaded without
    // restarting. Its own source calls it fragile. Nothing exercises it unless a
    // WAD is passed with a leading tilde.
    void reload();

private:
    void addSingleLump(std::string_view path, void* handle);
    void addDirectory(std::string_view path, void* handle);

    EA::Vector<Lump> lumps;

    // One buffer per lump, filled the first time it is asked for and empty until
    // then. Indexed alongside `lumps`, so the two never disagree about how many
    // lumps there are.
    EA::Vector<EA::Vector<std::byte>> cache;

    EA::Vector<void*> handles;

    std::string reloadName;
    int reloadLump = 0;
};

// The engine's one WAD, for as long as the engine has one of everything.
WadFile& wad();

// The boot-time WAD list: add every file in turn, and refuse to go on if the lot
// of them yielded no lumps at all. Doom::addWadFile builds the list this consumes;
// nothing else calls it.
void initWadFiles(const EA::Vector<std::string>& filenames);

// A lump's bytes, cached and owned. Vanilla's W_CacheLumpNum / W_CacheLumpName
// took a purge tag as well; there is nothing to purge now that WadFile owns every
// lump for the life of the process, so there is no tag. Still `void*` because each
// caller casts it to the structure it knows is in there - a Patch, the palette, a
// column of a composite texture.
void* cacheLumpNum(int lump);
void* cacheLumpName(std::string_view name);
} // namespace Doom
