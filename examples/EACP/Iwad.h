#pragma once

#include <eacp/Core/Utils/Environment.h>
#include <eacp/Core/Utils/FilePath.h>
#include <eacp/Network/OnlineResource/OnlineResource.h>

namespace PureDoom
{
// The shareware IWAD is not shipped with the repository. It is fetched once
// through eacp's OnlineResource and kept on this machine, and what is fetched
// is upstream PureDOOM's own copy of doom1.wad - the file this repository
// forked with, byte for byte (SHA-1 5b2e249b9c5133ec987b3ea77596381dc0d6bc1d) -
// so every golden under Tests/Goldens describes the file that arrives.
inline eacp::OnlineResource::Info sharewareIwad()
{
    auto info = eacp::OnlineResource::Info {};
    info.name = "the shareware IWAD";
    info.url = "https://raw.githubusercontent.com/Daivuk/PureDOOM/master/doom1.wad";

    // The engine looks the file up by this name under DOOMWADDIR, so it is
    // stated here rather than taken from the URL's last segment.
    info.fileName = "doom1.wad";

    // Shareware 1.9 is the last shareware there will ever be. Bumping this is
    // the one way to make a machine that already holds a copy fetch again.
    info.version = "1.9";
    return info;
}

// One place on the machine whichever binary asks: the app's own support
// directory, spelled out so the tests and the bench put the file where the app
// will find it rather than each keeping a copy under their own name.
inline eacp::FilePath iwadDirectory()
{
    return eacp::FilePath::appSupportDirectory({}, "PureDoomEACP") / "Resources";
}

// Points DOOMWADDIR at a directory holding doom1.wad and returns it. A
// DOOMWADDIR the user already set wins outright, so a developer pointing at
// another WAD still gets it and nothing is fetched. Otherwise the shareware IWAD
// is downloaded when this machine has no copy, and a copy it has is trusted
// without asking the server: the file never changes, and an offline machine
// must still boot - a failed revalidation would refuse the copy it holds (the
// eacp gap log in CLAUDE.md). Throws when the file cannot be had. Main thread
// only, before any event loop runs.
inline eacp::FilePath ensureIwad()
{
    if (auto chosen = eacp::getEnv("DOOMWADDIR"))
        return eacp::FilePath {*chosen};

    auto options = eacp::OnlineResource::Options {};
    options.info = sharewareIwad();
    options.directory = iwadDirectory();
    options.freshness = eacp::OnlineResource::Freshness::trust;

    auto directory = eacp::OnlineResource::fetch(options)->parentDirectory();
    eacp::setEnv("DOOMWADDIR", directory.str());
    return directory;
}

// Where the IWAD is once ensureIwad has run, for a test that reads the file
// itself rather than through the engine.
inline eacp::FilePath iwadPath()
{
    return eacp::FilePath {eacp::getEnvValue("DOOMWADDIR")} / "doom1.wad";
}
} // namespace PureDoom
