# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with
code in this repository.

## Project Overview

This is a fork of [Daivuk/PureDOOM](https://github.com/Daivuk/PureDOOM) — the
single-header DOOM source port — whose purpose is to port DOOM's platform
layer (video, input, timing; later audio and music) to
[eacp](https://github.com/eyalamirmusic/eacp).

The port has two goals:

1. Run DOOM on eacp's application, GPU and input stack.
2. Exercise eacp as a game platform layer and surface what it is missing.
   Every missing feature or rough edge found while porting is recorded in the
   gap log below.

## Layout

- `PureDOOM.h` — the upstream engine as a single header, generated from
  `src/DOOM` by `tools/gen_single_header.py`. Vendored code: never hand-edit
  and never clang-format it (same for `src/DOOM`).
- `examples/EACP/` — the eacp port. `DoomImpl.c` compiles the engine
  implementation as plain C; `Main.cpp` is the eacp platform layer.
- `examples/SDL/` — upstream's SDL3 reference port. Read-only; the best
  reference for how the engine expects to be driven (input mapping, audio
  stream format, MIDI tick).
- `doom1.wad` — the shareware data file the game boots with.

## Build

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target PureDoomEACP

./build/examples/EACP/PureDoomEACP.app/Contents/MacOS/PureDoomEACP
```

eacp is fetched from GitHub via CPM. To co-develop against a local eacp
checkout, pass `-DCPM_eacp_SOURCE=$HOME/Code/eacp` at configure time. Use
`$HOME`, not `~` — CMake does not expand tildes, and a quoted `~/...` path
silently configures against a non-existent directory.

The app boots `doom1.wad` from the repository root by default: PureDOOM has
no `-iwad` argument — it locates WADs via the `DOOMWADDIR` environment
variable (falling back to the current directory), so `main` points
`DOOMWADDIR` at the repo root unless the user already set it. Other classic
DOOM arguments (`-warp`, `-skill`, `-episode`, ...) pass straight through.

## Porting Rules

- eacp is never modified from this repository. When the port hits something
  eacp cannot do, implement a workaround here, and record it in the gap log
  below. eacp changes happen in the eacp repo itself and get picked up via
  `CPM_eacp_SOURCE`.
- Vendored DOOM sources (`PureDOOM.h`, `src/DOOM`) stay untouched apart from
  upstreamable bug fixes.
- The engine is single-threaded: `doom_init`, `doom_update`,
  `doom_get_framebuffer` and all input calls happen on the main thread. Audio,
  once wired, is the only exception and takes the engine lock the SDL example
  demonstrates.

## eacp Gap Log

Found while porting, newest last. Remove entries once fixed in eacp.

1. **No audio subsystem.** Sound effects need a pull-model PCM output stream
   (`DOOM_SAMPLERATE` = 11025 Hz, 16-bit stereo, mixed via
   `doom_get_sound_buffer()`); music needs a 140 Hz (`DOOM_MIDI_RATE`) tick
   draining `doom_tick_midi()` into a synth. Both are unwired; the game runs
   silent.
2. **KeyCode table gaps.** `Graphics::KeyCode` has no codes for punctuation
   (`,` `.` `-` `=` `[` `]` `;` `'` `/`), and modifier keys (Ctrl/Shift/Alt)
   produce no key events at all — DOOM binds them as ordinary keys (Ctrl =
   fire, Shift = run, Alt = strafe). Workarounds: punctuation is mapped from
   `KeyEvent::charactersIgnoringModifiers`; modifiers are diffed once per
   frame from polled `Window::getModifiers()` into synthetic down/up events.
3. **CPM consumers don't get app-bundle setup.** `eacp_default_setup()` (which
   sets `EACP_MACOS_PLIST` and the deployment target) only runs when eacp is
   the top-level project, so `set_default_target_setting()` on a consumer app
   target would stamp an empty Info.plist template. Workaround:
   `examples/EACP/CMakeLists.txt` sets the `EACP_MACOS_PLIST` cache variable
   itself from `eacp_SOURCE_DIR`.
4. **No display-metrics API.** `Window` uses `NSScreen`/`GetSystemMetrics`
   internally but nothing public reports the screen's visible size, so an app
   cannot pick an initial window size that fits the display (a 4x-scale DOOM
   window overflowed a laptop screen), nor clamp/center itself. Workaround: a
   conservative 3x default plus a resizable window with letterboxed rendering.

## Code Style

Applies to the port code in `examples/EACP` (and any future non-vendored
C++). Vendored C keeps upstream style.

Always use the most modern C++ and RAII practices.
Use auto for variables and whenever possible.
Don't use auto for functions and member functions.

Don't use comments unless absolutely needed. Use named functions to make code
self documenting.

Give std::function members a non-null default — a no-op lambda, or one
returning an empty value — so call sites invoke them directly without null
checks.

Member variables use plain names (no trailing underscores); constructor
parameters that would shadow a member get a `ToUse` suffix. Pass by
`const T&` whenever possible.

Enforced via `.clang-format` (copied from eacp):
- Allman brace style
- 85 column limit
- 4-space indentation (no tabs)
- Pointer alignment: left (`int* ptr`)
- Break constructor initializers before comma

Always run clang-format for edited code files — but never on vendored DOOM
sources.
