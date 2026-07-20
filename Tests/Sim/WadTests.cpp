// The WAD directory as the reader sees it.
//
// Nothing in the demo tests can tell you whether Doom::cacheLumpNum handed back the
// right bytes - a corrupt lump would desync the simulation, but so would a
// thousand other things, and the demo would only say "tic 48". This says
// "SW18_7, 4096 bytes, wrong".
//
// It exists for Step 4 of REFACTOR.md, which takes the zone allocator out from
// under the lump cache. That is the one refactor with no other net: PU_CACHE,
// the purge rover and the **user back-pointers are exactly what Doom::cacheLumpNum
// is built on, and replacing them with real ownership is the kind of change that
// can hand back plausible-looking rubbish.

#include "Common.h"
#include "DemoReplay.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace nano;
using namespace DoomTests;

namespace
{
struct Lump
{
    std::string name;
    int size = 0;
    std::uint64_t hash = 0;

    bool operator==(const Lump&) const = default;
};

using Directory = std::vector<Lump>;

std::string lumpsPath()
{
    return std::string {PUREDOOM_TESTS_DIR} + "/Goldens/doom1.lumps";
}

Directory readDirectory()
{
    auto directory = Directory {};

    for (auto lump = 0; lump < doomSimLumpCount(); ++lump)
    {
        auto name = std::array<char, 9> {};
        doomSimLumpName(lump, name.data());

        directory.push_back(
            {name.data(), doomSimLumpSize(lump), doomSimLumpHash(lump)});
    }

    return directory;
}

void writeGoldenDirectory(const Directory& directory)
{
    auto file = std::ofstream {lumpsPath()};

    file << "# doom1.wad as Doom::cacheLumpNum hands it over: name, bytes, hash.\n"
         << "# Re-record with DOOM_UPDATE_GOLDENS=1, and only on purpose.\n";

    for (const auto& lump: directory)
        file << lump.name << ' ' << std::dec << lump.size << ' ' << std::hex
             << lump.hash << '\n';

    std::printf("recorded %zu lumps -> %s\n", directory.size(), lumpsPath().c_str());
}

Directory readGoldenDirectory()
{
    auto file = std::ifstream {lumpsPath()};
    auto directory = Directory {};
    auto line = std::string {};

    while (std::getline(file, line))
    {
        if (line.empty() || line.front() == '#')
            continue;

        auto stream = std::istringstream {line};
        auto lump = Lump {};
        auto hash = std::string {};

        stream >> lump.name >> lump.size >> hash;
        lump.hash = std::stoull(hash, nullptr, 16);

        directory.push_back(lump);
    }

    return directory;
}

auto tWadDirectory = test("Sim/wadDirectory") = []
{
    check(doomSimBoot() != 0, "engine booted");

    auto directory = readDirectory();
    check(!directory.empty(), "the WAD has lumps in it");

    if (updatingGoldens())
    {
        writeGoldenDirectory(directory);
        return;
    }

    auto golden = readGoldenDirectory();

    if (golden.empty())
    {
        std::printf("\nNo lump golden. Record one with DOOM_UPDATE_GOLDENS=1\n\n");
        check(false, "golden exists");
        return;
    }

    check(directory.size() == golden.size(), "the WAD has the same number of lumps");

    const auto shared = std::min(directory.size(), golden.size());

    for (auto i = std::size_t {0}; i < shared; ++i)
    {
        if (directory[i] == golden[i])
            continue;

        std::printf("\nlump %zu differs\n"
                    "  golden: %s, %d bytes\n"
                    "  now:    %s, %d bytes\n\n",
                    i,
                    golden[i].name.c_str(),
                    golden[i].size,
                    directory[i].name.c_str(),
                    directory[i].size);

        check(false, "the lump reader returns the same bytes");
        return;
    }
};
} // namespace
