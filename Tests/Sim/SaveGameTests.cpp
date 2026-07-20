// p_saveg, tested directly - the one simulation path the demo goldens do not
// watch.
//
// A demo is input; nothing in a .lmp saves or loads a game, so Doom::archiveThinkers
// / Doom::unArchiveThinkers and the whole mobj/special byte layout ride through the
// suite unpinned. That layout is exactly what the Doom::Thinker -> Thinker
// virtualisation and the mobj/special zone-ownership change will rewrite: a vtable
// on Doom::Mobj shifts every offset the archive memcpy's, and no golden would notice.
// So the net has to exist before those steps do - the Step-0 move once more.
//
// The assertion is a round trip: build a distinctive world, archive it, reload a
// fresh base level and unarchive over it (precisely doLoadGame's sequence), and
// require the world - sectors, lines, sides, every mobj and the player - to come
// back byte-for-byte in its meaningful, restored fields. It is sharp for the same
// reason the demos are: a single serialized field read at the wrong offset moves
// the hash.

#include "../Common.h"

using namespace nano;

namespace
{
constexpr int unit = 1 << 16;

constexpr int e1 = 1;
constexpr int m1 = 1;
constexpr int skillMedium = 2;

// A freshly loaded E1M1 is already a rich world for this: dozens of things (the
// player, the monsters, the barrels and pickups, each in a spawn state) plus the
// sector light specials Doom::spawnSpecials starts at load - so Doom::archiveThinkers,
// Doom::archiveWorld and Doom::archiveSpecials all have real content to round-trip. A
// barrel spawned on top adds a mobj created after load, not one the map placed,
// which is the case a naive "re-run Doom::setupLevel" restore would get wrong.
auto tSaveLoadPreservesTheWorld = test("Sim/saveLoadPreservesTheWorld") = []
{
    check(doomSimBoot() != 0, "the engine booted headless");
    check(doomSimLoadLevel(e1, m1, skillMedium) != 0,
          "E1M1 loaded and the player spawned");

    auto player = doomSimPlayerHandle();
    check(player == 0, "the player is handle 0");

    auto sx = doomSimMobjX(player);
    auto sy = doomSimMobjY(player);

    auto barrel =
        doomSimSpawnMobj(doomSimTypeBarrel(), sx + 64 * unit, sy, doomSimOnFloorZ());
    check(barrel > 0, "a runtime barrel was added to the world");

    check(doomSimSaveLoadPreservesWorld() == 1,
          "archive -> reload -> unarchive brought the world back unchanged");
};

// Doom::readFile, which the test above does not reach.
//
// doomSimSaveLoadPreservesWorld drives save.cursor against its own scratch buffer,
// so it covers the serialization and never the file layer beneath it - and readFile
// has exactly one caller in the engine, doLoadGame. That left it uncovered while
// Step 9 strand (b) rewrote its out-parameter from a host-malloc'd byte** the caller
// had to free into an EA::Vector<byte>& it fills itself. This is the property
// doLoadGame actually depends on: the owner comes back sized to the file, holding
// the file.
//
// It reads the IWAD because that is a real file of known content that is certain to
// be present (the suite boots from it), and writes nothing - the round trip through
// writeFile would test the host's file callbacks as much as readFile.
auto tReadFileFillsTheOwner = test("Sim/readFileFillsItsOwner") = []
{
    check(doomSimBoot() != 0, "the engine booted headless");

    auto contents = doomSimReadFileIntoOwner(PUREDOOM_ROOT_DIR "/doom1.wad");

    check(contents.length > 0, "the IWAD read back a non-empty file");
    check(contents.length == contents.ownerSize,
          "the owner is sized to exactly the length readFile returned");
    check(contents.magicIsIwad != 0, "the bytes are the IWAD, not zeroes");
};
} // namespace
