# The C++ refactor

`src/DOOM` is 43,889 lines of 1993 C across 62 `.c` files: ~684 global data
symbols, ~1,534 file-scope statics, a 12 MB zone arena that is never handed
back, and warnings disabled wholesale (`-w`). This fork owns that code, and this
document is the plan for rewriting it as modern C++ in eacp's style — playsim,
renderer, WAD, UI and game loop alike — **without changing what the simulation
does.**

Three decisions frame the work:

- **The whole engine goes.** No permanent C/C++ seam.
- **The globals go with it.** The end state is an `Engine` object that can be
  constructed twice in one process. That is not cosmetic: it is what makes
  scenario tests (*load MAP01, place an imp, run 20 tics, assert*) writable at
  all, and today they are not.
- **`PureDOOM.h`, `tools/gen_single_header.py`, `examples/SDL` and
  `examples/Tests` are retired.** Nothing builds against them, and they are the
  only reason two files in the engine may not share a file-scope name.

The invariant that makes any of this safe is already in place. DOOM's simulation
is exactly reproducible, and `Tests/` replays 11,410 tics of recorded demo input
against a per-tic hash of the world. **The goldens under `Tests/Goldens/` come
out byte-identical after every step below.** A refactor never re-records. If a
golden moves, the refactor was wrong — that is the whole apparatus, and
`record-goldens` exists for intended behaviour changes, of which this project
has none.

## Progress

| Step | What | State |
|---|---|---|
| 0 | Widen the net to the renderer, the WAD and the tables | **done** |
| 1 | Retire the single-header packaging | **done** |
| 2 | The language flip: 62 `.c` → 62 `.cpp`, atomically | **done** |
| 3 | The core: leaves first (`Fixed`, `Angle`, `Trig`, `Random`) | **done** |
| 4 | Ownership: kill the zone allocator | next |
| 5 | The `Engine` object: globals become members | |
| 6 | The playsim | |
| 7 | The renderer | |
| 8 | UI, game loop, host boundary | |

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
  decoded lumps, handing out `EA::BufferView<const std::byte>`.
- `PU_LEVEL` — level lifetime → a `Level` whose destructor frees its own geometry.

`PU_STATIC` becomes ordinary members. Then `z_zone.c` is deleted.

Nothing observable in the simulation depends on the allocator — the hash mixes
mobj *fields*, never addresses, and `P_SpawnMobj` zeroes its own memory — so the
goldens hold this step honest, and `WadTests` holds the lump reader honest.

**The payoff is a test**: boot the engine twice in one process and get the same
demo hashes both times. Once that is green, NanoTest's one-case-per-process rule
is lifted and scenario tests become cheap.

## Step 5 — The `Engine` object

Subsystem by subsystem, globals move into a state struct owned by one `Engine`.
During the transition a single `Engine*` keeps the old call sites compiling; as
each file is rewritten it takes `Engine&` explicitly and the alias goes.
`doomstat.h` (73 externs), `r_state.h` (44) and `p_local.h` (27) are the three
headers that empty out.

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
