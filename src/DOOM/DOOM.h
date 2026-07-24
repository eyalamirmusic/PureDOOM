//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
//-----------------------------------------------------------------------------
/* The engine's public interface to whatever is hosting it.

   Link the `doom-engine` library and #include <DOOM/DOOM.h>. Call
   Doom::initGame once, Doom::updateGame every frame, and hand it input as it
   arrives. Platform hooks (printing, allocation, file I/O, the clock, exit,
   getenv) are the members of Doom::host() - assign any of them before
   initGame; each has a working default.

   This was extern "C" once, for embedders that were not C++. The engine and
   every consumer of it are C++ now, so the interface is a plain namespace.
*/
//-----------------------------------------------------------------------------

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Doom
{
// Sample rate of sound samples from doom
inline constexpr int DOOM_SAMPLERATE = 11025;

// MIDI tick needs to be called 140 times per seconds
inline constexpr int DOOM_MIDI_RATE = 140;

// Hide menu options. If for say your platform doesn't support mouse or
// MIDI playback, you can hide these settings from the menu.
inline constexpr int DOOM_FLAG_HIDE_MOUSE_OPTIONS = 1;
inline constexpr int DOOM_FLAG_HIDE_SOUND_OPTIONS = 2;
inline constexpr int DOOM_FLAG_HIDE_MUSIC_OPTIONS = 4;

// Darken background when menu is open, making it more readable. This
// uses a bit more CPU and redraws the HUD every frame
inline constexpr int DOOM_FLAG_MENU_DARKEN_BG = 8;

enum class SeekOrigin
{
    Current = 1,
    End = 2,
    Set = 0
};

// Doom key mapping
enum class Key
{
    Unknown = -1,
    Tab = 9,
    Enter = 13,
    Escape = 27,
    Space = 32,
    Apostrophe = '\'',
    Multiply = '*',
    Comma = ',',
    Minus = 0x2d,
    Period = '.',
    Slash = '/',
    Digit0 = '0',
    Digit1 = '1',
    Digit2 = '2',
    Digit3 = '3',
    Digit4 = '4',
    Digit5 = '5',
    Digit6 = '6',
    Digit7 = '7',
    Digit8 = '8',
    Digit9 = '9',
    Semicolon = ';',
    Equals = 0x3d,
    LeftBracket = '[',
    RightBracket = ']',
    A = 'a',
    B = 'b',
    C = 'c',
    D = 'd',
    E = 'e',
    F = 'f',
    G = 'g',
    H = 'h',
    I = 'i',
    J = 'j',
    K = 'k',
    L = 'l',
    M = 'm',
    N = 'n',
    O = 'o',
    P = 'p',
    Q = 'q',
    R = 'r',
    S = 's',
    T = 't',
    U = 'u',
    V = 'v',
    W = 'w',
    X = 'x',
    Y = 'y',
    Z = 'z',
    Backspace = 127,
    Ctrl = (0x80 + 0x1d), // Both left and right
    LeftArrow = 0xac,
    UpArrow = 0xad,
    RightArrow = 0xae,
    DownArrow = 0xaf,
    Shift = (0x80 + 0x36), // Both left and right
    Alt = (0x80 + 0x38), // Both left and right
    F1 = (0x80 + 0x3b),
    F2 = (0x80 + 0x3c),
    F3 = (0x80 + 0x3d),
    F4 = (0x80 + 0x3e),
    F5 = (0x80 + 0x3f),
    F6 = (0x80 + 0x40),
    F7 = (0x80 + 0x41),
    F8 = (0x80 + 0x42),
    F9 = (0x80 + 0x43),
    F10 = (0x80 + 0x44),
    F11 = (0x80 + 0x57),
    F12 = (0x80 + 0x58),
    Pause = 0xff
};

// Mouse button mapping
enum class MouseButton
{
    Left = 0,
    Right = 1,
    Middle = 2
};

// The platform hooks the embedder may override, all pre-filled with working
// defaults (stdio file I/O, gettimeofday, printf, malloc; see Host.cpp).
// Assign members of host() before initGame. Every hook is invoked
// without a null check - keep them non-null, eacp style.
using PrintHandler = std::function<void(std::string_view text)>;
using MallocHandler = std::function<void*(int size)>;
using FreeHandler = std::function<void(void* pointer)>;
using OpenHandler =
    std::function<void*(std::string_view filename, std::string_view mode)>;
using CloseHandler = std::function<void(void* handle)>;
using ReadHandler = std::function<int(void* handle, void* buffer, int count)>;
using WriteHandler = std::function<int(void* handle, const void* buffer, int count)>;
using SeekHandler = std::function<int(void* handle, int offset, SeekOrigin origin)>;
using TellHandler = std::function<int(void* handle)>;
using EofHandler = std::function<int(void* handle)>;
using GetTimeHandler = std::function<void(int* sec, int* usec)>;
using ExitHandler = std::function<void(int code)>;
using GetEnvHandler =
    std::function<std::optional<std::string>(std::string_view name)>;

struct Host
{
    PrintHandler print;
    MallocHandler malloc;
    FreeHandler free;
    OpenHandler open;
    CloseHandler close;
    ReadHandler read;
    WriteHandler write;
    SeekHandler seek;
    TellHandler tell;
    EofHandler eof;
    GetTimeHandler gettime;
    ExitHandler exit;
    GetEnvHandler getenv;

    // The DOOM_FLAG_* set initGame was handed. Host state rather than Engine
    // state - it is what the embedder wants hidden or darkened, the same whichever
    // world is running - so it survives resetEngine() with the callbacks.
    int flags = 0;

    Host();
};

// The one Host. Deliberately separate from the Engine: platform hooks are set
// once by the embedder and survive resetEngine().
Host& host();

// For the software renderer. Default is 320x200
void setResolution(int width, int height);

// Set default configurations. Lets say, changing arrows to WASD as default
// controls. A string value is kept as the view given - pass a literal or
// storage that outlives the engine.
void setDefaultInt(std::string_view name, int value);

template<typename T>
void setDefaultInt(std::string_view name, T value)
{
    setDefaultInt(name, static_cast<int>(value));
}

void setDefaultString(std::string_view name, std::string_view value);

// Initializes DOOM and starts things up. Call only once.
//
// The first form takes main()'s argv as the OS shaped it; the engine copies the
// arguments, so the host is free to let them go. The second is what a host that
// is not forwarding main() should use.
void initGame(int argc, char** argv, int flags);
void initGame(const std::vector<std::string>& args, int flags);

// Call this every frame
void updateGame(); // This will update at 35 FPS
void forceUpdateGame(); // Runs a frame every call, regardless of FPS.

// Channels: 1 = indexed, 3 = RGB, 4 = RGBA
const unsigned char* framebuffer(int channels);

// It is always 2048 bytes in size
short* soundBuffer();

// Call this 140 times per second. Or about every 7ms.
// Returns midi message. Keep calling it until it returns 0.
unsigned long tickMidi();

// Events
void keyDown(Key key);
void keyUp(Key key);
void buttonDown(MouseButton button);
void buttonUp(MouseButton button);
void mouseMove(int deltaX, int deltaY);
} // namespace Doom
