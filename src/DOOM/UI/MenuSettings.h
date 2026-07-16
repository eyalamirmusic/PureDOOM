#pragma once

namespace Doom
{
// The menu-adjustable, config-persisted user preferences: the mouse sensitivity
// G_BuildTiccmd scales the aim by, the screen-message toggle, and the two that
// shape the rendered frame - the detail level and the view size (screenblocks).
//
// These are config-backed, and stayed loose globals through the earlier sweep
// because Game/Config.cpp's defaults[] table captured their addresses at
// static-init (a reference-alias raced that capture and segfaulted). The
// doom_config->Host rework removed the static capture: Config.cpp now binds the
// defaults[] entries to these members at runtime (bindEngineDefaults), so they
// are owned here like any other cluster (REFACTOR.md, Step 5).
//
// screenblocks and detailLevel change what the frame looks like, so the demo
// frame goldens cover them - but the config sets them from Tests/doom-tests.cfg
// at boot before anything renders, exactly as before, so the migration is
// golden-neutral. The defaults here (all zero) match the vanilla file-scope
// int zero-init; the config overwrites them.
struct MenuSettings
{
    int mouseSensitivity = 0; // has default (5)
    int showMessages = 0;     // has default (1), 0 = off, 1 = on
    int detailLevel = 0;      // has default (0), 0 = high, 1 = normal
    int screenblocks = 0;     // has default (9)
};

MenuSettings& menuSettings();
} // namespace Doom
