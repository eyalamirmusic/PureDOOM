# The C++ refactor

`src/DOOM` is 43,889 lines of 1993 C across 62 `.c` files: ~684 global data
symbols, ~1,534 file-scope statics, a 12 MB zone arena that is never handed
back, and warnings disabled wholesale (`-w`). This fork owns that code, and this
document is the plan for rewriting it as modern C++ in eacp's style — playsim,
renderer, WAD, UI and game loop alike — **without changing what the simulation
does.**

Three decisions frame the work:

- **The whole engine goes.** No permanent C/C++ seam.
- **The globals go with it.** The end state is an `Engine` object rather than
  ~684 loose globals. That is not cosmetic — it is what will let the engine be
  *constructed*, not just booted, so a test owns its world instead of borrowing
  the process's. (Scenario tests — *load MAP01, place an imp, run 20 tics,
  assert* — turned out to need less than that: loading a level already resets the
  simulation cleanly, so the engine runs many scenarios per process today. See
  Step 4.)
- **`PureDOOM.h`, `tools/gen_single_header.py`, `examples/SDL` and
  `examples/Tests` are retired.** Nothing builds against them, and they are the
  only reason two files in the engine may not share a file-scope name.

The invariant that makes any of this safe is already in place. DOOM's simulation
is exactly reproducible, and `Tests/` replays 11,410 tics of recorded demo input
against a per-tic hash of the world. **The simulation goldens (`*.hashes`) come
out byte-identical after every step below, and are never re-recorded.** If one
moves, the refactor was wrong — that is the whole apparatus.

The *frame* goldens (`*.frames`) are held to almost the same standard, with one
documented exception that has now happened exactly once. They hash the rendered
picture, and DOOM's renderer has a 1993 quirk — tutti-frutti — where it reads a
few bytes past the end of a lump and draws whatever memory follows. That value is
undefined by DOOM's own design, and it depends on the allocator. Step 4 replaced
the allocator, so at 15 pixels across ~2,300 frames the garbage changed. Those
frames were re-recorded, once, with the allocator's new (deterministic) tail —
see Step 4. The simulation was bit-identical throughout; nothing about the *world*
changed, only undefined bytes landing differently. That is the bar: a frame golden
is re-recorded only when the pixels that moved are provably not part of any lump.

## Progress

| Step | What | State |
|---|---|---|
| 0 | Widen the net to the renderer, the WAD and the tables | **done** |
| 1 | Retire the single-header packaging | **done** |
| 2 | The language flip: 62 `.c` → 62 `.cpp`, atomically | **done** |
| 3 | The core: leaves first (`Fixed`, `Angle`, `Trig`, `Random`) | **done** |
| 4 | Ownership: kill the zone allocator | **payoff delivered** — WAD + `Level` geometry own their memory, multi-scenario replay proven; zone's *deletion* deferred into Steps 6–7 (its last users are mobjs/thinkers and renderer `PU_STATIC`) |
| 5 | The `Engine` object: globals become members | **in progress** — composition root owns `Random`/`WadFile`/`Level`; scalar clusters move in with Steps 6–8 |
| 6 | The playsim | **in progress** — `Vec2` + the map-geometry core (`p_maputl` side/intercept/distance/opening helpers) rewritten with unit tests; the scenario-test harness (place a mobj, drive `P_TryMove`/`P_CheckPosition`, read blockmap linking) is built and proven to bite |
| 7 | The renderer | |
| 8 | UI, game loop, host boundary | |

## Where this is — session handoff

Everything below is committed on branch **`C++Refactor`**; the working tree is
clean and the suite is green (**74 tests**, ~2s: `ctest --test-dir build`). Steps
0–3 are complete; 4's payoff is delivered; 5 and 6 are underway.

**What exists in modern C++** (`src/DOOM/`, `namespace Doom`, `-Wall` + clang-format;
everything else is still vanilla C compiled as C++ under `-w`):

- `Math/` — `Fixed`, `Angle`, `Trig`, `BBox`, `Vec2`.
- `Sim/` — `Random`, `Level` (level geometry, RAII), `MapGeometry`
  (`pointOnLineSide` / `pointOnDivlineSide` / `interceptVector`).
- `Wad/` — `WadFile` (owns lumps, RAII).
- `Engine/` — `Engine`, the composition root owning `Random`/`WadFile`/`Level`;
  `randomness()`/`wad()`/`level()` are accessors into the one `engine()`.

Everywhere the vanilla API survives (`FixedMul`, `finesine`, `P_Random`,
`W_CacheLumpNum`, `vertexes`, `P_PointOnLineSide`, …) it is a **shim/view** over
those owners, not a second implementation — so the new code sits on the critical
path of every demo and cannot go untested.

**The scenario-test harness has landed** — the enabler the rest of Step 6 rests on.
`Tests/SimProbe` now loads a level directly (`doomSimLoadLevel` → `G_InitNew`, no
demo, single-player forced so the player mobj spawns), spawns and places mobjs
(`doomSimSpawnMobj`, plain-int handles into a probe-side registry invalidated on
each level reload), and drives `P_CheckPosition` / `P_TryMove` on a handle, reading
the result and the mobj's position/flags back. `SimProbe.h` stays free of DOOM
types — the tests see integers and named-constant accessors (`doomSimTypeBarrel`,
`doomSimFlagNoClip`). `Tests/Sim/ScenarioTests.cpp` uses it for three collision
facts the demos only cover in aggregate, each shown to bite by mutation (see the
"Landed so far" note below).

**The immediate next step** (continuing Step 6, `p_maputl` → `p_map`), now with the
harness in hand:

1. The stateful half of `p_maputl` — the blockmap iterators
   (`P_BlockLinesIterator`/`P_BlockThingsIterator`, function-pointer callbacks →
   consider lambdas), thing-position linking (`P_SetThingPosition`/`Unset`),
   `P_PathTraverse`. The **net is now in place**: `P_LineOpening`'s pure core and
   `P_AproxDistance` are already extracted into `MapGeometry.h` with unit tests, and
   scenario tests pin the thing-position linking and `P_BlockThingsIterator` with
   locality (see below). What remains is the wholesale rewrite of the file into
   modern C++ and its flip out of the `-w` blanket — a per-file move, so the whole
   of `p_maputl.cpp` (the still-stateful iterators, intercepts and `P_PathTraverse`)
   goes at once, with the demos and these scenario tests holding it bit-identical.
2. Then `p_map` (`P_TryMove`, `P_CheckPosition`) itself — the scenario tests above
   pin it directly and specifically now.

**How to verify, every step** (nothing here re-records goldens):

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug -DCPM_eacp_SOURCE=$HOME/Code/eacp
cmake --build build
ctest --test-dir build --output-on-failure     # 74 tests, ~2s
git diff --stat Tests/Goldens/                 # MUST be empty
cmake --build build --target PureDoomEACP      # app still links (touches EngineAccess)
```

`-DPUREDOOM_BUILD_EACP_EXAMPLE=OFF` gives the fast engine-only loop.

**Traps already paid for, do not rediscover:** `doom_boolean` must stay `int` (not
`bool`); `pointOnLineSide` and `pointOnDivlineSide` are different formulae on
purpose; deleting a statement can silently orphan an unbraced `if`/`for` body;
changing a renderer allocator re-triggers tutti-frutti on composite pixel blocks;
the engine cannot re-`doom_init` but does not need to. All detailed in the step
notes below and in `CLAUDE.md`'s load-bearing-quirks list.

## Step 0 — Widen the net, then freeze it

The tic hash watches the simulation and nothing else. The renderer, the WAD
reader, the HUD, the status bar and the menu are invisible to it — and this plan
rewrites all of them. So the net is widened *before* the first line of engine
code changes.

Two things make that nearly free:

- `D_DoomLoop` calls `D_Display` (`d_main.c:377`), so the headless test **already
  runs the software renderer every tic**. The finished palette-indexed frame is
  sitting in `screens[0]`, unhashed.
- The engine accepts `-config <path>` (`m_misc.c:352`), which is the fix for a
  latent flaw in the suite as it stands: `M_LoadDefaults` reads the developer's
  real `~/.doomrc`, so `screenblocks`, `detailLevel` and `show_messages` leak
  into every test run. Harmless for the simulation — demo input comes from the
  `.lmp`, not the config — and fatal for a frame hash.

What lands:

- `SimProbe`: boot with a `-config` path that does not exist, so the defaults
  stand on every machine, and add `doomSimFrameHash()` — FNV over `screens[0]`
  (the 320x200 indexed frame) and `screen_palette[768]`, so the damage, pickup
  and invulnerability flashes are covered too.
- `DemoReplay.h`: a frame hash every 4th tic, into a second golden
  (`Tests/Goldens/demoN.frames`), reporting the first diverging frame the way
  `reportDivergence` already reports the first diverging tic.
- `Tests/Sim/WadTests.cpp`: lump count, and every lump's name, size and CRC. This
  is what pins `w_wad.c` and `r_data.c` when the zone allocator comes out.
- Whole-table checksums in `PrimitiveTests.cpp` over `finesine`, `finetangent`,
  `tantoangle`, `rndtable`, `states[]` and `mobjinfo[]`. The spot-checks there
  today would not catch a bad conversion of `tables.c` (2,130 lines) or `info.c`
  (4,663 lines) to `constexpr`.

Still uncovered, and honestly so: the menu (nothing in a demo opens one) and
audio. The menu gets its own net in Step 8, before `m_menu.c` is touched.

**Landed.** 35 tests, 2.3 seconds. The three `.hashes` goldens were re-recorded
as a check and came back byte-identical, which is the proof that moving the
config off `~/.doomrc` left the simulation alone. The net was then shown to bite:
adding 1 to the light-level start map in `R_SetViewSize` fails demo1's *frame*
golden at tic 4 while its *simulation* golden passes untouched.

One trap fell out of it, worth knowing before anything else passes an argument to
`doom_init`: **the engine does not copy its argv.** It keeps the pointer, and
`P_SpawnSpecials` asks `M_CheckParm("-avg")` on every level load — long after the
function that booted the engine has returned. The probe's `argv` had been a local
array, and got away with it only because `argc` was 1, so `M_CheckParm`'s loop
never dereferenced it. Adding `-config` made it a segfault. It is static now.

## Step 1 — Retire the single-header packaging

Deleted: `PureDOOM.h`, `tools/gen_single_header.py`, `examples/SDL`,
`examples/Tests`, `test_framebuffer.raw`, the `thirdparty/SDL` submodule (which
existed only for the SDL example), and the `PUREDOOM_BUILD_SDL_EXAMPLE` /
`PUREDOOM_BUILD_LEGACY_TESTS` options. Two options remain.

Three constraints died with the generator, and Step 2 onward may now rely on
their absence: **two files may share a file-scope name** (`am_map.c`'s `plr` had
to become `am_plr` because `hu_stuff.c` had one too); a `.c` may include a system
header (the generator commented every `#include` out, exempting only `DOOM.c`);
and the header include graph need no longer be acyclic. `dstrings.h`'s deliberate
double space — *"leave the extra space there, to throw off regex in PureDOOM.h
creation"* — is gone with it.

The CI workflow was stale in three ways and is rewritten: it ran the generator,
carried a `single_header` matrix axis that had been a no-op since the option's
last reader was disabled by default, and built the eacp app — which needs a GPU
and tracks a local eacp branch. It now builds the engine and the tests, which is
exactly what CI can actually check, and checks the thing that matters: that the
goldens hold on Linux and Windows too, not only on the machine that recorded them.

**Landed.** 35 tests green from a clean configure, goldens untouched, and the app
still builds. What the SDL example knew that nothing else recorded — the audio
pull model and the 140 Hz MIDI tick — is now written down under *What the engine
expects of its host* in `CLAUDE.md`, and `git log -- examples/SDL` still has the
code.

## Step 2 — The language flip: 62 `.c` → 62 `.cpp`, in one commit

Mechanical, and it must be **atomic**:

```c
// doomtype.h:27
#ifdef __cplusplus
typedef bool doom_boolean;                    // 1 byte
#else
typedef enum { false, true } doom_boolean;    // 4 bytes
#endif
```

`doomstat.h` declares dozens of `extern doom_boolean` globals, including
`playeringame[MAXPLAYERS]`. Mix one C++ translation unit into the C build and
they are read at the wrong size and the wrong stride, with **no compile error and
no link error** — the simulation quietly desyncs. There is no safe partial flip,
so there will not be one.

The cost is smaller than it sounds. A syntax-only C++20 pass says **29 of the 62
files already compile clean**, and the rest raise ~230 errors, all mechanical:
145 implicit `void*` → `T*` assignments (`Z_Malloc`, `W_CacheLumpNum`), ~60
enum↔int and `char*`/`const char*` mismatches, 15 `register` (three files), and
one `fixed_t try` in `r_segs.c:421`. `m_menu.c` is the worst, at 37.

Alongside:

- The engine's CMake glob is `*.c *.h` — **a `.cpp` dropped in today is silently
  ignored.** Widen it; give the `-w` generator expression a `CXX_COMPILER_ID`
  branch (it tests only `C_COMPILER_ID`); pin `-ffp-contract=off`. `FixedDiv2`
  goes through `double` (`m_fixed.c:50`) and the demos replay because that IEEE
  operation is identical every time. Making the contract explicit costs nothing,
  and is what eacp's own `eacp_force_optimization` does for the same reason.
- `Tests/SimProbe.c` → `.cpp` and `examples/EACP/EngineAccess.c` → `.cpp`: they
  are the only two C consumers of the engine's internal headers.
  `PrimitiveTests.cpp` loses the `extern "C"` block it wraps them in — a C++
  engine mangles `FixedMul`, so that block breaks at link time.

**Nothing may change.** Goldens byte-identical, app runs, suite green. That is
the proof the flip was mechanical.

### Landed

275 errors, and none of the casts were written by hand. They were generated from
clang's own diagnostics: the target type read out of the error message, wrapped
around the source range the compiler points at, looping until the file was clean.
A cast therefore cannot be to the wrong type — which is the whole reason not to
type two hundred of them yourself. The whole range is parenthesised, `(T) (expr)`,
because a cast binds tighter than the `&` in `x = P_Random() & 1`.

The goldens held: the C++ engine's simulation and its rendered frames are
bit-identical to the C engine's, tic for tic, and the WAD reads back the same
bytes. Which is what the step promised, and the only thing that could have
demonstrated it.

**`doom_boolean` is an `int`, and must stay one.** `doomtype.h` already had a
`typedef bool` waiting behind `#ifdef __cplusplus`, and taking it was the single
worst thing that happened here. Vanilla reads booleans through pointers to other
types — `ST_createWidgets` binds the status bar's ARMS widget with
`(int*) &plyr->weaponowned[i + 1]`, its *own* cast, and `STlib_updateMultIcon`
then reads four bytes back through that `int*`. Against a one-byte `bool` those
are three bytes of the neighbouring struct, the icon index comes out as garbage,
and the status bar draws a null patch on the first tic of the first demo. Making
it a real `bool` is a change to behaviour, not to spelling; it belongs to Step 5
onward, one subsystem at a time, with the demos watching.

Two more of the same shape, both **booleans that were never booleans** — a third
state, `-1`, that an int-sized enum could hold and a `bool` cannot:

- `animdef_t.istexture` (`p_spec.cpp`). The animated-texture table ends with
  `{-1}` and `P_InitPicAnims` walks until it finds it. As a `bool` the terminator
  is `true`, is never recognised, and the loop runs off the end of the array. The
  compiler caught this one, as a narrowing error.
- `spriteframe_t.rotate` (`r_defs.h`). `R_InitSpriteDefs` memsets `sprtemp` to
  `-1` meaning "no lump seen for this frame yet", and `R_InstallSpriteLump` tests
  `== false` and `== true` *separately*, needing a frame that is neither. The
  compiler could not see through the `memset`; the engine simply refused to boot,
  with `Sprite TROO frame I has rotations and a rot=0 lump`.

Both are now `int`, with the sentinel documented. They are the only two: every
other `memset` to `-1` in the engine is over a `short` array.

And one that C hid rather than tolerated: **`info.cpp` declared all 74 action
functions as `void A_Look();`** — which in C means *unspecified* arguments and
links against `A_Look(mobj_t*)`, and in C++ means *none* and does not. They now
carry their real signatures, taken from the definitions.

## Step 3 — The core: leaves first

From here, each step is one reviewable change: rewrite a module in eacp style,
run `clang-format` on it, take it out of the `-w` blanket, extend the tests, keep
the goldens still.

Order, cheapest first — the seven leaves are dependency-free and 443 lines
together: `m_bbox`, `m_swap`, `m_fixed`, `tables`, `m_random`, `m_cheat`,
`m_argv`. What comes out of them:

- `Math/Fixed.h` — `Fixed`, a trivially-copyable `constexpr` wrapper over
  `std::int32_t` whose `*` is `FixedMul` and `/` is `FixedDiv`. **`FixedDiv2`
  keeps its `double`.** Rewriting it in pure integer arithmetic changes the
  rounding and desyncs every demo ever recorded.
- `Math/Angle.h` — the BAM angle, carrying the three quirks that look like bugs
  and are not (see the rules below).
- `Math/Trig.h` — the fine tables as `constexpr std::array`, proven wholesale by
  the checksums from Step 0.
- `Sim/Random.h` — `Random`, holding `rndtable` and both indices. The first piece
  of engine state to move inside an object, and the smallest possible rehearsal
  for Step 5.

`info.c` and `sounds.c` (4,886 lines of pure data) become `constexpr` tables in
the same wave.

New files land in `src/DOOM` alongside the old. **Vanilla names stay until a file
is genuinely rewritten**, then it moves — `p_map.cpp` is still `p_map.cpp` while
it is still vanilla-shaped, so it stays diffable against the 1993 source you will
be reading it against.

The engine may use eacp's containers: `EA::Vector` comes from
`ea_data_structures`, which is header-only, `INTERFACE`-only, C++20 and pulls in
nothing — no eacp, no graphics. But `CPMAddPackage(eacp)` currently runs inside
`examples/EACP/CMakeLists.txt`, *after* `add_subdirectory(src/DOOM)`, so the
target does not exist when the engine is configured. Hoist
`CPMAddPackage(NAME EADataStructures ...)` into the root list ahead of the
engine; CPM dedupes by name, so eacp's own `find_package` reuses it, and the
tests keep linking `doom-engine` alone. (Not needed yet — nothing in the core
wanted a container.)

### Landed

`src/DOOM/Math/` — `Fixed`, `Angle`, `Trig`, `BBox`. `src/DOOM/Sim/` — `Random`.
Those two directories **are** the rewrite: a file moves into one the moment it
stops being 1993 C, and progress is the flat vanilla list getting shorter. New
code is compiled with `-Wall -Wextra -Wpedantic` and clang-formatted from its
first line; vanilla keeps its blanket `-w` and its exemption, per file, until
someone rewrites it.

**The shims are what make this real.** `m_fixed.cpp`, `tables.cpp`, `m_bbox.cpp`
and `m_random.cpp` still export the vanilla API — `FixedMul`, `finesine`,
`P_Random`, `prndindex` — because most of the engine still calls it. But they
*delegate* rather than duplicate, so the new types sit on the critical path of
every demo the suite replays. There is one copy of the arithmetic, one copy of the
tables, one supply of chance; a caller that has been rewritten and one that has
not cannot disagree. That is the only way a new implementation of the simulation's
core gets tested at all, and it is why they are shims and not copies.

Two details that had to be got right for that to hold:

- **`rndindex` and `prndindex` are now references** into `Doom::Random`. The
  simulation probe hashes four bytes of `prndindex` every tic, so the index stays
  an `int` (masked by hand) rather than becoming a `std::uint8_t`. The refactor may
  change how the tests find state; it may never change what they mix.
- **The trig tables are `const`, not `constexpr`,** and store raw words with the
  type in the accessors. Wrapping sixteen thousand literals in `Fixed {...}` would
  be a sixteen-thousand-line diff on data whose entire point is that it did not
  change, and a `constexpr` array in a header included by sixty translation units
  buys nothing at runtime and costs compile time. `// clang-format off` guards the
  data so it stays visibly verbatim.

**`BBox::add` is `else if`, and must stay so.** It is the obvious candidate for an
independent `min`/`max` per axis, and that would be a different function. On a
fresh (inverted) box a single point moves `left` and leaves `right` at its
sentinel — a point cannot be both below the minimum and above the maximum in one
call — and points fed in descending x never write `right` at all. The engine gets
away with it (`P_GroupLines` feeds whole linedefs, `V_MarkRect` a top-left then a
bottom-right), but "gets away with" is not "does not depend on": min/max changes
what `P_GroupLines` computes for a sector's bounding box, which changes what the
renderer and `P_BlockLinesIterator` see. Pinned by `Tests/Sim/MathTests.cpp`, both
in its own right and against `M_AddToBox` directly.

18 new tests (`Tests/Sim/MathTests.cpp`), 53 in total, still about two seconds.
`I_Error` now takes a `const char*`, which it always should have.

**Deferred, with reasons.** `m_swap` is entirely `#ifdef __BIG_ENDIAN__` and
compiles to nothing on every machine this builds on; `m_argv` and `m_cheat` are
better rewritten with the subsystems that use them. `info.cpp` and `sounds.cpp`
stay as they are: `states[]` is half function pointers into the action table, and
that table is precisely what Step 6 replaces with a real `Thinker` — converting it
to `constexpr` now means converting it twice. The whole-table checksums from Step
0 are already watching all of them.

## Step 4 — Ownership: kill the zone allocator

`z_zone.c` is a 12 MB arena malloc'd once (`i_system.c:35`), with no `Z_Shutdown`
and no way to give it back — the direct cause of *the engine cannot be booted
twice*. Its `PU_*` tags serve exactly two purposes:

- `PU_CACHE` — lump caching → a `Wad/WadFile` owning its directory and its
  decoded lumps.
- `PU_LEVEL` — level lifetime → a `Level` whose destructor frees its own geometry.

`PU_STATIC` becomes ordinary members. Then `z_zone.cpp` is deleted.

Nothing observable in the simulation depends on the allocator — the hash mixes
mobj *fields*, never addresses, and `P_SpawnMobj` zeroes its own memory — so the
goldens hold this step honest, and `WadTests` holds the lump reader honest.

**The payoff — and it was mis-stated.** The plan said the payoff was "boot the
engine twice in one process". It is not, and that framing sent the work chasing
the wrong target. `doom_init` still cannot run twice (`Z_Init` would leak the
arena), and it does not need to: a scenario test never re-inits the engine, it
loads another *level*. And loading a level already resets the whole simulation —
`G_InitNew` → `P_SetupLevel` clears the thinkers (`P_InitThinkers`), the random
indices (`M_ClearRandom`), the leftover mobjs and specials (`Z_FreeTags`), and now
the geometry (`Doom::Level` assigns fresh vectors). Once the geometry moved to the
`Level` object, that reset became *clean*, and the engine runs any number of
scenarios per process. `Tests/Sim/ReplayTests.cpp` proves it: a demo replayed a
second time matches itself tic for tic, and a *different* demo loaded over the
first matches its own fresh-boot golden. The zone need not be deleted for this —
that was the surprise.

### Landed so far — the WAD (`PU_CACHE`)

`Doom::WadFile` (`Wad/WadFile.h`) owns the directory, the file handles and the
decoded lump bytes. It replaces the *tangled* half of the zone: `PU_CACHE` blocks
were purgeable, so `W_CacheLumpNum` had to hand `Z_Malloc` a back-pointer
(`&lumpcache[lump]`) for the allocator to null out behind the caller's back, and
every user of a lump then had to remember `Z_ChangeTag`/`Z_Free` when it was done.
All of that is gone — the `lumpcache` array, the tag argument (now ignored), and
~40 `Z_ChangeTag`/`Z_Free` calls across nine files. **A lump pointer is now
permanent.** `AM_unloadPics` and `ST_unloadGraphics` had nothing left to do and
say so.

`w_wad.cpp` is a shim over it; `lumpinfo`/`numlumps` are a *view* onto the
WadFile's own vector (`Doom::Lump` is layout-compatible with `lumpinfo_t`), because
`r_things` still parses sprite names out of the directory and `r_data` still sums
lump sizes.

Three bugs surfaced, and they are the reason this half took the work it did:

- **A deleted `Z_Free` was the body of an unbraced `if`.** Removing
  `Z_Free(maptex2)` from `R_InitTextures` left `if (maptex2)` with no body, which
  then swallowed the `for` loop below it — so on shareware (no TEXTURE2, `maptex2`
  null) `R_GenerateLookup` never ran, the wall column tables were never built, and
  every wall, floor and ceiling rendered as smeared garbage while the sprites and
  status bar (which read lump data directly) stayed perfect. It compiled clean.
  Audit every unbraced body when deleting a statement.

- **Tutti-frutti, and the one golden re-record.** After that fix, 15 pixels across
  ~2,300 frames still differed. The WAD golden passed (all 1,264 lumps read
  byte-identical) and the simulation was bit-identical, so the pixels could not be
  a lump-reading bug. Padding the lump buffers' tails moved *exactly* those pixels:
  proof that DOOM reads a few bytes past a lump's end — the 1993 tutti-frutti
  quirk, undefined by design and allocator-dependent. The old self-contained zone
  made that garbage the same on every machine; a per-lump `std::vector` would read
  heap-adjacent bytes that differ across platforms. So `WadFile::data` gives each
  lump a **64-byte zero tail** (measured: the over-read is under that): the quirk
  still happens, deterministically, reading zero everywhere. The frame goldens were
  re-recorded once against that — 3 lines total, demo1 #1332 and demo2 #168/#957.
  This was settled by building the pre-Step-4 commit in a worktree and diffing the
  actual pixels, not by argument.

The one that looked like a third bug and was not: `R_GetColumn` tests
`if (!texturecomposite[tex])` on a `Z_Malloc`'d array that is never explicitly
zeroed. It is nonetheless safe, because `R_GenerateLookup` writes
`texturecomposite[texnum] = 0` for every texture before any column is drawn — so
the "fix" of `memset`-ing it changed nothing, which is how it was ruled out.

### Landed so far — the level geometry (`PU_LEVEL`, the clean half)

`Doom::Level` (`Sim/Level.h`) owns the nine arrays that a level builds once and
throws away whole: `vertexes`, `segs`, `subsectors`, `sectors`, `nodes`, `lines`,
`sides`, the per-block mobj-chain heads (`blocklinks`), and the flat line-pointer
buffer `P_GroupLines` carves into per-sector slices (vanilla's `linebuffer`). Each
is a `std::vector`; the loaders in `p_setup.cpp` `assign` into them and refresh the
vanilla global to `.data()`. **`assign`, not `resize`** — a shorter second level
must not inherit the first level's tail, and `Z_Malloc` handed back fresh zeroed
memory every load.

The vanilla globals (`vertexes`, `numsegs`, `sectors`, …) stay as views onto the
vectors — the renderer and playsim index them thousands of times and are not being
rewritten yet. `Tests/Sim/LevelTests.cpp` pins the one invariant the demos can't
see: that every global still equals its vector's `data()`/`size()` after a load. A
loader that resized a vector and forgot to refresh its global would leave it
dangling, and the demos might survive that by allocator luck.

The blockmap and reject matrix lumps are *not* here: they are WAD lumps
(`W_CacheLumpNum`), so `WadFile` already owns them permanently — `blocklinks` is
the only blockmap-related allocation that was ever the zone's. The blockmap
*descriptor*, though — the grid's origin, extent and the lump pointers the iterators
read cells from — has since moved onto `Level` as a `Doom::Blockmap` (`Sim/Blockmap.h`),
which also carries the block-index arithmetic (`(world - origin) >> MAPBLOCKSHIFT`,
the bounds check, the flat index) that the playsim used to re-derive at every call
site. `P_LoadBlockMap` fills it and refreshes the vanilla `bmaporgx`/`bmapwidth`/
`blockmap` globals as views onto it — `LevelTests` pins that, the same way it pins
the geometry views — and `P_SetThingPosition`/`P_UnsetThingPosition` (which the
thing-linking scenario test pins) address the grid through it. This is the first
piece of Step 5's globals-into-`Engine` work to actually move: the clipping globals
follow as `p_map`/`p_maputl` are rewritten to reach them through the owner.

What stays on the zone, deliberately: **mobjs and the thinker specials** (doors,
lifts, lights — `PU_LEVEL`/`PU_LEVSPEC`) and all the renderer's `PU_STATIC` data
(textures, colormaps, sprite tables). The mobjs and thinkers have a per-object
lifecycle (`P_RemoveThinker` frees them mid-play), which is the thinker rewrite in
Step 6; the renderer data belongs to Step 7. `Z_FreeTags(PU_LEVEL, …)` at the top
of `P_SetupLevel` therefore stays — it now frees only those, not the geometry.

### The scenario-test unlock, delivered

The reason to care about all this was scenario tests, and they are unblocked now —
not by deleting the zone (the plan's mistaken gate), but by the `Level` object
making the per-level reset clean. `Tests/Sim/ReplayTests.cpp`:

- **`replaysASecondTimeInOneProcess`** — play a demo to the end, queue it again
  without re-initing, play it again: identical tic for tic. If the reset left a
  stale sector, an un-freed thinker or an uncleared random index, it would diverge.
- **`aDifferentLevelLoadsOverThePrevious`** — play demo1, then demo2 in the same
  process; demo2 must match its own fresh-boot golden. This is the only test that
  drives `Level::assign` to a *different* size than the level already in its
  vectors — the grow/shrink reload a single demo never exercises.

### What is left of Step 4, and where it went

Deleting `z_zone.cpp` outright is no longer a step of its own. Its last users are
subsystem-specific and move with their subsystems: the renderer's `PU_STATIC` in
Step 7, the mobjs and thinker specials in Step 6. The zone is deleted when its last
caller does, and there is no longer a reason to force it earlier — the payoff it
was gating is already in hand.

One caveat carried forward: changing the *allocator* under a renderer allocation
can re-trigger tutti-frutti (composite pixel blocks get over-read the same way
lumps did). So when the renderer's `PU_STATIC` moves in Step 7, the pixel blocks
want the same zero-tail treatment `WadFile` gave the lumps — the metadata arrays
(pointers, sizes, offsets) are safe, the pixel data is not.

## Step 5 — The `Engine` object

Subsystem by subsystem, globals move into a state struct owned by one `Engine`.
During the transition a single `Engine*` keeps the old call sites compiling; as
each file is rewritten it takes `Engine&` explicitly and the alias goes.
`doomstat.h` (73 externs), `r_state.h` (44) and `p_local.h` (27) are the three
headers that empty out.

The three subsystems already extracted — `Doom::Random`, `Doom::WadFile`,
`Doom::Level` — are the `Engine`'s first members; the free `randomness()`, `wad()`
and `level()` singletons become accessors into the one instance. With the whole of
the engine's state eventually under one object, the engine can be *constructed*
rather than only booted: a fresh `Engine` is a clean world, which is what a test
that re-inits (rather than reloads a level) would need. That is a stronger property
than the scenario-test unlock already in hand, and it is what retires the loose
globals for good.

### Landed — the composition root

`Doom::Engine` (`Engine/Engine.h`) holds `Random`, `WadFile` and `Level`. The three
free accessors moved into `Engine/Engine.cpp` and now return `engine().random`,
`engine().wad`, `engine().level` — one owner where there were three independent
singletons. `Sim/Level.cpp` held nothing but `level()` and is gone.
`Tests/Sim/EngineTests.cpp` pins the wiring (`&randomness() == &engine().random`,
and so for the others) and that a second `Engine` is genuinely independent — no
hidden shared state, the property a test-owned world will rest on.

`engine()` is a function-local static, deliberately: `m_random.cpp` binds
`int& rndindex = randomness().menuIndex` at static-init time, which reaches through
it before `main()`, and a function-local static is constructed on that first call
regardless of translation-unit order.

**What did not move, and why.** The ~684 scalar globals (`doomstat.h`'s 73,
`r_state.h`'s 44, `p_local.h`'s 27) stay put. Each cluster is owned by a subsystem
that Steps 6–8 rewrite, and it moves *into the `Engine` when that subsystem is
rewritten to take an `Engine&`* — not before. Aliasing them in now (`int& gametic =
engine().gametic`) would be golden-neutral but would scatter reference-globals
across the transition for no gain until the call sites change. The `Engine` grows
with the rewrite; it is not filled speculatively ahead of it.

The end shape, once the clusters have moved in:

```cpp
auto doom = Engine {config, wad};
doom.runTic();
```

## Step 6 — The playsim

Covered exactly by the demos, and now with scenario tests for locality. One
change per module: `p_maputl` → `p_map` (`P_TryMove`, `P_CheckPosition`, the
blockmap walk) → `p_mobj` → `p_inter` → `p_enemy` → `p_pspr` → `p_sight` →
`p_user` → the specials (`p_spec`, `p_doors`, `p_floor`, `p_plats`, `p_lights`,
`p_ceilng`, `p_switch`, `p_telept`) → `p_tick`.

`thinker_t`'s function-pointer union becomes a real `Thinker` with a virtual
`tick()`. One rule attaches to that, because `SimProbe` finds mobjs by comparing
`thinker->function.acp1 == P_MobjThinker`: **the probe may change how it finds
things; it may never change what it mixes, or in what order.** Otherwise a golden
moves for a reason that is not a behaviour change, and the net has been cut
rather than widened.

### Landed so far — `Vec2` and the map-geometry core

The playsim's core value type is in: `Doom::Vec2` (`Math/Vec2.h`), two `Fixed`
making a point or a vector in the map plane, trivially copyable and
layout-compatible with the pair of `fixed_t`s the vanilla structs still store — so
a rewritten function takes a `Vec2` while the `mobj_t`/`line_t` it came from stays
1993 C.

The geometric core of collision, sight and shooting moved onto it:
`Sim/MapGeometry.h` has `pointOnLineSide`, `pointOnDivlineSide` and
`interceptVector` (plus a `DivLine` value), and `p_maputl.cpp`'s `P_PointOnLineSide`
/ `P_PointOnDivlineSide` / `P_InterceptVector` are three-line shims over them. The
fixed-point quirks are preserved verbatim and commented as load-bearing — the
`>> FRACBITS` on one factor in the line formula, the `>> 8` on both plus the
sign-bit fast path in the divline formula. **They are different formulae and the
header says so**; a merge would desync the demos.

`Tests/Sim/GeometryTests.cpp` gives the locality the demos can't: side answers on
by-hand-clear geometry (a point south of an eastward line is on the front), a sweep
proving the line and divline formulae agree wherever a point sits clearly off the
line, and the intercept of two crossing lines landing a quarter of the way along.
The demos remain the proof the shims read the right fields — a bit-identical replay
could not survive reading `dx` where `dy` was meant.

Three more pure helpers from `p_maputl` joined the header: `approxDistance` (DOOM's
`|larger| + |smaller|/2` distance estimate), `lineOpening` (the vertical window a
two-sided line leaves — lower ceiling down to higher floor, plus the lower floor,
which `PIT_CheckLine` narrows a mover against) and `boxOnLineSide` (which side of a
line a whole bounding box is on, or −1 straddling — the cheap reject `PIT_CheckLine`
runs before the exact test). `p_maputl.cpp`'s `P_AproxDistance`, `P_LineOpening` and
`P_BoxOnLineSide` are shims over them; the single-sided-line early-out
(`openrange = 0`) stays in the `P_LineOpening` shim, being about the linedef's
structure rather than the two sectors' heights, and `boxOnLineSide` takes the
linedef's precomputed `slopetype` as an int rather than recomputing it.
`GeometryTests.cpp` pins the estimate as an *over*estimate of a real diagonal — a
"fix" to a true hypotenuse would desync every demo, the aim and the blockmap search
radius having been tuned against these numbers — the opening window, and the box
side for each slopetype (front, back, straddle). The goldens held bit-identical
across every swap, which, with `P_AproxDistance` on the critical path of the
renderer and the sound code as well as the playsim, is the real proof the extraction
changed nothing. With this the pure geometry of `p_maputl` is fully out; what stays
in the file is the stateful part (linking, iterators, intercepts, `P_PathTraverse`).

### Landed so far — the scenario-test harness

The enabler for the rest of the playsim, and the Step-0 move again — widen the net
before touching the code, with a new driver. `Tests/SimProbe` grew a scenario API:
`doomSimLoadLevel(episode, map, skill)` loads a level synchronously through
`G_InitNew` with no demo (forcing single-player so the map's player-1 start spawns
a mobj); `doomSimSpawnMobj` places a thing and hands back a plain-`int` handle into
a probe-side registry (invalidated whenever a level load frees the `PU_LEVEL`
mobjs, and re-seeded with the fresh player as handle 0); `doomSimCheckPosition` /
`doomSimTryMove` drive the two clipping functions on a handle and return the
answer, with `doomSimMobjX/Y/Z` and a flags get/set for the before/after state.
`SimProbe.h` stays free of DOOM types — the tests see integers and named-constant
accessors (`doomSimTypeBarrel`, `doomSimOnFloorZ`, `doomSimFlagNoClip`). It rests
on the multi-scenario capability Step 4 proved: a level load resets the world
cleanly, so a scenario runs on a fresh world in the same process.

`Tests/Sim/ScenarioTests.cpp` pins four playsim facts the demos only prove in
aggregate, **three of them without any reference to the map's geometry** — they
follow from the collision and linking code, not from where E1M1's walls happen to
be:

- a solid barrel dropped exactly on the player's own start flips `P_CheckPosition`
  from legal to illegal, and `MF_NOCLIP` flips it back — isolating thing collision
  and the NOCLIP early-out from the walls entirely;
- a blocked `P_TryMove` (onto a barrel 40 units east, past the 26-unit combined
  radius) answers false and leaves the mobj exactly where it was — the invariant a
  rewrite is likeliest to break subtly;
- a legal `P_TryMove`, into a spot *discovered* clear by asking `P_CheckPosition`
  rather than assuming one, commits the new position, and the round trip back
  proves it was a real move and not a no-op;
- the blockmap linking under all of it, read directly: two barrels share the
  player's start cell, and the count `P_BlockThingsIterator` finds there moves by
  exactly one as the second is linked (`P_SetThingPosition`), unlinked
  (`P_UnsetThingPosition`) and relinked — the locality that says *which* of the
  three broke, where the demos only say the world moved.

Each was shown to bite by mutation: making `PIT_CheckThing` never block fails the
first two; removing the position commit in `P_TryMove` fails the third's commit
assertion while its "the move succeeds" assertion still passes — so it pins the
commit specifically; skipping the blockmap link in `P_SetThingPosition` fails the
fourth, and a no-op `P_UnsetThingPosition` breaks it too (the relink then splices
the barrel into the cell twice and the iterator loops — the corruption a real
unlink prevents). (A cautionary detour worth recording: the first attempt to prove
the commit case mutated the *earlier* occurrence of `thing->x = x;`, which is in
`P_TeleportMove`, not `P_TryMove` — the net only looks like it bites if you make
sure the mutation lands where you think. Prove it, don't trust it.) Nothing here
touches a golden or the append-only probe hash; it is purely additive.

Next in the module, now netted for locality: the wholesale rewrite of the stateful
half of `p_maputl` into modern C++ (the blockmap iterators — function-pointer
callbacks → lambdas — the thing-position linking, the intercept routines and
`P_PathTraverse`), which flips the whole file out of the `-w` blanket and under
`clang-format`, and then `p_map` (`P_TryMove`, `P_CheckPosition`) itself, which the
scenario tests pin directly.

## Step 7 — The renderer

Pinned by the frame goldens from Step 0. `r_data` → `r_bsp` → `r_segs` →
`r_plane` → `r_things` → `r_draw` → `r_main`.

The software renderer stays. The GPU path in `examples/EACP` is the real renderer
now, but the software frame is still the fallback outside a level, the source of
the composited status bar, and what the overlay capture draws into.
`EngineAccess` reaches directly into `r_state.h`, `r_bsp.h` and `r_data.h`, so it
moves with each renderer change — it is ours, and the intent is that it gradually
stops being a reach-around and becomes the engine's actual interface.

## Step 8 — UI, game loop, host boundary

`am_map`, `st_stuff`, `hu_stuff`, `wi_stuff`, `f_finale`, `f_wipe`, `m_menu`,
`g_game`, `d_main`.

`m_menu.c` is the one part of the engine with **no test coverage at all** —
nothing in a demo opens a menu. Before touching it: drive synthetic key events
through `doom_key_down`, hash the frames, record a golden. Same technique as
Step 0, different driver.

Last, the host boundary: the 13 function pointers in `doom_config.h` become a
`Host` interface, and audio (gap-log item 1) is wired through it rather than
around it.

## The rules

1. **A refactor never re-records a golden.** `record-goldens` is for intended
   behaviour changes, and this project has none. Re-recording to turn a red suite
   green defeats the entire apparatus.
2. **The probe's hash is append-only.** Change how it finds state, never what it
   mixes or in what order.
3. A file leaves the `-w` blanket and comes under `clang-format` the moment it is
   rewritten, and not before.
4. These are **not bugs**, and three of them already have tests saying so:
   `FixedDiv2` going through `double`; the trig tables sampled at bucket centres
   (`finesine[0] == 25`); `SlopeDiv` answering `SLOPERANGE` for any denominator
   under 512; `R_PointToAngle2` landing one unit below due north (`ANG90 - 1`).
5. Every change runs `ctest` before and after, and `git diff --stat
   Tests/Goldens/` comes back empty. Anything the demos cannot see — renderer,
   automap, HUD, melt, menu — also gets the app run and looked at.
