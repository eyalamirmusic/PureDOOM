// Doom::initTextures decodes TEXTURE1 straight out of the lump, each record at
// whatever byte offset the lump's directory names - which is usually not
// aligned for the record's int fields (BIGDOOR1's record sits at offset % 4 ==
// 2, and UBSan flagged every read through the overlaid MapTexture). The decode
// therefore goes through aligned copies of the on-disk structs, and this pins
// the parse itself: the expected values were read from doom1.wad's TEXTURE1 by
// hand (struct.unpack, not the engine), so a restructured decode has to agree
// with the lump, not merely with its former self.
//
// The demos cover these textures too, but only as pixels three stages later -
// a mis-parsed patchcount would fail a frame golden with no word of why. This
// names the field.

#include "../Common.h"

using namespace nano;

namespace
{
auto tTextureDirectory = test("Sim/textureDirectoryDecodes") = []
{
    check(doomSimBoot() != 0, "engine booted");

    check(doomSimTextureCount() == 125, "the shareware TEXTURE1 names 125 textures");

    // The first record, and the only 4-aligned one of these three.
    const auto aastinky = doomSimTextureNumForName("AASTINKY");
    check(aastinky == 0);
    check(doomSimTextureWidth(aastinky) == 24);
    check(doomSimTextureHeight(aastinky) == 72);
    check(doomSimTexturePatchCount(aastinky) == 2);
    check(doomSimTexturePatchOriginX(aastinky, 1) == 12);
    check(doomSimTexturePatchOriginY(aastinky, 1) == -6);

    // The record UBSan flagged: offset % 4 == 2.
    const auto bigdoor = doomSimTextureNumForName("BIGDOOR1");
    check(bigdoor == 1);
    check(doomSimTextureWidth(bigdoor) == 128);
    check(doomSimTextureHeight(bigdoor) == 96);
    check(doomSimTexturePatchCount(bigdoor) == 5);
    check(doomSimTexturePatchOriginX(bigdoor, 1) == 0);
    check(doomSimTexturePatchOriginY(bigdoor, 1) == 24);
    check(doomSimTexturePatchOriginX(bigdoor, 2) == 17);
    check(doomSimTexturePatchOriginY(bigdoor, 2) == 0);

    // The last record, with a patch whose origins are both negative.
    const auto tekwall = doomSimTextureNumForName("TEKWALL5");
    check(tekwall == 124);
    check(doomSimTextureWidth(tekwall) == 128);
    check(doomSimTextureHeight(tekwall) == 128);
    check(doomSimTexturePatchCount(tekwall) == 1);
    check(doomSimTexturePatchOriginX(tekwall, 0) == -120);
    check(doomSimTexturePatchOriginY(tekwall, 0) == -8);
};
} // namespace
