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

2. **The ~55 dead-in-both-eras macros.** Defined and unused in this lineage *and*
   at 110ddbe. Deleting them is a judgement call reserved for a human. Do not
   convert them: a sweep once converted ten and every one came out as
   `[[maybe_unused]] constexpr`, an attribute whose only job is to silence the
   diagnostic saying the thing is unused — which is the signal it should not have
   been converted. All ten were reverted.

3. **Two unmeasured toolchains, and then `-Werror`.** The engine builds with zero
   warnings on Apple Clang (`Debug` and `Release`), real GCC 16 (`Release`), and
   Windows arm64 under both clang-cl and MSVC. Ubuntu's gcc/clang and MSVC on
   `/W4` are the remainder. CI prints a per-configuration warning count and fails
   on nothing, so those measure themselves on the next push. `-Werror` waits on
   them.

   Raising the bar further — `-Wconversion`, `-Wshadow` — is a real decision that
   should be taken for all compilers at once rather than arrived at by accident on
   one. MSVC's `/W4` has six warnings switched off for exactly this reason, each
   corresponding to a GCC/Clang flag this project has deliberately not enabled;
   the reasoning is written out at the flags in `src/DOOM/CMakeLists.txt`.

4. **Two dead enums.** `StatusBarMode` and `ChatState` are declared and never
   used. Deletion is a human's call, per item 2's rule.

5. **A frame golden for the intermission.** `Tests/Sim/IntermissionTests.cpp`
   asserts the state machine; it has no recorded frames yet. The natural
   follow-up now that the sanitizers run clean through the transition.

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
