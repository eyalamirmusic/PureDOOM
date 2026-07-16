#pragma once

namespace Doom
{
// The config-backed control settings: the keyboard bindings G_Responder tests (movement, strafe,
// fire, use, strafe-modifier, run), the mouse and joystick button/axis bindings, the device
// enables (usemouse/usejoystick), the always-run and crosshair toggles. All are persisted to the
// config and adjusted only from outside the game (the config file, or the app's key setup).
//
// These stayed loose globals through the earlier sweep for the same reason the volumes did:
// Game/Config.cpp's defaults[] table captured their addresses at static-init, which a
// reference-alias would race. The doom_config->Host rework removed that (Config.cpp binds the
// defaults[] entries to these members at runtime, bindEngineDefaults), so they move in like the
// rest of the config-backed set (REFACTOR.md, Step 5). The app's eacpDoomBindKeys reaches them
// the same way - through defaults[].location, which the runtime bind points at these members.
//
// Not demo-covered (a demo feeds ticcmds directly, bypassing the bindings), so verified by build +
// app-link; a reference alias is pure storage relocation, behaviour-preserving by construction.
// All default to 0, matching the vanilla file-scope int zero-init the config then overwrites.
struct InputConfig
{
    int key_right = 0;
    int key_left = 0;
    int key_up = 0;
    int key_down = 0;
    int key_strafeleft = 0;
    int key_straferight = 0;
    int key_fire = 0;
    int key_use = 0;
    int key_strafe = 0;
    int key_speed = 0;

    int mousebfire = 0;
    int mousebstrafe = 0;
    int mousebforward = 0;
    int mousemove = 0;

    int joybfire = 0;
    int joybstrafe = 0;
    int joybuse = 0;
    int joybspeed = 0;

    int usemouse = 0;
    int usejoystick = 0;
    int crosshair = 0;
    int always_run = 0;
};

InputConfig& inputConfig();
} // namespace Doom
