#pragma once

#include <string>

namespace Doom
{
// The config file paths: basedefault is the resolved default location (~/.doomrc,
// or the DEVDATA path in dev mode) that doomMain fills in at startup, and
// defaultfile is the file loadDefaults/saveDefaults actually read and write -
// either basedefault or the -config override argument.
//
// Unlike the option globals in Config.cpp's defaults[] table, neither of these
// has its address captured at static-init (defaultfile is *assigned* basedefault
// at runtime, not bound to it), so they were never actually config-blocked - they
// just lived at file scope because that is where their owners kept them. They move
// into the Engine like any other per-run state (REFACTOR.md, Step 5).
struct ConfigPaths
{
    std::string basedefault; // default config file location
    std::string defaultfile; // the config file actually used
};

ConfigPaths& configPaths();
} // namespace Doom
