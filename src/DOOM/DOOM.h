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

enum SeekOrigin
{
    DOOM_SEEK_CUR = 1,
    DOOM_SEEK_END = 2,
    DOOM_SEEK_SET = 0
};

// Doom key mapping
enum Key
{
    DOOM_KEY_UNKNOWN = -1,
    DOOM_KEY_TAB = 9,
    DOOM_KEY_ENTER = 13,
    DOOM_KEY_ESCAPE = 27,
    DOOM_KEY_SPACE = 32,
    DOOM_KEY_APOSTROPHE = '\'',
    DOOM_KEY_MULTIPLY = '*',
    DOOM_KEY_COMMA = ',',
    DOOM_KEY_MINUS = 0x2d,
    DOOM_KEY_PERIOD = '.',
    DOOM_KEY_SLASH = '/',
    DOOM_KEY_0 = '0',
    DOOM_KEY_1 = '1',
    DOOM_KEY_2 = '2',
    DOOM_KEY_3 = '3',
    DOOM_KEY_4 = '4',
    DOOM_KEY_5 = '5',
    DOOM_KEY_6 = '6',
    DOOM_KEY_7 = '7',
    DOOM_KEY_8 = '8',
    DOOM_KEY_9 = '9',
    DOOM_KEY_SEMICOLON = ';',
    DOOM_KEY_EQUALS = 0x3d,
    DOOM_KEY_LEFT_BRACKET = '[',
    DOOM_KEY_RIGHT_BRACKET = ']',
    DOOM_KEY_A = 'a',
    DOOM_KEY_B = 'b',
    DOOM_KEY_C = 'c',
    DOOM_KEY_D = 'd',
    DOOM_KEY_E = 'e',
    DOOM_KEY_F = 'f',
    DOOM_KEY_G = 'g',
    DOOM_KEY_H = 'h',
    DOOM_KEY_I = 'i',
    DOOM_KEY_J = 'j',
    DOOM_KEY_K = 'k',
    DOOM_KEY_L = 'l',
    DOOM_KEY_M = 'm',
    DOOM_KEY_N = 'n',
    DOOM_KEY_O = 'o',
    DOOM_KEY_P = 'p',
    DOOM_KEY_Q = 'q',
    DOOM_KEY_R = 'r',
    DOOM_KEY_S = 's',
    DOOM_KEY_T = 't',
    DOOM_KEY_U = 'u',
    DOOM_KEY_V = 'v',
    DOOM_KEY_W = 'w',
    DOOM_KEY_X = 'x',
    DOOM_KEY_Y = 'y',
    DOOM_KEY_Z = 'z',
    DOOM_KEY_BACKSPACE = 127,
    DOOM_KEY_CTRL = (0x80 + 0x1d), // Both left and right
    DOOM_KEY_LEFT_ARROW = 0xac,
    DOOM_KEY_UP_ARROW = 0xad,
    DOOM_KEY_RIGHT_ARROW = 0xae,
    DOOM_KEY_DOWN_ARROW = 0xaf,
    DOOM_KEY_SHIFT = (0x80 + 0x36), // Both left and right
    DOOM_KEY_ALT = (0x80 + 0x38), // Both left and right
    DOOM_KEY_F1 = (0x80 + 0x3b),
    DOOM_KEY_F2 = (0x80 + 0x3c),
    DOOM_KEY_F3 = (0x80 + 0x3d),
    DOOM_KEY_F4 = (0x80 + 0x3e),
    DOOM_KEY_F5 = (0x80 + 0x3f),
    DOOM_KEY_F6 = (0x80 + 0x40),
    DOOM_KEY_F7 = (0x80 + 0x41),
    DOOM_KEY_F8 = (0x80 + 0x42),
    DOOM_KEY_F9 = (0x80 + 0x43),
    DOOM_KEY_F10 = (0x80 + 0x44),
    DOOM_KEY_F11 = (0x80 + 0x57),
    DOOM_KEY_F12 = (0x80 + 0x58),
    DOOM_KEY_PAUSE = 0xff
};

// Mouse button mapping
enum MouseButton
{
    DOOM_LEFT_BUTTON = 0,
    DOOM_RIGHT_BUTTON = 1,
    DOOM_MIDDLE_BUTTON = 2
};

// The platform hooks the embedder may override, all pre-filled with working
// defaults (stdio file I/O, gettimeofday, printf, malloc; see Host.cpp).
// Assign members of Doom::host() before initGame. Every hook is invoked
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
void setDefaultString(std::string_view name, std::string_view value);

// Initializes DOOM and starts things up. Call only once. argv is main()'s,
// kept by the engine (not copied).
void initGame(int argc, char** argv, int flags);

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
