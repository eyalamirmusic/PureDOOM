# The C++ refactor

> ## The goal
>
> **Every source file in `src/DOOM` is clean, modern, RAII C++ in eacp's style —
> and the simulation behaves *exactly* as it always has.**
>
> Not "it compiles as C++". Every file reads as C++ someone *wrote*: RAII ownership
> end to end, eacp's containers and types, real classes with methods, no loose
> globals, no shim or alias layer, no 1993 C idiom — save the handful of quirks that
> are load-bearing and pinned by a test.

**That goal is met.** `src/DOOM` began as 43,889 lines of 1993 C across 62 `.c`
files — ~684 global data symbols, ~1,534 file-scope statics, a 12 MB zone arena
never handed back, and warnings disabled wholesale with `-w`. None of that
remains. What is left of the whole effort is a short, named, mostly *deliberate*
tail, listed under **What is left** below.

This document is now the record of that: the end state, what remains, and the
lessons worth carrying into future work. It is no longer a plan.

## The rules the whole thing rests on

These outlive the refactor. Breaking one silently defeats the apparatus.

1. **A refactor never re-records a *simulation* golden** (`*.hashes`). Those pin
   the world, and the world does not change. A red `*.hashes` suite says the
   refactor was wrong, not that the golden is stale.

   The *frame* goldens (`*.frames`) hold to the same bar with one measured
   exception, which has happened exactly once: DOOM's renderer reads a few bytes
   past a lump's end (tutti-frutti), an undefined value that depended on the
   allocator, so when Step 4 replaced it, 15 pixels across ~2,300 frames moved and
   those frames were re-recorded. The test for doing that again: the pixels that
   moved must be provably not part of any lump.

2. **The simulation probe's hash is append-only.** `Tests/SimProbe` may change
   *how* it finds state; it may never change *what* it mixes, or in what order.

3. **Some things that look like bugs are load-bearing and must survive.**
   `FixedDiv2` goes through `double`; the trig tables are sampled at bucket
   centres; `slopeDiv` gives up under 512; `pointToAngle2` lands one unit below
   north; `BBox::add` is `else if`, not an independent min and max.
   `Tests/Sim/PrimitiveTests.cpp` and `Tests/Sim/MathTests.cpp` pin these on
   purpose — a refactor will want to "fix" all of them, and each would desync
   every demo ever recorded.

4. **Preserved 1993 defects are behaviour, not cleanups.** Each is documented at
   its own site and recorded under **Preserved defects** below. Fixing one is a
   deliberate behaviour change for a human to make, not a tidy-up.

## Progress

Every step is done. The detail that used to live in this table is now in
`CLAUDE.md`, which describes the code as it *is* rather than how it got there.

| Step | What | State |
|---|---|---|
| 0 | Widen the net to the renderer, the WAD and the tables | done |
| 1 | Retire the single-header packaging | done |
| 2 | The language flip: 62 `.c` → 62 `.cpp`, atomically | done |
| 3 | The core: leaves first (`Fixed`, `Angle`, `Trig`, `Random`) | done |
| 4 | Ownership: kill the zone allocator | done |
| 5 | The `Engine` object: globals become members | done |
| 6 | The playsim | done |
| 7 | The renderer | done |
| 8 | UI, game loop, host boundary; `thinker_t`→`Thinker` | done, bar audio |
| 9 | Modern C++ / RAII across the board | done, bar audio |
| 10 | Strings, and the `extern "C"` API removed | done |
| 11 | Scoped enums: every `enum` becomes `enum class` | done |

Step 11 is the most recent and is not described in the older steps' terms: all
~1,600 enumerators across 50 enums are scoped and PascalCase, the flag enums go
through explicit helpers in `doomtype.h` (`flagBits`/`hasFlag`/`withFlags`/
`withoutFlags`/`toggledFlags`) rather than operator overloads, index conversions
are spelled `toIndex(...)`, and every switch over a small enum lists its cases so
`-Wswitch` — not a `default:` — is what notices a new enumerator. `CLAUDE.md`
has the working detail.

## What is left

1. **Audio.** Blocked outside this repository, on an eacp audio stream. The
   engine side is built: sound is a pull model at `DOOM_SAMPLERATE`, music a
   140 Hz push. See `CLAUDE.md`'s "What the engine expects of its host".

2. **Two unmeasured toolchains, and then `-Werror`.** The engine builds with zero
   warnings on Apple Clang (`Debug` and `Release`), real GCC 16 (`Release`), and
   Windows arm64 under both clang-cl and MSVC. Ubuntu's gcc/clang and MSVC on
   `/W4` are the remainder, and `-Werror` waits on them.

   **Nothing measures them today, and the workflow will not start on its own.**
   `.github/workflows/tests.yml` triggers on pushes to `master` and PRs into it;
   the refactor branch is a long way ahead of `master`, so its five rows — four
   distinct toolchains — have never seen this code. The Report-warning-count step
   is built and ready and has never run against it. Taking the measurement is a
   deliberate act: add the branch to the workflow's triggers and push, or build
   Ubuntu locally in a container. Until one of those happens, "CI will tell us" is
   a plan, not a state.

   Raising the bar further — `-Wconversion`, `-Wshadow` — is a real decision that
   should be taken for all compilers at once rather than arrived at by accident on
   one. MSVC's `/W4` has six warnings switched off for exactly this reason, each
   corresponding to a GCC/Clang flag this project has deliberately not enabled;
   the reasoning is written out at the flags in `src/DOOM/CMakeLists.txt`.

That is the whole list. Three items closed recently, kept here because each says
something the code alone does not:

- **The intermission's frame golden.** `Tests/Goldens/intermission.frames` pins 401
  tics of the real E1M1 → scoreboard → E1M2 transition, and every screen a demo
  never reaches now has one. Compiler-independent like the rest, reproducible across
  runs, and sharp: moving `SP_STATSX` by one pixel fails it and nothing else in the
  99-test suite. One script drives the transition and two tests read it — the state
  machine and the frames — so a failure says which broke.

- **The dead-in-both-eras macros**, deleted — 70 of them, each verified
  unreferenced in this lineage *and* in the 127-file tree at 110ddbe before it went.
  Deleting rather than converting was always the right half of that choice; what the
  item could not say in advance is that the sweep would find two live cross-boundary
  checks resting on dead code (`static_assert`s now, in `UI/StatusBar.cpp` and
  `UI/Hud.cpp`), and that the honest count was 70 rather than the ~55 estimated —
  see the two sweep traps below for why the first pass under-counted.

  **Four macros that look identically dead stay, for two different reasons.**
  `_CRT_SECURE_NO_WARNINGS`, `_CRT_NONSTDC_NO_DEPRECATE` and
  `_WINSOCK_DEPRECATED_NO_WARNINGS` are read by MSVC's own headers, so "nothing in
  this repository mentions it" is not the test for them. `DOOM_LINUX` is one of
  three names for a single question, and dropping the fallback would leave an
  `#else` that identifies no platform at all.

- **The two dead enums.** `StatusBarMode` and `ChatState` are gone, with the 1993
  debris that surrounded them in `UI/StatusBarTypes.h`.

## Preserved defects

Found during the refactor, preserved deliberately, documented at their sites.
Each is in the 1993-lineage source too, and each is behaviour the goldens record.

- **`Host/Sound.cpp`'s MUS delay decode** has an operator-precedence bug that
  truncates any multi-byte delay.
- **`UI/Intermission.cpp`'s `drawAnimatedBack`** tests the enum *constant*
  `commercial` rather than comparing against it, so the intermission's animated
  background has never drawn in this lineage.
- **`UI/Hud.cpp` (twice) and `UI/Intermission.cpp` test the enumerator `french`**
  — value 1, always true — instead of `gameVersion().language`, so French
  character translation and the French shift table are always applied. This one
  only surfaced when `enum class` removed the implicit conversion to bool.
- **Quicksave and quickload's third builder call is `strcpy` where `concat` was
  meant**, so the prompt never shows the savegame name.
- **`UI/Automap.cpp`'s `st_notify`** reproduces vanilla's field-shifted
  `{ 0, ev_keyup, AM_MSGEXITED }`, where `ev_keyup` lands in the `int data1` slot
  and the responder's test therefore never fires.
- **`Enemy.cpp`'s `fatAttack1/2/3`** pass `actor.target` to `spawnMissile` with no
  null guard where every sibling attack has one.
- **`Specials.cpp`'s `doDonut`** dereferences `getNextSector`'s result unchecked,
  where every other call site guards it.
- **`UI/Hud.cpp`'s chat responder** indexes the 128-entry `shiftxform` with an
  `unsigned char`, so shift plus a key code above 127 reads past the table. It is
  also why `shiftxform` stays a raw pointer: as a `std::span` the same read
  becomes an MSVC debug-STL assertion, which is a worse failure mode than the
  silent read.
- **`UI/Finale.cpp`'s three glyph loops bound `c` with `> HU_FONTSIZE` where
  `UI/Menu.cpp` uses `>=`**, so a character mapping to exactly `HU_FONTSIZE`
  indexes one past the 63-entry font. Both spellings are vanilla's own — `>` in
  `f_finale.c` and `m_misc.c`, `>=` in `m_menu.c` — and both are reproduced
  faithfully. It takes a backtick to reach — `toUpper` upper-cases `a`..`z` and
  leaves everything else alone, and a backtick sits at `HU_FONTSTART + 63` — and
  nothing that reaches those loops contains one: the finale texts are fixed
  strings, and the fourth site, `Game/Config.cpp`'s `drawText`, has no callers
  here or at 110ddbe. Worth knowing anyway, because
  unlike `shiftxform` this one indexes an `Array` rather than a raw pointer, so if
  a caller ever does pass user text through it the Windows `Debug` failure is an
  STL assertion rather than vanilla's silent garbage glyph.

## Traps worth carrying

Every one of these cost real time here. They are ordered roughly by how
transferable they are.

### Verification and sweeps

- **Verify a completeness claim against the *category*, not the spelling you
  searched for.** Strand (a) was declared finished twice while a whole syntactic
  tier of aliases still stood, because each sweep searched for the spelling it had
  just fixed. Enumerate the category first, then count what is left in it.

- **After a mechanical rewrite that turns an expression into a comparison, grep
  that the comparison never appears anywhere but a conditional.** The enum sweep
  rewrote `if (x->meleestate)` into a `!= StateNum::Null` test and also caught
  *value* contexts, producing `static_cast<StateNum>(x->meleestate != StateNum::Null)`
  — a bool cast to a state. It compiled with zero warnings and hung the playsim in
  an infinite `setMobjState` loop. An explicit cast is exactly what silences the
  type system, so the cast you added for safety is the one that hides this.

- **A dead-code set is a fixed point, not one pass, and a comment counts as a
  mention but not as a use.** The macro sweep made both mistakes in one afternoon.
  `ST_MAPWIDTH` looked live because `ST_MAPTITLEX`'s body named it, and
  `ST_MAPTITLEX` was itself dead — a second round found it. Then four more
  (`CENTERY`, `HU_TITLEHEIGHT`, `HU_INPUTWIDTH`, `HU_INPUTHEIGHT`) survived the
  whole sweep because each had a comment beside it *explaining that it was dead*,
  and the search counted that sentence as a use. Strip comments, then iterate to a
  fixed point.

- **Deleting dead code can delete a live check with it.** `ST_NUMFACES` was dead,
  but `StatusBarGraphics.h` claimed a compile-time check against it — a claim that
  had been false since the alias sweep retired the reference-to-array bindings that
  once provided it. `HU_FONTSIZE` against `HudFont::fontSize` was the same, and
  `MenuState.h` had already recorded a third instance of exactly this shape without
  anyone noticing it was a *category*. Before deleting something a comment points
  at, check whether the comment describes a guarantee — and if it does, check the
  guarantee still exists. A `static_assert` cannot be retired by accident; a
  binding that happens to typecheck can.

- **`git grep -E` and BSD `sed` accept `\b` and never match it.** A sweep run
  through either silently does nothing and reports success. Use `perl` or GNU
  tools when word boundaries matter.

- **A reader-count heuristic lies after a hoist.** Once a function hoists
  `auto& x = cluster();`, counting readers of the old name undercounts.

- **`extern` declarations are not only in headers.** Several sat in `.cpp` files
  and were missed by a headers-only survey.

- **A test can assert the very thing being removed.** `Random/vanillaNamesAliasTheObject`
  pinned the alias layer that was being retired.

- **"Its signature can't change" is a claim to check, not assume.** Several
  RAII conversions were blocked on a supposed constraint that did not exist —
  `readFile`'s `byte**` out-parameter had exactly one caller.

### Types and ownership

- **When a raw arithmetic type becomes a strong one, the sites needing an audit
  are not only the ones that fail to compile** — they are every site where a
  *literal of another type* met the old one. Those compile, run, and warn in a way
  that is easy to dismiss. This is how `thintriangle_guy`, the refactor's one real
  behaviour bug, survived: `-.5 * FRACUNIT` started converting `-.5` to `int` 0,
  collapsing the automap's thing-shape to a point. The compiler printed the exact
  defect in plain language in *every build* for months, inside an 81-warning
  haystack, and no golden could see it because the shape draws only under IDDT.

- **RAII means owning the release, not owning the layout.** `LevelPool`'s blocks
  are variable-sized and hold polymorphic `Thinker`s whose addresses are
  serialised and threaded on a list, so they can never be relocated and a
  container is not available at any price. It got a destructor instead. Recognise
  this shape rather than treating it as a failure to convert.

- **A pointer that owns *sometimes* stays a raw view, and gains a separate owning
  member.** `DemoState::demobuffer` is allocated when recording but points at a
  WAD lump when playing back; making it an owner would free WAD memory on every
  playback. `SaveGameState::buffer` is the same shape.

- **An owner-of-owners invalidates published views on growth.** Where an inner
  buffer's `.data()` outlives the call, reserve the outer container up front.

- **Retiring an alias is a good way to find state nothing reads.** Eight members
  turned out to have no readers at all once the vanilla name was gone.

- **Names collide across clusters.** `UI/Menu.cpp`'s `mousex`/`mousey` are
  references into a different cluster than the identically-named ones elsewhere.

### Tests and gates

- **No golden can see a leak.** The goldens hash the world and the picture;
  leaked memory changes neither until the process runs out.
  `Tests/Sim/OwnershipTests.cpp` installs a counting `malloc`/`free` pair and
  asserts live blocks after `resetEngine()` return to the post-boot figure. That
  is what found the level pool's missing destructor.

- **Before refactoring a file, check what actually covers it — by running the
  code, not by reading a list.** The cheat matcher was live engine code with *no
  gate over it at all*, and went unnoticed because the coverage notes were
  organised around screens and `checkCheat` is not a screen. A category nobody has
  thought to name cannot show up as missing.

- **Booting tests go in `SimTests`.** Only it links `Tests/TestMain.cpp`, which
  points `DOOMWADDIR` at the repository root. A booting test in `PrimitiveTests`
  passes when you run the binary by hand from the repo root and fails under ctest,
  which runs it from elsewhere — the opposite of the usual failure, and easy to
  misread as flakiness.

- **The app-link gate is invisible to the test configuration.** `examples/EACP`
  is not built by the fast `-DPUREDOOM_BUILD_EACP_EXAMPLE=OFF` loop, and
  `EngineAccess.cpp` includes engine headers directly, so an engine change can
  break the app with every test green. Keep a second build directory for it.

- **Flat CPU is the tell for a hang; a busy loop is a different problem.** When a
  test stops making progress, `sample <pid>` names the function in one command.
  Reasoning about it does not.

### Build and toolchains

- **A warning suppression is scoped to one compiler and spelled in its dialect,
  so it fails silently in the direction that looks clean.** Two generated tables
  carried `#pragma GCC diagnostic ignored "-Wwritable-strings"` — Clang's name for
  the flag. Clang went quiet; GCC did not recognise the option, warned about
  *that*, and then emitted 314 warnings Clang had never shown. Prefer fixing the
  type over naming the flag; if you must suppress, spell it per compiler and scope
  it as tightly as possible.

- **The compile flags are chosen by the *driver*, not the compiler's name.**
  clang-cl reports `CMAKE_CXX_COMPILER_ID` as `Clang` with
  `CMAKE_CXX_COMPILER_FRONTEND_VARIANT` `MSVC`. Selecting on the ID alone fed
  `-Wall` to an MSVC-style driver, where it means `/Wall`, which clang implements
  as `-Weverything` — ~44,000 warnings, and it silently dropped `-ffp-contract=off`,
  so the determinism the goldens rest on was not in force on the one Windows
  toolchain most likely to contract.

- **Nothing here is a C++20 module, but CMake scans for them anyway**, which puts
  `-fmodules-ts` on GCC's command line, makes `__has_feature(modules)` true, and
  sends Apple's `<cstring>` down a Clang-only path with `rsize_t` undeclared. The
  root `CMakeLists.txt` sets `CMAKE_CXX_SCAN_FOR_MODULES OFF` for this reason.

- **`Debug` and `Release` fail differently, and `Debug` fails worse.** On Windows
  a corrupted heap or failed assertion is reported through a *modal dialog*, which
  under ctest reaches no desktop: the binary stops with no output, no exit code and
  no CPU. `Tests/TestMain.cpp` routes `_CRT_WARN`/`_CRT_ERROR`/`_CRT_ASSERT` to
  stderr so the next one says what it is.

- **Run AddressSanitizer when a golden moves for no reason.** It found the
  `doomu.wad` one-byte heap overflow immediately, with a stack, after an afternoon
  of reasoning had not.

- **`_WIN32` is the macro to test, never bare `WIN32`.** The latter is not a
  compiler macro at all; it arrives from the Windows SDK or from a build system
  that adds `-DWIN32`. Every Windows build was one build system away from silently
  taking the `DOOM_LINUX` branch.
