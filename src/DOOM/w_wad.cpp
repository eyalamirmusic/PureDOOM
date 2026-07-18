// The vanilla WAD API, now a shim over Doom::WadFile (Wad/WadFile.h), which owns
// the directory, the file handles and the lump bytes.
//
// The thing that actually changed: **a lump pointer is now permanent.** The zone
// used to own cached lumps as PU_CACHE blocks, which were purgeable, which is why
// W_CacheLumpNum had to hand Z_Malloc a back-pointer (`&lumpcache[lump]`) for the
// allocator to null out behind the caller's back - and why every user of a lump
// then had to remember to say Z_ChangeTag or Z_Free when it was done. All of that
// is gone, along with the lumpcache array and the tag argument, which is now
// ignored.
//
// `lumpinfo` and `numlumps` remain because r_things still parses sprite names
// straight out of the directory and r_data still adds up lump sizes. They are a
// view onto the WadFile's own vector, not a second copy.

#include "Wad/WadFile.h"

#include "doom_config.h"

#include "doomtype.h"
#include "i_system.h"
#include "m_swap.h"
#include "w_wad.h"

#include "Host/System.h"
lumpinfo_t* lumpinfo;
int numlumps;

static_assert(sizeof(Doom::Lump) == sizeof(lumpinfo_t),
              "Doom::Lump must be layout-compatible with vanilla's lumpinfo_t");


// The directory moved into the WadFile, so these two follow it wherever its
// vector happens to live.
static void refreshDirectoryView()
{
    numlumps = Doom::wad().count();
    lumpinfo = (lumpinfo_t*) Doom::wad().directory().data();
}

void W_AddFile(char* filename)
{
    Doom::wad().addFile(filename);
    refreshDirectoryView();
}

void W_Reload()
{
    Doom::wad().reload();
    refreshDirectoryView();
}

void W_InitMultipleFiles(char** filenames)
{
    for (; *filenames; filenames++)
        Doom::wad().addFile(*filenames);

    refreshDirectoryView();

    if (numlumps == 0)
        Doom::fatalError("Error: W_InitFiles: no files found");
}

void W_InitFile(char* filename)
{
    char* names[2];

    names[0] = filename;
    names[1] = 0;
    W_InitMultipleFiles(names);
}

int W_NumLumps()
{
    return numlumps;
}

int W_CheckNumForName(const char* name)
{
    return Doom::wad().find(name);
}

int W_GetNumForName(const char* name)
{
    return Doom::wad().number(name);
}

int W_LumpLength(int lump)
{
    return Doom::wad().length(lump);
}

void W_ReadLump(int lump, void* dest)
{
    Doom::wad().read(lump, dest);
}

// The tag is ignored, and there is nothing to release. It is still in the
// signature because two hundred call sites still pass one; they lose it as each
// subsystem is rewritten.
void* W_CacheLumpNum(int lump, int tag)
{
    (void) tag;
    return (void*) Doom::wad().data(lump);
}

void* W_CacheLumpName(const char* name, int tag)
{
    return W_CacheLumpNum(W_GetNumForName(name), tag);
}
