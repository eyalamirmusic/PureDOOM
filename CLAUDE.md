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
  implementation as plain C; `Main.cpp` boots the engine and `View.h` is the eacp
  platform layer and GPU renderer. `EngineAccess.h/.c` is the plain-C snapshot
  interface to engine internals (camera, wall geometry, view state) — the `.c` is
  included at the end of `DoomImpl.c`'s translation unit, never compiled
  standalone.

  The six shaders share `DoomShader.h`: `DoomShader` resolves a palette index the
  way the software renderer does (index → COLORMAP row → palette), and
  `ScreenQuadShader` adds the screen-space quad the four full-frame passes draw.
  Every shader is the difference from those, and nothing else.

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

    A powerup can take that choice away entirely: `R_SetupFrame` reads
    `player->fixedcolormap` and puts every wall, flat, sprite and the weapon
    through one row with no light and no distance — the invulnerability sphere's
    inverse map (row 32), the light-amp visor's brightest row (row 1). The whole
    COLORMAP lump is therefore uploaded, all 34 rows, not the 32 the light
    calculation can reach, and each vertex carries not just its row but *how much
    of the distance term applies* — one for an ordinary surface, zero for one the
    engine has locked. Which is also why that is a vertex attribute rather than a
    uniform: **the sky is exempt from the powerups** and stays on row 0, a vanilla
    quirk its own source calls out ("Because of this hack, sky is not affected by
    INVUL inverse mapping").
  - The sky is a cylinder pinned to the camera, its texture repeating four
    times around, mapped so a screen row lands where the engine would put it.
  - The weapon and muzzle flash are drawn in screen space over the world.

    The weapon is **not** lit at its sector's start map, and lighting it that way
    (as this port first did) draws it far too dark in almost every room in the
    game. `R_DrawPSprite` reads `spritelights[MAXLIGHTSCALE-1]` — the *nearest*
    entry of the scale table, the weapon being right up against the camera — which
    is 23 rows brighter than the start map at a 320-column view. DOOM's weapon is
    therefore fullbright in any sector above light level 240 and close to it well
    below that, and the start map alone lights it as though it stood infinitely
    far away: at light level 96 the row comes out at 31 (near-black) where vanilla
    picks 13.
  - Geometry is grouped by texture into one draw per texture; textures upload
    lazily on first use (a WAD holds well over a thousand sprite lumps).
- **GPU automap**: the map as geometry rather than as a rasterized frame. What
  it draws and the colour it picks are `AM_Drawer`'s own choices, mirrored in
  `eacpDoomBuildAutomap`; only its Bresenham walk into the 320x168 frame is
  replaced, by a quad per line that the vertex shader widens (the perpendicular
  needs a length, and the GPU normalizes for free). The shapes it draws the
  player and the things with, and the rotation it puts them through, are the
  engine's own globals, used as they stand. Two things vanilla's rasterizer
  cannot do come out of it: the lines keep their real endpoints instead of
  snapping to whole pixels, and the map is recentred on the *interpolated* view
  rather than on the player's last tic, so it glides at the display's rate
  instead of crawling at 35Hz. Zoom and hand-panning still step, being the
  engine's own per-tic quantities.

  **The one place the port departs from vanilla on purpose**
  (`eacpDoomRevealAutomap`). A wall is revealed on the map as a *side effect of
  being drawn*: `R_StoreWallRange` sets `ML_MAPPED` as the software renderer lays
  it down. But `D_Display` skips `R_RenderPlayerView` entirely while the automap
  is up, so vanilla's map stops filling in the moment you look at it, and only
  catches up when you close it — walking with the map open reveals nothing
  (measured: the mapped-line count sits frozen while the player moves). Most
  source ports quietly fix this, and so does this one: the BSP is walked once a
  tic while the map is up, which marks what the player can see. It stops there —
  the planes and the sprites are never drawn, and `R_RenderPlayerView`'s four
  `NetUpdate` calls are not wanted, as they drain the event queue. The walls it
  *does* draw on the way land in the frame the automap had just drawn itself into
  (the column drawers write through `ylookup`, which was aimed at `screens[0]`
  when the view size was set and does not follow it anywhere), so the map is
  drawn again afterwards to put it back — which the software fallback needs, as
  it reads that whole frame, and the GPU path does not, reading only the status
  bar from it.
- **Overlay** (`eacpDoomBuildOverlay`): the layers the engine draws over the
  view in software and nothing else reproduces - HUD messages, the level name,
  the PAUSE graphic, the menu, the automap's marks. Without it a menu forced the
  whole screen back to the software frame, so the world visibly dropped to
  320x200 the moment one opened; messages and PAUSE were not drawn at all in the
  GPU path, which sampled only the status-bar rows from the software frame.

  The engine offers no way to draw them anywhere but over the frame it has just
  rendered, so they are captured: `screens[0]` is pointed at scratch, those
  drawers alone are run, and the real frame is put back. Coverage cannot be read
  off one pass - a pixel the menu legitimately drew may hold whatever the
  scratch was primed with - so each layer is drawn twice over two differently
  primed buffers and counts as covered exactly where the two agree. They are
  pure (the skull blinks on `M_Ticker`, not `M_Drawer`), so wherever anything
  was drawn they agree by construction.

  It is captured as *two* layers, because a menu darkens the frame it finds and
  then draws itself over it: a message, the level name and PAUSE are already on
  the screen by then and dim with the world, while the menu stays bright. The
  green channel says which, and picks the COLORMAP row.
- **Menu darkening** is applied to the GPU view rather than to a framebuffer:
  one extra COLORMAP lookup in the world, automap, weapon and overlay shaders
  (`eacpDoomDarkenRow`). That is exactly what `M_Drawer` does to its 64000
  pixels, and it leaves the world at full resolution behind the menu. Row 0 is
  the identity, so playing costs the lookup and nothing else. (It is not quite
  the identity on the *index* - it folds the palette's seven duplicate entries
  onto their twins - but it is the identity on the colour they resolve to.) The
  status bar needs none of this: the engine darkens its own frame, which is
  where the strip is sampled from.
- **Screen melt**: drawn over the GPU view, not instead of it, which is what
  keeps the level it is revealing at the window's resolution. It was the last
  thing forcing the whole screen back to 320x200, and the most conspicuous once
  the menu and automap stopped doing so: `G_DoLoadLevel` wipes on every level
  start, so a new level arrived as a low-resolution image that snapped up to full
  resolution a second later, when the melt ended.

  It needs **no offscreen render target**, contrary to what this log used to say.
  The melt only ever *reads* the outgoing frame; what it composites is

      column c, row r = the outgoing frame's row (r - offset[c]) when
                        r >= offset[c], and the incoming frame's row r otherwise

  so "the incoming frame" is just the framebuffer left alone - the level the GPU
  has already drawn there. Only the outgoing frame becomes a texture, and it is a
  320x200 software frame whatever happens (it is the title or intermission
  screen, which is 320x200 artwork), so nothing is lost by it staying one. The
  rows a column has not reached are discarded in the shader, and the level shows
  through. `eacpDoomBuildWipe` hands over that frame and the column offsets,
  un-transposing the outgoing screen that `wipe_initMelt` left column-major.

  Two things it has to respect. The engine raises `is_wiping_screen` at the end
  of the frame that renders the incoming screen and only sets the melt up on the
  *next* one, so on that first frame there is no column table yet and the whole
  outgoing screen is still what should be on the screen. And `wipe_exitMelt`
  frees the column table without clearing the pointer to it, so `go` - the melt's
  own "I am set up" flag - is the only safe thing to test.
- **Screen size** (the menu's, which persists in `~/.doomrc` — and whose default
  is **9**, not 10, so this is never hypothetical). The GPU renderer honours the
  two layouts that change what is on the screen and no others: with the status bar
  (the 168 rows above it) and without it (screenblocks 11, the whole 320x200
  frame). At a *smaller* size it keeps drawing the full-width view rather than
  shrinking it into a border, that having been a concession to 1993 hardware and
  not something this renderer needs to reproduce.

  Ignoring 11 was not an option, only a bug: the engine then renders the view over
  all 200 rows and `ST_Drawer` draws no bar at all (its widgets hang off
  `st_statusbaron`), so the strip composited from the software frame stops being
  the status bar and becomes a 320x200 slice of the world. The world is drawn over
  the whole frame instead, and no strip is composited. A taller view also wants a
  wider vertical field of view, which is one more scale on the projected y rather
  than a second projection — the horizontal 90 degrees is unchanged, the view being
  full-width either way.
- Still missing (B4): spectre fuzz. Anything outside a level (title,
  intermission, finale) falls back to the software frame automatically, which is
  right - those screens *are* 320x200 - and the status bar is always composited
  from it.
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
  upstreamable bug fixes. `PureDOOM.h` is *generated* from `src/DOOM` — edit the
  `src/DOOM` file and regenerate (`cd tools && python3 gen_single_header.py`),
  never the header directly.

  One such fix is in place (`d_net.c`, `NetUpdate`). PureDOOM runs with
  `singletics = true`, whose `D_DoomLoop` path builds a tic's command and runs it
  in the same breath, advancing `maketic` and `gametic` together. But `NetUpdate`
  is also called from `D_Display` and `R_RenderPlayerView` — vanilla called it
  there to keep the netcode fed while a slow frame rendered — and each of those
  calls advanced `maketic` with no `gametic` to match. `maketic` therefore
  climbed until it jammed against the `BACKUPTICS/2-1` cap and stayed there
  permanently, and since `D_DoomLoop` writes the command to `netcmds[maketic]`
  while `G_Ticker` reads `netcmds[gametic]`, **every command was executed five
  tics (143ms) after it was built** — aim, movement and fire alike. `NetUpdate`
  now builds no command when `singletics` is set (it still drains events). This
  took the aim's input-to-screen lag from 163ms to 17ms, and stopped the player
  coasting for five tics after a movement key was released.

  That fix is currently the *only* vendored change.
- The engine is single-threaded: `doom_init`, `doom_update`,
  `doom_get_framebuffer` and all input calls happen on the main thread. Audio,
  once wired, is the only exception and takes the engine lock the SDL example
  demonstrates.

### What the engine expects of its host

Two of these are not obvious, and getting either wrong makes the game feel
broken rather than fail outright.

- **The keys the app asks for do not stick by themselves.** DOOM cannot rebind a
  key from inside the game, yet it still writes every binding out to `~/.doomrc`
  and, at startup, reads them back *over* whatever `doom_set_default_int` asked
  for. A config left behind by an older build therefore pins that build's keys
  for good, and changing the binding in `Main.cpp` silently does nothing at all —
  the game keeps the old key with no sign anything was ignored. `Main.cpp` calls
  `eacpDoomBindKeys()` after `doom_init` to apply them again once the config has
  been read. What the player *can* change from the menu (mouse sensitivity,
  screen size, volumes) is left alone and still persists.

  Not every key can be bound, either. `HU_Responder` **eats** the key
  `HU_MSGREFRESH` sits on (Enter): `G_Responder` asks the HUD before it touches
  `gamekeydown` and returns the moment the HUD says it took the event, so Enter
  never reaches `gamekeydown` and cannot be a game key without moving the
  refresh off it. `use` is therefore bound to vanilla's own Space, which nothing
  else reads.

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
  (`eacpDoomTicTime`) says a tic is due, and rebuild what derives from its
  state — the software frame, the palette, the world's geometry — only then.
  Rendering still runs every refresh.
- **Do not draw the camera straight from the engine.** It would then sit still
  for two or three frames and jump, which reads as lag however fast the frames
  arrive — this is what made the game feel sluggish. `View::viewCamera`
  interpolates the position across the tic it is part-way through, and runs the
  *aim* ahead: the mouse movement gathered since the last tic is the turn the
  engine is about to make (`pendingTurn` reproduces its formula, sensitivity
  included), so applying it now makes the view follow the mouse every frame
  with no lag, and the engine's angle lands exactly where the view already was
  when the tic applies it.
- **Place everything between tics with the engine's clock, not the display's**
  (`eacpDoomTicTime`). A tic lasts 28.6ms and a frame 8.3ms, so a ramp paced by
  the display saturates early on some tics and is cut short on others.
- **Read that clock exactly once a frame**, and take both answers from the one
  reading: whether a tic is due, and how far into the tic the frame sits. Ask
  twice — once to decide the tic, once to place the frame — and a tic boundary
  can fall between the two asks, so the fraction wraps back to nothing while the
  state it is placed between is still the *previous* tic's. The frame is then
  drawn a whole tic in the past: a jump backwards, then a jump forwards to
  recover, on a few percent of frames. That is what "jumps in the wrong
  direction" was.
- **Everything that moves on the tic has to be placed between tics too**, or it
  jitters against a world that glides — and the engine keeps no previous state,
  so each one is reconstructed differently:
  - the **heading** is split by where the turn came from (`View::viewAngle`;
    Shift+F7 drops back to plain interpolation to compare). What the *keyboard*
    turned is interpolated — a held key turns at a steady rate, and interpolating
    is what makes that read as smooth. What the *mouse* turned is applied at
    once, and the view then runs *ahead* by the movement the engine has not been
    handed yet, which is the turn it is about to make anyway. Interpolating the
    heading instead would cost a whole tic of lag on the one thing the hand is
    holding. GZDoom does exactly this: `R_InterpolateView` gives the local
    player's yaw as `curYaw + LocalViewAngle` and never interpolates it.
    The mouse is *not* filtered, and must not be: what looked like noise in it
    (one steady sweep measuring -10, -30, -13, -12, -14, -24, -10) was the
    system's pointer acceleration, and eacp now hands a locked window the raw
    device movement instead, which is linear. GZDoom smooths nothing either.
    Running ahead is safe on it because what it runs ahead on is the
    *accumulated* mouse, not the last delta, so a mouse's per-sample raggedness
    integrates away. Measured against a deliberately ragged sweep, running ahead
    was not merely no worse but far steadier than interpolating (frame-to-frame
    wobble 0.3ms against 10.2ms) — interpolation quantises the aim to the tic and
    then has to swing between the results.
  - **things** (monsters, items) are wound back from where the tic left them by
    their own momentum, which the engine already stores.
  - **floors and ceilings** a door or a lift is driving come from
    `eacpDoomSnapshotTic`, taken before each tic runs. The walls that meet them
    read the same numbers, so nothing tears.
  - the **weapon** is interpolated from the previous tic's HUD sprites.
- **Billboards and the sky must be built around the camera being drawn from**,
  not the engine's. Built for the engine's heading, a sprite sits progressively
  edge-on as the view turns within a tic and visibly pulses; hence
  `eacpDoomBuildGeometry` takes the view camera and the geometry is rebuilt per
  frame rather than per tic.

## eacp Gap Log

Found while porting, newest last. Remove entries once fixed in eacp.

Already added on the eacp branch `doom-stage-a-gpu-palette` (all four were
gaps this port surfaced): `TextureFormat::R8Unorm`, so indexed data — the
framebuffer, wall textures, flats, the COLORMAP — uploads as one byte per
pixel instead of being expanded to RGBA on the CPU; `Buffer::update`, so the
world's geometry buffer is re-uploaded each frame rather than reallocated;
`ShaderProgram::setDiscardBelow`, an alpha test in the shader EDSL, without
which no sprite or masked texture can be drawn; and three **input fixes**, all
found by comparing against GZDoom:

- **`MouseEvent::rawDelta`** — a second movement figure alongside `delta`.
  `delta` is the *pointer's* movement, shaped by the system's acceleration
  curve; `rawDelta` is the *device's*, with no curve. The curve exists so a
  cursor can cross a screen and land on a target, and a widget dragged by the
  pointer (a knob, a scrubber) should follow it — but through it, the same
  flick of the hand turns a camera a different amount depending how fast it
  was made, which reads as the aim being unpredictable. That is why GZDoom
  (which takes the device's figures through SDL) feels instant. Both are now
  always reported and the caller picks. macOS reads the unaccelerated fields
  off the CGEvent; Windows uses Raw Input, which also escapes the
  whole-pixel rounding and screen-edge clamping of its warp-to-centre lock.
- **The mouse lock's cursor warp** was reported as motion the user had made —
  measured at −222 px in a single event, enough to spin a locked camera round
  the instant you clicked. The warp now marks itself and that one delta is
  dropped. (eacp's comment had asserted the disassociate-first ordering
  prevented this; it does not. GLFW compensates for the same behaviour.)
- **`GPUView::setFramesInFlight`** — exposed so a view can choose how many frames
  the renderer has on the go at once. Note that the two backends mean different
  things by it and **only DXGI's is a latency knob**. On DXGI it is the depth of
  the present queue, and two is the default because a third queued frame is a
  third refresh of delay. On Metal it is `maximumDrawableCount`: the size of the
  pool of buffers the layer hands out to draw into, *not* a queue of finished
  frames. A display-link-driven view presents one frame per refresh either way,
  so shrinking the pool dequeues nothing — it just means `nextDrawable` may find
  no free buffer and block the calling thread, and that wait lands between
  sampling the input and drawing with it. Lowering it to two therefore *raises*
  latency on Metal (measured: sample-to-screen 23ms at three, 32ms at two), and
  three is the Apple default. This port should not lower it.

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
9. **A `View` cannot reach the `Window` it is in.** Anything a view needs from
   its window — the mouse lock, the modifier keys — has to be handed to it by the
   app. eacp's own `Apps/GPU/Maze` assigns a `Graphics::Window*` back-pointer
   after construction and null-checks it at every use; this port instead declares
   the window *before* the view and hands it over as a `Graphics::Window&` at
   construction, which makes it impossible to be null and removes the checks. That
   is a workaround, not a fix: it constrains member order in `App`, and it only
   works because a `Window` needs no content view to exist. A `View::getWindow()`,
   or a window reference given to the view on `setContentView`, would settle it
   properly for every view that locks the mouse.

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

Use eacp's own containers as they are meant to be used. `Vector<T>` (eacp's
re-export of `EA::Vector`) is deliberately **`int`-indexed and `int`-sized** —
call `resize`, `assign`, `size` and `operator[]` on it directly and index it with
plain `int`. Reaching through `getVector()` for the underlying `std::vector`, or
casting indices to `std::size_t`, is working against it.

Enforced via `.clang-format` (copied from eacp):
- Allman brace style
- 85 column limit
- 4-space indentation (no tabs)
- Pointer alignment: left (`int* ptr`)
- Break constructor initializers before comma

Always run clang-format for edited code files — but never on vendored DOOM
sources.
