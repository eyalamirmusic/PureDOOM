# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with
code in this repository.

## Project Overview

This began as a fork of [Daivuk/PureDOOM](https://github.com/Daivuk/PureDOOM) —
the single-header DOOM source port — to port DOOM's platform layer (video,
input, timing; later audio and music) to
[eacp](https://github.com/eyalamirmusic/eacp).

It no longer tracks upstream. **This repository owns `src/DOOM`** and modifies
it freely; the engine itself is being refactored into modern code, physics and
game logic included. Divergence from upstream PureDOOM is deliberate and
permanent, and nothing here needs to be upstreamable.

The project now has three goals:

1. Run DOOM on eacp's application, GPU and input stack.
2. Exercise eacp as a game platform layer and surface what it is missing.
   Every missing feature or rough edge found while porting is recorded in the
   gap log below.
3. Refactor the engine itself, behind the safety net described under
   **Testing** below. Read that section before touching anything in `src/DOOM`:
   DOOM's simulation is exactly reproducible, and the tests exist to keep it
   that way through a rewrite.

## The C++ refactor is underway — read `REFACTOR.md` first

The engine is being rewritten from 1993 C into modern C++ in eacp's style,
step by step. **`REFACTOR.md` is the plan and the running state of it**: which
step we are on, what each one changes, and the rules the whole thing rests on.
Read it before touching `src/DOOM`, and update its progress table as steps land.

The end state: no C left, no `PureDOOM.h`, no zone allocator, and no ~684
globals — an `Engine` object constructed rather than booted. (Scenario tests, once
thought to depend on that, do not: the engine already runs many scenarios per
process — see `Tests/Sim/ReplayTests.cpp` — because loading a level resets the
simulation cleanly. The `Engine` object is what retires the loose globals, not
what unblocks the tests.)

The four rules that matter most, because breaking one silently defeats the whole
apparatus:

1. **A refactor never re-records a *simulation* golden** (`*.hashes`). Those pin
   the world, and the world does not change. A red `*.hashes` suite is telling you
   the refactor was wrong, not that the golden is stale. The *frame* goldens
   (`*.frames`) hold to the same bar with one measured exception: DOOM's renderer
   reads a few bytes past a lump's end (tutti-frutti), an undefined value that
   depends on the allocator — so when Step 4 replaced the allocator, 15 pixels
   across ~2,300 frames moved and those frames were re-recorded, once. The test:
   re-record a frame golden *only* when the pixels that moved are provably not part
   of any lump (`REFACTOR.md`, Step 4 tells the story). `record-goldens` otherwise
   exists for intended behaviour changes, of which this refactor has none.
2. **The simulation probe's hash is append-only.** `Tests/SimProbe` may change
   *how* it finds state; it may never change *what* it mixes, or in what order.
3. A file leaves the engine's blanket `-w` and comes under `clang-format` the
   moment it is genuinely rewritten, and not before. Vanilla filenames stay until
   then, so a half-refactored file is still diffable against the 1993 source.
4. Some things that look like bugs are load-bearing and must survive: `FixedDiv2`
   goes through `double`; the trig tables are sampled at bucket centres;
   `SlopeDiv` gives up under 512; `R_PointToAngle2` lands one unit below north.
   `Tests/Sim/PrimitiveTests.cpp` pins three of the four on purpose.

## Layout

- `src/DOOM/` — **the engine, and the code we own.** Built as the `doom-engine`
  static library, which both the app and the tests link, so a change to the
  simulation reaches both and neither can run code the other is not.

  It is **C++20 as of Step 2 of `REFACTOR.md`**, and there is no C left anywhere
  in the repository — but the flat files are still 1993 C that a compiler now
  accepts, not C++ anyone wrote.

  **The subdirectories are the rewrite.** `Math/` (`Fixed`, `Angle`, `Trig`,
  `BBox`, `Vec2`), `Sim/` (`Random`, `Level`, `MapGeometry`), `Wad/` (`WadFile`)
  and `Engine/` (`Engine`, the composition root that owns `Random`/`WadFile`/
  `Level`) are real C++ in `namespace Doom`, and a file moves into one the moment
  it stops being vanilla.
  Progress is the flat list getting shorter. The two are compiled differently on
  purpose: rewritten sources get `-Wall -Wextra -Wpedantic` and clang-format from
  their first line; vanilla keeps a blanket `-w` and its formatting exemption until
  someone rewrites it. Both are set per-file in `src/DOOM/CMakeLists.txt`, so the
  line moves as the work does.

  The vanilla API is still there — `FixedMul`, `finesine`, `P_Random`,
  `prndindex`, `W_CacheLumpNum`, `vertexes`, `numsegs` — because most of the engine
  still calls it. But `m_fixed.cpp`, `tables.cpp`, `m_bbox.cpp`, `m_random.cpp`,
  `w_wad.cpp` and the geometry loaders in `p_setup.cpp` are now **shims/views over
  owning objects**, not the owners: one copy of the arithmetic, one copy of the
  tables, one supply of chance, one owner of the lumps, one owner of the level
  geometry. `rndindex`/`prndindex` are *references* into `Doom::Random`;
  `vertexes`/`numsegs`/`sectors`/… are pointer-and-count *views* onto
  `Doom::Level`'s vectors, refreshed by each loader after it fills its vector.
  That is deliberate — it puts the new types on the critical path of every demo
  the suite replays, which is the only thing that can test them.

  Those three owners live inside one `Doom::Engine` now (`Engine/Engine.h`), and
  `randomness()`/`wad()`/`level()` are accessors into the single `engine()`
  instance. The `Engine` is the composition root that the ~684 scalar globals
  (`doomstat.h`, `r_state.h`, `p_local.h`) migrate into — but each cluster moves
  *with* the subsystem that owns it, as Steps 6–8 rewrite that code to take an
  `Engine&`, not speculatively ahead of it.

  Three constraints died with the single header in Step 1, and the code may now
  rely on their absence: **two files may share a file-scope name** (the header
  concatenated every `.c` into one translation unit, which is why `am_map.cpp`'s
  `plr` had to become `am_plr`); a source file may include a system header; and
  the header include graph need not be acyclic.

  One trap survives the flip and is load-bearing: **`doom_boolean` is an `int`,
  not a `bool`** (`doomtype.h` says why at length). Vanilla reads booleans through
  pointers to other types — `ST_createWidgets` binds the ARMS widget with
  `(int*) &plyr->weaponowned[i + 1]`, its own cast — and a one-byte `bool` makes
  those reads garbage. Turning it into a real `bool` is a change to the engine's
  behaviour, not its spelling, and belongs to a later step, one subsystem at a
  time with the demos watching.
- `Tests/` — the test suite. See **Testing**.
- `examples/EACP/` — the eacp port. `Main.cpp` boots the engine, `View.h` is the
  eacp platform layer and GPU renderer, and `EngineAccess.h/.cpp` is the plain-C
  snapshot interface to engine internals (camera, wall geometry, view state).
  `EngineAccess.cpp` is an ordinary translation unit that includes the engine's
  headers; nothing DOOM-typed leaks out through the `.h`.

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
- `doom1.wad` — the shareware data file the game boots with.

Upstream's SDL reference port used to live in `examples/SDL` and was the best
worked example of how the engine expects to be driven. It was deleted with the
single header in Step 1 — it was a C consumer of it — and what it knew that this
repository did not is written down under **What the engine expects of its host**
below. `git log -- examples/SDL` still has it if you want to read it.

## Build

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug -DCPM_eacp_SOURCE=$HOME/Code/eacp
cmake --build build --target PureDoomEACP

./build/examples/EACP/PureDoomEACP.app/Contents/MacOS/PureDoomEACP
```

Targets: `doom-engine` (the engine, from `src/DOOM`), `PureDoomEACP` (the game),
`SimTests` and `PrimitiveTests` (see **Testing**), and `record-goldens`. That is
all of them — there are only two build options left, `PUREDOOM_BUILD_TESTS` and
`PUREDOOM_BUILD_EACP_EXAMPLE`.

The tests need no GPU and no eacp — they link `doom-engine` alone — so
`-DPUREDOOM_BUILD_EACP_EXAMPLE=OFF` gives a fast engine-only loop while
refactoring. That is also what CI builds, for the same reason.

eacp is fetched from GitHub via CPM. To co-develop against a local eacp
checkout, pass `-DCPM_eacp_SOURCE=$HOME/Code/eacp` at configure time. Use
`$HOME`, not `~` — CMake does not expand tildes, and a quoted `~/...` path
silently configures against a non-existent directory.

The GPU render paths need four eacp features this port surfaced
(`TextureFormat::R8Unorm`, `Buffer::update`, `ShaderProgram::setDiscardBelow`,
and the raw-mouse/warp input fixes). These have since **merged to eacp `main`**,
so the default CPM fetch builds the app cleanly — the local-source override is now
only for co-developing eacp, not a requirement. (It used to be: the features
lived on the branch `doom-stage-a-gpu-palette` and building against `main`
failed.)

The app boots `doom1.wad` from the repository root by default: PureDOOM has
no `-iwad` argument — it locates WADs via the `DOOMWADDIR` environment
variable (falling back to the current directory), so `main` points
`DOOMWADDIR` at the repo root unless the user already set it. Other classic
DOOM arguments (`-warp`, `-skill`, `-episode`, ...) pass straight through.

## Testing

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug -DCPM_eacp_SOURCE=$HOME/Code/eacp
cmake --build build
ctest --test-dir build --output-on-failure
```

Roughly two seconds for the lot. **Run it before and after anything you change
in `src/DOOM`.**

Run the binaries through ctest, not bare. NanoTest registers one ctest case per
test and re-runs the binary with `--test <name>`, so every test gets a fresh
process — which the engine needs, being several hundred globals and a zone
allocator that `doom_init` does not undo. A bare `./SimTests` puts all of them in
one process and only the first can boot; it says so rather than quietly passing.

### The demo tests are the safety net

DOOM's simulation is exactly reproducible: fixed-point arithmetic, and a fixed
256-byte random table walked by an index. That is why demos work at all — a
`.lmp` is nothing but the player's input, one ticcmd per tic, and `G_ReadDemoTiccmd`
feeds the game from it instead of from the keyboard. Identical input against a
deterministic simulation must produce an identical world.

So a demo *is* the assertion, and there is nothing to hand-write. `Tests/Sim/DemoTests.cpp`
replays the shareware WAD's three attract-mode demos (11,410 tics of real play:
combat, damage, death, respawn, doors, lifts, and a level's worth of monsters
thinking), hashes the world after every tic, and holds it against
`Tests/Goldens/*.hashes`. On a mismatch it reports **the first diverging tic**,
with where the player was standing and what the random index had reached.

It is extremely sharp. Changing `FRICTION` from `0xe800` to `0xe801` — one part
in 59,392, invisible to any player — desyncs demo1 at tic 48.

`prndindex` is the canary. `P_Random` drives the simulation and `M_Random` does
not, and they keep separate indices for exactly that reason (the screen melt
calls `M_Random`, which is why a wipe cannot desync the game). Add, drop or
reorder a single `P_Random` call and everything after it shifts.

**When a change to behaviour was intended**, re-record:

```bash
cmake --build build --target record-goldens
```

The diff on `Tests/Goldens/` is then the reviewable record of what you changed.
Re-recording to make a red suite go green is the one thing that defeats the whole
apparatus — the goldens pin the simulation *as it stands, bugs included*, which
is exactly what a behaviour-preserving refactor wants held still.

### The same replay also watches the renderer

`D_DoomLoop` calls `D_Display` every tic, so the software renderer has been
running throughout the suite the whole time — it was simply that nobody looked at
what it produced. Now the replay hashes that too: `screens[0]`, the finished
320x200 palette-indexed frame, together with the live palette (the damage, pickup
and invulnerability flashes are palette swaps, and change nothing else). Every 4th
tic, against `Tests/Goldens/*.frames`.

They are **separate goldens on purpose**, and the failure says which moved. A
renderer refactor that desyncs the simulation is a very different bug from one
that merely draws it wrong, and the suite should not make you guess which you
have.

This is what pins `r_*`, the status bar and the HUD through the rewrite, and it is
sharp: adding 1 to the light-level start map in `R_SetViewSize` — one COLORMAP row,
a shade — fails demo1 at tic 4 while the simulation golden sails through.

Two things had to be true for it to work at all, and both were already true:

- **The melt does not read the clock.** Vanilla's `D_Display` busy-waits on
  `I_GetTime` to advance the wipe (it is still there, `#if 0`'d). PureDOOM moved
  it into `D_UpdateWipe`, which advances by exactly one tic per call.
- **The config is pinned.** `M_LoadDefaults` reads the developer's real
  `~/.doomrc` unless given `-config`, and `screenblocks` alone (10 in a real
  config, 9 by default) changes the shape of every frame. `SimProbe` boots against
  `Tests/doom-tests.cfg` instead, so the frames mean the same thing on every
  machine. The simulation never cared — a demo's input comes from the `.lmp` — so
  this changed no existing golden, and that was checked.

### The engine runs many scenarios per process

`Tests/Sim/ReplayTests.cpp` replays a demo a second time in one process (identical
tic for tic) and loads a *different* demo over the first (matches its own
fresh-boot golden). Together they prove the per-level reset is clean — the thing
that makes scenario tests possible and the only test of `Doom::Level`'s reload
(grow/shrink) path. `Tests/Sim/LevelTests.cpp` separately checks the view invariant
after a single load: that `vertexes`/`numsegs`/… still equal their `Level` vector's
`data()`/`size()`.

### What the tests do not cover

Audio, and the eacp platform layer and GPU renderer. The **menu** used to be
here — nothing in a demo opens one — but it now has its own frame golden
(`Tests/Goldens/menu.frames`, via `Tests/MenuReplay.h`): synthetic `doom_key_down`
events drive a scripted walk through the menus over the title screen, hashed every
tic, the same Step-0 technique the renderer got. That golden was built *before*
`m_menu` was rewritten into `UI/Menu` and holds it byte-identical. The rest still
needs the app run and looked at.

### The WAD directory has its own golden

`Tests/Sim/WadTests.cpp` walks all 1,264 lumps of `doom1.wad` and hashes each
one's bytes as `W_CacheLumpNum` hands them over, against
`Tests/Goldens/doom1.lumps`. A demo would notice a corrupt lump only as a desync
at some tic with no explanation; this names the lump.

It was built for Step 4 of `REFACTOR.md`, which took the zone allocator out from
under the lump cache — the one refactor with no other net, since `PU_CACHE`, the
purge rover and the `**user` back-pointers *were* what `W_CacheLumpNum` was built
on. It did its job: `Doom::WadFile` (`src/DOOM/Wad/`) now owns the lumps, and this
test held it to reading every byte identically while it took over.

### The primitive tests give locality

`Tests/Sim/PrimitiveTests.cpp` covers the arithmetic underneath the simulation —
fixed-point, the random table, the trig tables, the geometry helpers. When a demo
desyncs at tic 48, these say *which primitive* stopped agreeing with itself.

Three of them pin things that look like bugs and are not. A refactor will want to
"fix" all three, and all three would desync every demo ever recorded:

- **The trig tables are sampled at bucket centres**, so `finesine[0]` is 25 and
  not 0, and `sin(90°)` comes out as 65535 rather than 65536.
- **`SlopeDiv` gives up on any denominator under 512** and answers `SLOPERANGE`,
  so `SlopeDiv(0, 1)` is 2048, not 0.
- **`R_PointToAngle2` lands one unit below the exact cardinal** — due north is
  `ANG90 - 1` — inheriting the same half-bucket offset. (Due south is exact, being
  reached by negating rather than by a lookup.)
- **`M_AddToBox` / `BBox::add` is `else if`, not an independent min and max.** On
  a fresh (inverted) box, one point moves `left` and leaves `right` at its
  sentinel — a point cannot be both below the minimum and above the maximum in a
  single call — and points fed in descending x never write `right` at all. The
  engine gets away with it (`P_GroupLines` feeds whole linedefs), but min/max
  changes what a sector's bounding box comes out as, and therefore what the
  renderer and `P_BlockLinesIterator` see. `Tests/Sim/MathTests.cpp` pins it.
- **DOOM reads past the end of a lump — tutti-frutti — and it is preserved, not
  fixed.** A wall texture shorter than the column it fills makes the renderer draw
  whatever memory follows the patch; the value is undefined and was the same on
  every machine only because the old zone was one contiguous arena. `WadFile::data`
  keeps that true by giving each lump a 64-byte zero tail, so the over-read still
  happens but draws a deterministic zero everywhere. Do not "fix" the over-read in
  the renderer — it is a visible 1993 behaviour, and the frame goldens are recorded
  with it.
- **`pointOnLineSide` and `pointOnDivlineSide` are different formulae** (both in
  `Sim/MapGeometry.h`) and must stay different. The line version shifts one factor
  of the cross product by `FRACBITS`; the divline version shifts both by 8 and has
  a sign-bit fast path that decides most cases without multiplying. They answer the
  same for a point clearly off the line but not identically at the margins, and the
  collision/BSP/sight code depends on the specific one it calls. Merging them into
  one "clean" side test desyncs the demos. `Tests/Sim/GeometryTests.cpp` pins both.

Also worth knowing before touching `m_fixed.cpp`: **`FixedDiv2` goes through
`double`.** The simulation is therefore not strictly integer-only. It is still
deterministic — the same IEEE-754 operation every time, which is why demos replay
— but anyone rewriting `FixedDiv` in pure integer arithmetic will change the
rounding and desync the game.

Those are all spot-checks, which is the right shape for a property and the wrong
shape for a transcription. So the tables are *also* checksummed whole — `finesine`,
`finetangent`, `tantoangle`, `rndtable`, `states[]` and `mobjinfo[]`, every entry.
Step 3 of `REFACTOR.md` turns `tables.cpp` (2,130 lines) and `info.cpp` (4,663) into
`constexpr` arrays, and a spot-check would happily pass over one mistyped digit in
the middle of 16,000 numbers. A failure prints the new checksum, so you can see
what you did and decide whether you meant it.

(`states[]` is hashed without its `action` pointer, which is a function address
and differs between builds of the same engine.)

### Layers still to build

- **Scenario tests**: load a level, place mobjs, run tics, assert. `P_TryMove`
  into a wall, `P_DamageMobj`, `P_CheckSight`, door and lift specials. Write
  these per-subsystem *as* you refactor it — they are how you get locality on
  code the demos only cover in aggregate. **Unblocked** as of Step 4: the engine
  runs many scenarios per process (it never needed re-`doom_init`, only a clean
  per-level reset, which the `Level` object completed — see
  `Tests/Sim/ReplayTests.cpp`). What is still missing is only *probe surface* —
  functions to place a mobj, set the player, read back a result — which is
  additive and lands with the playsim rewrite.
- **Port-layer tests**: `View`'s tic/interpolation state machine is still not
  testable, because it lives inside a `GPU::GPUView` whose members construct GPU
  textures from `GPU::Device::shared()`. Extracting it into a plain GPU-free
  struct that `View` owns and delegates to is the prerequisite, and it is where
  the port's subtlest bugs have lived (the double-clock-read that drew frames a
  tic in the past, the five-tic input lag, mouse accumulate-and-flush).

## Porting Rules

- eacp is never modified from this repository. When the port hits something
  eacp cannot do, implement a workaround here, and record it in the gap log
  below. eacp changes happen in the eacp repo itself and get picked up via
  `CPM_eacp_SOURCE`.
- **The engine is ours to change.** `src/DOOM` is edited directly, and the
  refactor's whole point is to change it. There is nothing to regenerate and
  nothing downstream to keep in step: the library is the only artifact.

  What *does* hold you back is the engine's behaviour, which the demo tests pin
  exactly — the simulation *and*, since Step 0, the frames drawn of it. Read
  **Testing** before changing anything under `src/DOOM`.

  The engine's headers are also the interface, and several things a renderer
  needs were `static` in a `.c` and only reachable because the single-header
  build made one translation unit of everything. Those are now exported properly
  (`am_map.h` has the automap's state and shapes, `f_wipe.h` the melt's,
  `r_data.h` the texture composition types, `m_random.h` the two random
  indices). Export the next one the same way rather than reaching around it.

  One older fix predates all this (`d_net.cpp`, `NetUpdate`). PureDOOM runs with
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
- The engine is single-threaded: `doom_init`, `doom_update`,
  `doom_get_framebuffer` and all input calls happen on the main thread. Audio,
  once wired, is the only exception — it is pulled from the audio callback, on
  another thread, and must take a lock against `doom_update`.

### What the engine expects of its host

Two of these are not obvious, and getting either wrong makes the game feel
broken rather than fail outright.

- **Audio, when it is wired** (nothing here plays a sound yet — gap-log item 1).
  This is what the deleted SDL example demonstrated and nothing else in the
  repository records. Sound is a **pull** model: run an output stream at
  `DOOM_SAMPLERATE` (11025 Hz), 16-bit stereo, 512 samples — 2,048 bytes a
  buffer — and call `doom_get_sound_buffer(len)` from the audio callback, taking
  the engine lock around it, because `doom_update` is on another thread. Music is
  a **push** model: a 140 Hz timer (`DOOM_MIDI_RATE`) draining `doom_tick_midi()`
  into a synth for as long as it keeps returning messages. Resample if your device
  wants another rate; the engine only ever produces that one.

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
