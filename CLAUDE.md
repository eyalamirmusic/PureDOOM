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
  implementation as plain C; `Main.cpp` is the eacp platform layer and GPU
  renderer. `EngineAccess.h/.c` is the plain-C snapshot interface to engine
  internals (camera, wall geometry, view state) — the `.c` is included at the
  end of `DoomImpl.c`'s translation unit, never compiled standalone.

### Renderer status

Two paths, toggled at runtime with **Shift+F8**:

- **Software frame** (Stage A): the engine's palette-indexed framebuffer as
  an R8 texture, palette looked up in the fragment shader. No CPU pixel
  conversion.
- **GPU world** (Stages B1-B3): the level drawn as real hardware 3D, at the
  window's resolution rather than 320x200.
  - Geometry is re-read from the live level every frame, so moving sectors,
    doors, animated textures and moving monsters need no invalidation. Walls
    come from the linedefs (both sides, with vanilla's upper/lower/middle
    pegging rules); floors and ceilings come from subsector polygons
    reconstructed by clipping a large square down the BSP, because vanilla
    nodes carry only split planes and the segs alone would leave holes at BSP
    cuts; every thing in the level is a camera-facing billboard using the same
    eight-rotation frame the engine would pick.
  - Textures are composed from their patches (as `R_GenerateComposite` does),
    which is what makes a masked texture's holes come out as holes: the
    engine's cached columns are post data, not pixels, for exactly those
    textures. Masked textures and sprites carry coverage in alpha and are
    alpha-tested in the shader.
  - Shading is DOOM's own, not an imitation: the texture yields a palette
    index, the COLORMAP row chosen by sector light and distance remaps it,
    and the palette resolves the colour. Light banding, diminishing, fullbright
    frames and palette flashes all come out exact.
  - The sky is a cylinder pinned to the camera, its texture repeating four
    times around, mapped so a screen row lands where the engine would put it.
  - The weapon and muzzle flash are drawn in screen space over the world.
  - Geometry is grouped by texture into one draw per texture; textures upload
    lazily on first use (a WAD holds well over a thousand sprite lumps).
  - Still missing (B4): the screen-melt wipe (needs offscreen render targets),
    spectre fuzz, and the automap. Menus, the automap and wipes fall back to
    the software frame automatically; the status bar is always composited from
    it.
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

The GPU render paths currently require eacp features that only exist on the
local eacp branch `doom-stage-a-gpu-palette` (`TextureFormat::R8Unorm`,
`Buffer::update`), so until that merges, building against GitHub `main`
fails — configure with the local-source override above.

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

### What the engine expects of its host

Two of these are not obvious, and getting either wrong makes the game feel
broken rather than fail outright.

- **Hand it the mouse once per tic, with the whole movement.** `G_Responder`
  *assigns* the mouse delta (`mousex = ev->data2 * ...`) rather than adding to
  it, and `G_BuildTiccmd` consumes and zeroes it once a tic. Posting one
  `doom_mouse_move` per platform mouse event — which arrive several times per
  tic — therefore throws away all but the last, and the aim crawls. Accumulate
  and flush once per tic (`View::flushMouse`), as vanilla's `I_StartTic` does.
  It also stops mouse motion from filling `D_PostEvent`'s 64-slot ring buffer,
  which silently overwrites rather than blocking, and so can swallow
  keystrokes.
- **The game only moves on a tic, 35 times a second.** The display refreshes
  two to four times as often. Step the engine when its own clock
  (`eacpDoomTicCount`) says a tic is due, and rebuild what derives from its
  state — the software frame, the palette, the world's geometry — only then.
  Rendering still runs every refresh.

## eacp Gap Log

Found while porting, newest last. Remove entries once fixed in eacp.

Already added on the eacp branch `doom-stage-a-gpu-palette` (all three were
gaps this port surfaced): `TextureFormat::R8Unorm`, so indexed data — the
framebuffer, wall textures, flats, the COLORMAP — uploads as one byte per
pixel instead of being expanded to RGBA on the CPU; `Buffer::update`, so the
world's geometry buffer is re-uploaded each frame rather than reallocated;
and `ShaderProgram::setDiscardBelow`, an alpha test in the shader EDSL,
without which no sprite or masked texture can be drawn.

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
5. **No declarative window aspect-ratio constraint.**
   `WindowOptions::onWillResize` works for keeping a window 4:3 (the port
   snaps the proposed size to the smaller correction), but macOS has a native
   `NSWindow.contentAspectRatio` that anchors resize better and also governs
   zoom; a `WindowOptions::contentAspectRatio` mapping to it (and to WM_SIZING
   on Windows) would make the callback unnecessary for the common case.
6. **The shader EDSL has almost no scalar maths.** `sin`/`cos` exist (the
   transform builders use them) but there is no `floor`, `fract`, `abs`,
   `min`/`max`/`clamp`, `step` or `mix` for `Float`. B2 dodged this by letting
   the samplers do the work — `Repeat` tiles wall textures instead of `fract`,
   `Nearest` rounds the COLORMAP row instead of `floor`, `Clamp` bounds it
   instead of `clamp` — and it has held up so far, but any shader wanting real
   arithmetic will hit this. (`discard` was the blocking case and is now in.)
7. **No offscreen render targets.** `Frame` only ever renders into the view's
   drawable, so a pass cannot render into a texture and sample it later.
   DOOM's screen-melt wipe needs exactly that (it reads back the previous
   frame), as does any post-processing pass — a CRT/scanline filter over the
   finished frame, for instance.
8. **No cull-mode state** in `RenderPipelineDescriptor`. Not blocking (DOOM's
   walls are fine drawn double-sided), but every triangle is rasterised from
   both faces.

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
