# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with
code in this repository.

## Project Overview

This began as a fork of [Daivuk/PureDOOM](https://github.com/Daivuk/PureDOOM) —
the single-header DOOM source port — to port DOOM's platform layer to
[eacp](https://github.com/eyalamirmusic/eacp).

It no longer tracks upstream. **This repository owns `src/DOOM`** and modifies it
freely. Divergence from upstream PureDOOM is deliberate and permanent, and nothing
here needs to be upstreamable.

Three goals:

1. Run DOOM on eacp's application, GPU and input stack.
2. Exercise eacp as a game platform layer and surface what it is missing. Every
   gap found while porting is recorded in the gap log below.
3. Keep the engine itself modern C++, behind the safety net described under
   **Testing**. Read that section before touching anything in `src/DOOM`: DOOM's
   simulation is exactly reproducible, and the tests exist to keep it that way.

Audio arrived separately, through
[MakeASound](https://github.com/eyalamirmusic/MakeASound) rather than eacp, and has
its own gap log at the end of this file.

**The C++ refactor is finished.** `REFACTOR.md` records the end state, the short
list of what remains (audio, and two unmeasured toolchains before `-Werror`), the
preserved 1993 defects, and the traps the work turned up. Read it before a large
change; read this file for how the code works today.

## The four rules that matter most

Breaking one silently defeats the whole apparatus.

1. **A refactor never re-records a *simulation* golden** (`*.hashes`). Those pin
   the world, and the world does not change. A red `*.hashes` suite is telling you
   the refactor was wrong, not that the golden is stale. The *frame* goldens
   (`*.frames`) hold to the same bar, with one measured exception documented in
   `REFACTOR.md`. `record-goldens` exists for intended behaviour changes.
2. **The simulation probe's hash is append-only.** `Tests/SimProbe` may change
   *how* it finds state; it may never change *what* it mixes, or in what order.
3. **Nothing is exempt from `-Wall -Wextra -Wpedantic` or from clang-format.**
   New code is written to the strict flags without asking. Zero warnings is a
   state to hold, not a number to admire.
4. **Some things that look like bugs are load-bearing and must survive.**
   `fixedDivUnchecked` goes through `double`; the trig tables are sampled at bucket
   centres; `slopeDiv` gives up under 512; `pointToAngle2` lands one unit below
   north; and `BBox::add` is `else if`, not an independent min and max.
   `Tests/Sim/PrimitiveTests.cpp` and `Tests/Sim/MathTests.cpp` pin these on
   purpose — a refactor will want to "fix" all of them, and each would desync
   every demo ever recorded. The *preserved defects* listed in `REFACTOR.md` are
   the same rule applied to outright bugs: documented at their sites, not fixed.

## Layout

- `src/DOOM/` — **the engine, and the code we own.** Built as the `doom-engine`
  static library, which both the app and the tests link, so a change to the
  simulation reaches both and neither can run code the other is not.

  It is C++20. There is no C left anywhere in the repository, no flat layer, and
  no vanilla naming: `src/DOOM` holds exactly three files at top level —
  `DOOM.h` (the public interface an embedder includes, a plain C++ header in
  `namespace Doom`), `doomtype.h` (the `byte` foundation plus the enum helpers
  below), and `Containers.h` (the container vocabulary). Zero `.cpp` files.

  **The containers are unqualified `Doom::` names.** `Containers.h` mirrors
  eacp's own `<eacp/Core/Utils/Containers.h>` — it pulls in the four
  `ea_data_structures` headers and re-exports `Array`, `Vector`, `OwnedVector`,
  `OwningPointer` and `makeOwned` into `namespace Doom`, so a signature reads
  `Vector<T>&`. Include it rather than the individual headers.

  One carve-out. **`DOOM.h` stays standard-library-only** and spells its argument
  vector `std::vector` — an embedder should not need eacp's containers to call
  `initGame`. **There
  are no `extern` variables anywhere in the repository** — a grep for a bare
  `extern` (outside comments and `extern "C"` prose) comes back empty, and it
  should stay that way. What used to be an `extern` global is now either a member
  of an `Engine` state cluster reached through its accessor (`level().sectors`,
  `graphicsData().textures`, `automapView().m_origin`, `videoState().screens`,
  `wipeState().meltRunning`, …) or, for the generated/config data tables that stay
  defined in one `.cpp`, a free accessor function (`states()`, `mobjinfo()`,
  `finesine()`, `S_sfx()`, `defaults()`, …).

  **The nine subdirectories *are* the engine**, all real C++ in `namespace Doom`:

  | Directory | Files | What it is |
  |---|---|---|
  | `Sim/` | 69 | the whole playsim — `Movement`, `MapAction`, `Enemy`, `Player`, `Weapon`, `Sight`, `Interaction`, the specials' spawners/handlers, `Thinker`/`ThinkerList`, `Tick`, `Setup`, `SaveGame`, `Info`, plus `Random`/`Level`/`MapGeometry` |
  | `Thinkers/` | 18 | the nine things that act once a tic, one type per file — `Mobj` and the eight specials (`FireFlicker`, `LightFlash`, `Strobe`, `Glow`, `Plat`, `Door`, `Ceiling`, `FloorMove`), each carrying its own `tick()` |
  | `Game/` | 56 | game loop, netcode, config, args, sound dispatch, and most of the `Engine`'s state clusters |
  | `UI/` | 42 | menu, HUD, status bar, automap, intermission, finale, screen melt, cheats |
  | `Render/` | 38 | the software renderer, all eight units — `Main`, `BSP`, `Segs`, `Planes`, `Things`, `Draw`, `Data`, `Sky`, plus `Video`, and the `Drawers` drawer-selection cluster |
  | `Math/` | 12 | `Fixed`, `Angle`, `Trig`, `BBox`, `Vec` (`Vec2`/`Vec3`/`Vec2i`), `Swap` |
  | `Host/` | 12 | the platform boundary — `Video`, `System`, `Sound`, `Net`, `Api`, `Host` |
  | `Wad/` | 3 | `WadFile` |
  | `Engine/` | 2 | `Engine`, the composition root |

  `src/DOOM/CMakeLists.txt` still splits compile flags between a "vanilla" bucket
  and a "rewritten" one. That split has finished moving — the vanilla glob matches
  two top-level headers and no translation unit at all — so everything compiled is
  under the strict flags. Keep the machinery; it costs nothing and documents the
  rule.

### The `Engine` is the composition root

`Engine/Engine.h` aggregates ~83 state clusters reached through free accessors
into the one `engine()` instance — `randomness()`/`wad()`/`level()`, the
renderer's (`viewPoint`, `graphicsData`, `drawState`, `spriteState`, …), the UI's
(`menuState`, `automapView`, `hudState`, `statusBarState`, `wipeState`, …) and the
game's (`gameSession`, `playerState`, `demoState`, `netState`, …). `doomstat.h`,
`r_state.h` and `p_local.h` do not exist.

Readers reach a cluster through its owner, hoisting a local reference once per
function (`auto& draw = drawState();`) rather than calling the out-of-line accessor
per access, which matters in the per-pixel drawers. The `Engine` is **constructed**,
not booted; `Engine/resetEngineMakesAFreshInstance` proves it.

**The host callbacks live on `Doom::host()`**, a deliberately separate immortal
singleton that must *not* be reset with a fresh Engine. The `doom_print` /
`doom_malloc` / … reference-aliases that used to stand in front of them are gone;
call sites reach the members directly (`host().print(...)`, `host().malloc(...)`),
and `doom_flags` moved onto the same singleton as `host().flags`. The members are
`std::function`s constructed with working defaults (stdio, `gettimeofday`,
`eacp::getEnv` — `Host/Host.cpp`); an embedder overrides them by assigning
`Doom::host().print = …` directly. String-shaped hooks take `std::string_view`
(`getenv` returns `std::optional<std::string>`).

**The loose pointer-and-count views are gone.** `vertexes`/`numsegs`/`sectors`/…
were views onto `Doom::Level` refreshed by each loader; readers index the vectors
directly now (`level().sectors[i]`, `level().segs.size()`), so a count cannot drift
from the thing it counts and the view cannot go stale. The same happened to
`GraphicsData`'s (`textures`/`sprites`/`textureheight`/… are `graphicsData()`
members) and `Math/Trig`'s (`finesine`/`finecosine`/… are `finesine()`-style
accessors over tables now file-local to `Trig.cpp`). The **drawer function
pointers** are gone too: `colfunc`/`spanfunc`/`fuzzcolfunc`/… were raw
`void(*)()` globals and are now `std::function` members of the `Drawers` cluster
(`Render/Drawers.h`), reached as `drawers().column()` / `drawers().span()` — the
per-column indirection was measured and the demo suite's wall clock did not move.
The automap's vector shapes and the melt's state are deliberate exported
carve-outs (`UI/AutomapTypes.h`, `UI/Wipe.h`, both including their state clusters)
that the eacp compositor reads through accessors (`mapShapes()`, `automapView()`,
`wipeState()`). Being exported does not mean being at `::` scope: everything
`UI/AutomapTypes.h` declares — the `MapLine` shapes, the colour indices, the line
counts — is in `namespace Doom`, and `examples/EACP` qualifies them.

Three constraints died with the single header and the code may rely on their
absence: **two files may share a file-scope name**, a source file may include a
system header, and the header include graph need not be acyclic.

### Enums are all scoped

**Every enum in the repository is an `enum class` with PascalCase enumerators.**
A grep for a raw `enum` comes back empty, and it should stay that way.

`doomtype.h` carries the two helper families the conversion rests on:

```cpp
template <typename E> requires std::is_enum_v<E>
constexpr int toIndex(E value);              // the integer an enum value names

constexpr int  flagBits(E...);               // OR several flags together
constexpr bool hasFlag(int bits, E...);      // ANY-of test
constexpr int  withFlags(int bits, E...);    // set
constexpr int  withoutFlags(int bits, E...); // clear
constexpr int  toggledFlags(int bits, E...); // ^
```

Five things to know before touching them:

- **They are functions, not operator overloads, deliberately.** Overloading `&`
  and `|` would make enum-to-int implicit again, which is the thing `enum class`
  is here to prevent. A named call says at each site that a bit operation is
  happening. `hasFlag` is an ANY-of test, which is what `bits & MASK` meant at
  every call site here.

- **The flag *words* stay plain `int`.** `Mobj` and `MobjInfo` are memcpy'd whole
  by the savegame, `Ticcmd::buttons` goes onto the wire, and `mobjinfo[]` composes
  flag sets at compile time. The enum types the individual flags; the helpers
  spell the combination. `flagBits` is variadic over *heterogeneous* enums, which
  is what lets `cmd.buttons = flagBits(ButtonCode::Special, SpecialCommand::Pause)`
  read naturally — those two occupy different bits of the same byte.

- **`toIndex`'s enum constraint is load-bearing.** Wrapping a non-enum is a hard
  error, which makes a mechanical sweep self-checking. It is what caught
  `givePower`'s `int /*PowerType*/ power` and `setPsprite`'s `int position`, both
  of which had been carrying their real type in a comment.

- **Not everything that was an `enum` became an `enum class`.** Where nothing ever
  *holds* the type and the names are only ever positions or magnitudes, they are
  `constexpr int`: `PowerDuration`'s tic counts (assigned into `powers[]` and
  counted down), `Math/BBox.h`'s `boxTop`..`boxRight` (they only subscript a raw
  `fixed_t[4]`), `Wad/MapFormat.h`'s `mapLumpLabel`..`mapLumpBlockmap` (only ever
  added to a lump number), and the mask/shift members prised out of the flag enums
  — `mobjTranslationMask`/`Shift`, `buttonSpecialMask`, `buttonWeaponMask`/`Shift`,
  `buttonSaveMask`/`Shift`. The test is whether a variable, parameter or field ever
  has the type. If none does, it is a set of constants, not a type.

  The converse also holds, and the compiler tells you which: `UI/Menu.cpp`'s
  fourteen menu-item enums went to `constexpr` first and raised
  `-Wunused-const-variable` for every item no menu indexes by name. Enumerators do
  not warn when unused, and that is the evidence a closed enumeration was the right
  shape.

- **`ButtonCode` is split in two.** Vanilla's single enum hid that `BT_*` and
  `BTS_*` are different vocabularies for the same byte — `BTS_PAUSE` and
  `BT_ATTACK` are both bit 0, and which applies depends on whether `BT_SPECIAL` is
  set. They are `ButtonCode` and `SpecialCommand`.

**Switches over a small enum list their cases**, with no `default:`, so `-Wswitch`
is what notices a new enumerator. Where a `default:` survives it is for one of
three reasons, all legitimate: the switch is over a plain `int` (`line.special`,
`gamemap`, a keypress); it is over one of the generated-table enums, where listing
968 `StateNum`s would be worse than the default; or it decodes a byte read from a
savegame, where the default is the corrupt-file error path.

Two hazards when converting or extending an enum:

- **A `bool` explicitly cast to an enum compiles silently.** A sweep that rewrote
  `if (x->meleestate)` into a `!= StateNum::Null` comparison also caught *value*
  contexts, producing `static_cast<StateNum>(x->meleestate != StateNum::Null)` —
  which sent every melee-capable monster to state 1 and hung the playsim in an
  infinite `setMobjState` loop. Zero warnings. After any such rewrite, grep that
  the comparison never appears outside a conditional. The explicit cast you added
  for safety is exactly what silences the type system.
- **The generated tables are checksum-pinned**, which is what makes a 1,400-name
  rename safe: `Info/stateTableIsIntact` and `Info/mobjInfoTableIsIntact` mix the
  enum fields as their integer values, so the checksums are unchanged by scoping
  and still pin the transcription.

### One `namespace Doom` per file, and the engine never qualifies itself

**Every file that opens `namespace Doom` opens it exactly once**, and **no engine
file writes `Doom::`**. Both are greppable invariants, and they are the same
invariant: a `Doom::` inside the engine only ever appeared because the declaration
had been left at `::` scope, so the prefix was *required* rather than redundant.
There were 170 namespace blocks across 37 files (`Sim/MapTypes.h` had 11,
`Wad/MapFormat.h` 18, `Game/StringsEnglish.h` 23) and 158 `Doom::` occurrences, and
**not one of the 158 was redundant** — the reopening and the prefix were two
symptoms of one cause.

The blocks were split so that *comments* could sit between them, which is what a
mechanical namespace-wrapping sweep leaves behind. Merging them is not cosmetic:
while a file is in pieces there is no single place a declaration belongs, so the
next one lands at `::` scope and needs a prefix again.

Four things to know before touching this:

- **`using namespace Doom;` is gone from every file** (it was in `Sim/Info.cpp`,
  `Sim/Items.cpp` and `Game/SoundData.cpp`, each so a verbatim 1993 table could name
  `MF_*`/`S_*`/`SPR_*` unqualified while sitting at `::` scope). The tables are
  inside the namespace now and name those enumerators directly. **A namespace-block
  scanner must exclude `using namespace Doom`** — matching it makes the *next* `{`,
  which is a table's opening brace, look like the namespace's, and the merge then
  eats the table's closing `};`. That mis-parse silently corrupted exactly those
  three files, and it is the reason to run the scanner's own output past a build
  rather than trusting it.
- **What deliberately stays at `::` scope**, and it is a short list: `doomtype.h`'s
  `byte`, `Host/Platform.h`'s `doom_abs`/`doom_memset`/`doom_memcpy` (the memory
  primitives), `Game/Args.h`'s `myargv()`/`myargCount()`, `Host/Sound.h`'s
  `mixbuffer()`, and the `#define`s that cannot leave the preprocessor. Everything
  else is in the namespace.
- **A `#define` hoisted out of a merged block must take its comment with it.** Three
  files (`Game/Strings.h`, `Game/StringsEnglish.h`, `Game/GameDefs.h`) keep macros
  whose whole justification is a comment explaining why they cannot become
  `constexpr`; moving the macro and stranding the rationale is how that comment stops
  being read. An `#include` between two blocks is the harder case and must be hoisted
  above the first block, never swallowed into the namespace.
- **The prefix moves outward, not away.** `Tests/` and `examples/EACP/` are outside
  `namespace Doom` and now qualify what they previously got for free — the automap's
  colour constants, `NUMPLYRLINES`, `FRACUNIT`/`FixedMul`/`FixedDiv`,
  `defaults()`/`numdefaults()`. That is the correct direction: the engine reads
  cleanly and its consumers say whose names they are using. `Math/FixedPoint.h`
  carried a comment asserting the opposite ("deliberately at `::` scope" so the tests
  could read `FRACUNIT` bare); letting a *test's* convenience decide an engine
  header's scope is the tail wagging the dog, and that comment is gone.

Two things this sweep found, both of which the goldens could not have: six files
carried a `// Global-scope data that was X.cpp. It stays at :: scope because these
are the vanilla names other translation units (and the eacp port) still link
against.` banner over a section that was **entirely comments** — every declaration
had migrated to the `Engine` long before, so the banner asserted a rule that
governed nothing, and `UI/Hud.cpp`'s identical banner was what justified keeping
three live tables outside the namespace (nothing outside the engine reads any of
them). And nine `nullptr` pointer globals in `Render/Data.cpp`, `Render/Things.cpp`
and `Render/Draw.cpp` were **dead** — vestiges of the view-removal sweep, each
survived by a same-named `GraphicsData`/`DrawState` member, each still carrying a
comment describing how `initTextures` refreshed it. Deleting them is
compiler-verified: a reader would have become an undeclared identifier.

### Types, containers and idiom

**The vanilla types and function names are gone.** All 107 types are PascalCase in
`namespace Doom` (`mobj_t`→`Mobj`, `line_t`→`Line`, `player_t`→`Player`, …), and
every call site calls the namespaced function (`drawPlanes`, `tryMove`,
`displayFrame`, `fatalError`, `cacheLumpNum`). No prefixed
spelling survives; do not go looking for one. The state-action adapter layer is
gone: `Sim/Info.cpp`'s `states[]` used to store every action through one
type-erased `void(*)(void*)` and a `Sim/Actions.{h,cpp}` forwarding shim, and now
`State::action` (`Sim/ActionFunc.h`) carries **two typed function pointers** — a
`void(*)(Mobj&)` and a `void(*)(Player&, PspDef&)`, only one set per state — so the
table names the real playsim/weapon functions directly with no cast on the call
path.

**`Fixed` and `Angle` are the strong types, spelled by their own names
everywhere.** The `using fixed_t = Doom::Fixed;` / `using angle_t = Doom::Angle;`
aliases that used to stand in for them are retired — a grep for `fixed_t`/`angle_t`
finds only the comments recording that they are gone. `FixedMul`/`FixedDiv` survive
as thin operator wrappers for readability.

#### Points are `Vec2` / `Vec3` / `Vec2i`, not loose pairs

`Math/Vec.h` holds all three: `Vec2` (two `Fixed`, the map plane), `Vec3` (three,
with `.xy()` and `setXY` — the playsim is mostly two-dimensional and z is carried
along), and `Vec2i` (two `int`, a screen pixel). **`Vertex` and `MapPoint` are
aliases of `Vec2`**, which is what lets `line.pointSide(*seg.v1)` take an endpoint
straight from the map. A `Mobj` holds `pos` and `mom`; a `Line` holds `delta`; a
`Node` holds a whole `DivLine partition`, so the cast vanilla used to reach it with
has nothing left to do.

**They are aggregates and must stay aggregates.** Every site builds one with
`{x, y}` and the savegame still `memcpy`s a whole `Mobj`, so a user-declared
constructor would break both at once.

What deliberately stays a loose pair, and why — the rule is whether the two numbers
are one *quantity*:

| Stays loose | Why |
|---|---|
| `Wad/MapFormat.h`'s `short x, y` | `reinterpret_cast` onto raw WAD lump bytes |
| `Blockmap::contains`/`index`, `forEach*InBlock` | `bx`/`by` are loop counters over a cell *range* derived from a box's four edges, not a point. `blockOf(Vec2)` exists for the sites that do convert a point |
| `VisSprite::gz`/`gzt` | the two ends of a vertical extent, not `gpos`'s third component |
| `DOOM.h`'s `mouseMove(int, int)` | the embedder's public interface; the pair is a delta an untyped caller supplies |

`DegenMobj` must keep `pos` at the same offset as `Mobj`'s — the sound code casts
one to the other. That is not a `static_assert` (both are polymorphic, so `offsetof`
would draw `-Winvalid-offsetof`); `Tests/Sim/StateClusterTests.cpp` puts the actual
cast through its paces instead, and inserting a field before `pos` fails it.

**`doom_boolean` is gone**; a boolean is a `bool`. Four declarations **stay `int`**
on purpose, each saying so at its site, because each is storage that only *looks*
like a flag: `Render/Data.cpp`'s `MapTexture::masked` (overlaid on raw `TEXTURE1`
bytes), `Game/GameSession.h`'s `deathmatch` (tri-state: 0 coop, 1 deathmatch, 2
altdeath), `Sim/Specials.cpp`'s `AnimDef::istexture` (its table ends on a `-1`
sentinel that would read as `true`), and `Host/Net.cpp`'s `trueval` (its address
goes to `ioctl(FIONBIO)`, which reads a whole word back through it).

#### `Array<T, N>` — five things that bite

The fixed-size C arrays are `Array<T, N>`. **19 are deliberately still raw**, and
the distinction is by *struct*, not by file:

| Still raw | Why |
|---|---|
| `Wad/MapFormat.h`'s 8 structs | `reinterpret_cast` onto raw WAD lump bytes |
| `Render/RenderTypes.h`'s `Patch::columnofs` | the same — and a *flexible* array, declared `[8]` but indexed to `[width]`, with pixel data starting at `&columnofs[width]` |
| `Game/PlayerTypes.h`'s 9 | `Player` is `memcpy`'d whole by `Sim/SaveGame.cpp`; `IntermissionStart`/`IntermissionPlayer` are `memcpy`'d to the `-statcopy` address |
| `Game/NetTypes.h`'s `NetPacket::cmds` | packed onto the wire, checksummed through a `reinterpret_cast<unsigned*>` |

- **`EA::Array` value-initializes; a raw C array does not.** Its sole member is
  `ContainerType container {}`, so `EA::Array<char, N> x;` zeroes where
  `char x[N];` left garbage. "I left the `= {}` off, so nothing changed" is false.
- **It adds no storage**, so it is layout- and size-identical to the raw array —
  an implementation fact about eacp, not a language guarantee. One place depends on
  it: **`VisPlane::top`/`bottom` are indexed out of bounds on purpose**,
  `Render/Planes.cpp` writing a `0xff` sentinel at `[minx - 1]` and `[maxx + 1]` so
  the span loop needs no bounds test. That is what `pad1`..`pad4` are for, and
  `RenderTypes.h` pins it with a `static_assert`.
- **`EA::Array` does not decay to `T*`.** Sites using the bare array as a pointer
  need `.data()`; `&arr[i]` is fine. The pointer-*difference* idiom is the one that
  hides — `player - players_.players` computes a player index in four places.
- **`&arr[N]` — the one-past-the-end pointer — is the exception, and no compiler
  catches it.** On a raw C array it is the ordinary way to write an end pointer; on
  `EA::Array` it is `std::array::operator[]` out of range, and **MSVC's debug STL
  asserts on it** — a *runtime* failure, in `Debug` only, on one toolchain, in code
  that runs correctly everywhere else. Use `.data() + N`. Its cousin is the
  deliberate out-of-bounds subscript (the `VisPlane` sentinel above), which needs
  `.data()` for the same reason.
  `grep -nE '&[A-Za-z_.>-]+\[[A-Za-z_:]*(max|MAX|NUM)'` finds the family.
- **`EA::Array<char, N>` is not an aggregate**, so a bare string literal in a table
  stops binding. Verify any bulk string change by extracting every literal before
  and after and diffing them.

**What must not become a `Vector`.** An `Array` earns a `Vector` only when its
length is *data* — decided by the WAD or the map — so the cap, the companion count
and any terminator collapse into `size()`. The renderer's pools look like the same
shape and are not:

| Stays `Array` | Why a `Vector` breaks it |
|---|---|
| `PlaneScratch::visplanes`/`openings`, `BSPScratch::drawsegs`, `SpriteState::vissprites`, `SolidSegs::solidsegs`, `Clip::intercepts` | each hands out **interior pointers** that outlive the statement — `openings` worst of all, storing a *biased* pointer (`lastopening - start`) inside a `DrawSeg` |
| anything `memcpy`'d or `memset` with `sizeof(container)` | `sizeof` silently becomes the vector's three pointers, and **no compiler warns** |
| `VisPlane::top`/`bottom` | the deliberate out-of-bounds sentinel writes |
| ring buffers (`events`, `itemrespawnque`, `bodyque`, `chatchars`) | head/tail are wrap cursors, not a live count |
| slot-indexed tables (`activeplats`, `activeceilings`, `buttonlist`) | the save game archives **by slot**; slot identity is the meaning |
| `StatusBarWidgets`' arrays | the STlib widgets hold `&w_arms[i]` |
| fixed domains (256 palette entries, `MAXPLAYERS`, 8 rotations, the trig tables) | the size is the domain, and several are type-punned |

Before converting one, `grep` the owning struct for `memcpy`, `memset`, `sizeof`,
`reinterpret_cast` and `&…[`, and ask whether any pointer into it outlives the
expression.

#### The thinker list is an `OwnedVector<Thinker>`

Vanilla's `thinkercap` — the circular doubly linked list every mobj and moving-sector
special hangs on — is gone, and so is `LevelPool`, the *second* intrusive list that
owned the same blocks purely so a level reload could free them. `Sim/ThinkerList.h`
is `using ThinkerList = OwnedVector<Thinker>;` and it is both: it holds the order and
it holds the ownership. `Thinker::prev`/`next`, `addThinker(Thinker&)`, `levelAlloc`,
`levelFree`, `freeLevelAllocations`, `initThinkers`, `LevelChunk` and `LevelPool` all
went with them; allocation and registration are one call, `addThinker<T>()`
(`Sim/Tick.h`), and the level reset is `thinkerList().clear()`.

The list earned none of its linked-ness — nothing walked it backwards, and nothing
unlinked from the middle, removal being the lazy `removed` flag with the only unlink
at `runThinkers`' own cursor. **`OwnedVector` is `Vector<OwningPointer<T>>`, so the
elements never move**, which is the load-bearing fact: every `Mobj*` held elsewhere
(a `target`, `player->mo`, a sector's `soundtarget`/`specialdata`, the
`activeplats`/`activeceilings` slots, the blockmap and sector links) survives an
append. A by-value `Vector<Thinker>` could not exist, and that distinction is what
the old "a container is not available at any price" note in `REFACTOR.md` missed.

Five things to respect:

- **`runThinkers` iterates by index, re-reading `size()`.** A `tick()` spawns
  thinkers, which append here, and vanilla ran those in the *same* tic; an index loop
  reproduces that exactly. A range-for or cached iterator does not, and the append
  reallocates the buffer under it.
- **Erase at the cursor with `removeAt`** — an order-preserving `erase`. Never
  swap-and-pop: the order *is* the simulation, and the demo goldens fail on the tic.
- **Never bind a reference into the vector's buffer across a `tick()`.** `*thinkers[i]`
  is safe because it is the object, not the slot. `thinkers[i]` is not.
- **A walk that mutates must be by index too.** `Line::teleport` spawns fog mid-walk
  and only survives a range-for because every such path `return`s first; it is written
  as an index loop so that stays true. The read-only walks (`Enemy`, `SaveGame`,
  `Render/Data`, the port, `SimProbe`) are plain range-fors over `OwningPointer<Thinker>&`.
- **`Thinker` declares its own `operator new`/`operator delete` over
  `host().malloc`/`free`,** because `DOOM.h` lets an embedder replace those and
  `Tests/Sim/OwnershipTests.cpp` counts blocks through exactly that pair. A plain
  `new Mobj` compiles and runs and makes the leak test measure nothing.

`Thinker` is abstract (`tick()` is pure) — the sentinel head was the only thing that
needed a concrete base. That promptly caught `DegenMobj`, a sector's sound origin,
which inherits `Thinker` only so its `pos` sits at `Mobj::pos`'s offset and is the one
`Thinker` never *in* the list; its `tick()` is empty and never called.

#### There is no thinker type tag

`ThinkerKind` — the `enum class` that replaced vanilla's function-pointer identity
test — is gone too, and the reason generalises past this hierarchy. **A discriminator
plus a cast is two independent statements of one type, and no compiler checks either
against the other.** `kind() == ThinkerKind::Ceiling` sitting next to
`archiveSectorThinker<Ceiling>` compiles just as happily with the wrong type in the
second half. The tag was also answering two unrelated questions:

| Question | Asked by | Now |
|---|---|---|
| "is this a `Mobj`?" | `Sim/Enemy` ×4, `Sim/Teleport`, `Render/Data`, `Sim/SaveGame` ×2, `examples/EACP`, `Tests/SimProbe` | `virtual Mobj* asMobj()` on `Thinker`, `return this` in `Mobj`, overridden nowhere else. It hands back the *typed pointer*, so a caller cannot test one type and cast to another |
| "which special is this?" | `Sim/SaveGame`'s `archiveSpecials`, once per save | `dynamic_cast`, via `archiveSpecialIfType<T>` |

`asMobj()` is a plain virtual rather than a `dynamic_cast` because it is genuinely
hot: `Engine::emitSprites` asks it of every thinker on every frame, and `SimProbe`
hashes through it every tic. `archiveSpecials` is the opposite — once per save — and
is the only place that needs the *static* type, to size the record it `memcpy`s. What
stays paired there is a type and its **wire tag** (`SpecialClass::Ceiling`), and that
pairing is the file format; it cannot be derived from anything.

**Its coverage is partial, and measured.** `Sim/saveLoadPreservesTheWorld` fails if the
`LightFlash`/`Strobe`/`Glow` branches go (E1M1 spawns those at load) and passes with
`Ceiling`, `Door`, `Floor` or `Plat` deleted — nothing in the suite saves a game with a
door open or a lift moving. That gap predates the `dynamic_cast` and is unchanged by
it, but it is the sharpest remaining hole in `p_saveg`.

#### Constants, and the guard/bound rule

**Every overflow guard must test the same constant that sizes the array**, not a
second one of equal value — otherwise raising the bound moves the array and leaves
the guard behind, silently and with no diagnostic. Fixed-size members are sized by
their own cluster's constant (`PlaneScratch::maxVisplanes`/`maxOpenings`,
`SpriteState::maxVisSprites`, `BSPScratch::maxDrawSegs`, `SolidSegs::maxSegs`,
`DrawTables::maxWidth`/`maxHeight`, `SwitchList::maxSwitches`,
`AnimatedSurfaces::maxAnims`/`maxLineAnims`, `AutomapView::numMarkPoints`,
`Clip::maxSpecialCross`). Do not add a second spelling of any of these. The
enum-derived counts follow the same rule: `numAmmo` is `toIndex(AmmoType::NumAmmo)`,
so a count and its enum cannot drift.

Where two constants must agree across a subsystem boundary, the fix is a
`static_assert`, not a third spelling and not a comment. `Game/Game.cpp`'s
`SAVESTRINGSIZE == menuSaveStringSize` and `Host/Host.cpp`'s `SeekOrigin` ↔
`SEEK_*` check are the worked examples — the latter pins an assumption vanilla
relied on silently, that DOOM's seek values match the C library's.

**The real category is "the guard and the array bound are not the same token"**,
which is wider than "a macro with a `constexpr` twin". `Sim/Mobj.cpp` bounded an
array sized `MAX_DM_STARTS` with a bare literal `10` — no second spelling existed,
so no grep for duplicate constants could have found it.

The constant *macros* are closed (629 → 240 across `src/DOOM`, of which 199 is
`Game/StringsFrench.h`, which no build compiles — so 41 in everything that
compiles). What remains is deliberate: the string families that cannot leave the
preprocessor because adjacent-literal concatenation happens at translation phase 6
(`PRESSKEY`, `DOSY`, `DEVDATA`, `DEVMAPS`), and the feature toggles read by
`#ifdef` (`RANGECHECK`, `Host/Platform.h`'s three, and the three `_CRT_*`/
`_WINSOCK_*` toggles that MSVC's own headers read — those three *look* unused to
any grep and are not).

**The dead-in-both-eras macros are gone** — 62 of them, deleted rather than
converted, which was always the choice on offer: converting one produces a
`[[maybe_unused]] constexpr`, an attribute whose only job is to silence the
diagnostic saying the thing is dead. Two things that sweep taught, both general:

- **The dead set is a fixed point, not one pass.** `ST_MAPWIDTH`'s only mention
  was inside `ST_MAPTITLEX`'s body, and `ST_MAPTITLEX` was itself dead; a second
  round found it. Iterate until a round adds nothing.
- **Deleting dead code can delete a live check with it.** `ST_NUMFACES` held the
  face count a third time, and `StatusBarGraphics.h` claimed a compile-time check
  against it — but that check had been a reference-to-array binding the alias sweep
  retired, silently, months earlier. `UI/Hud.h`'s `HU_FONTSIZE` against
  `HudFont::fontSize` was the same shape. Both are now `static_assert`s at
  `UI/StatusBar.cpp` and `UI/Hud.cpp`. When a comment claims a guard, check the
  guard is still there before trusting it — and prefer the `static_assert`, which
  cannot be retired by accident.

**A `constexpr` is implicitly parenthesized and several vanilla macro bodies are
not** (`PLAYERRADIUS 16 * FRACUNIT`), so equivalence is a fact to establish per
call site. What breaks is dividing by, or taking `.`/`->`/`[]` off, a bare macro.

The function-like macros are gone. One family survives on purpose: `Host/Net.cpp`'s
`ntohl`/`ntohs`/`htonl`/`htons` sit inside `#if defined(I_NET_ENABLED) &&
!defined(DOOM_APPLE)`, so **no build here compiles them** and no gate could check a
change to them.

#### Strings

The C string layer is gone. `Host/Text.h` replaced it — `concat(parts...)`,
variadic `print(...)`/`printTo(handle, ...)`, variadic `fatalError(...)`
(`Host/System.h`), `hexString`, `toUpper`/`equalsIgnoreCase`, and
`parseInt`/`parseHex` (exact ports of `doom_atoi`/`doom_atox`, sign handling and
all — the goldens pin their no-sign behaviour). Lump-name composition
(`"STTNUM"+i`, `"WIMAP"+epsd`, `E?M?`) is `concat(...)` straight into the WAD
lookups, which take `std::string_view`.

Five things to know:

- **An 8-byte WAD name field is NOT NUL-terminated when full**, and a
  `std::string_view` built from a bare `const char*` runs `strlen` off its end.
  `nameView(ptr, 8)` (`Host/Text.h`) is the bounded view; `WadFile.h` and
  `Render/Data.h` say so at their lookup declarations.
- **A `std::string_view` built from a null pointer is UB, and the null arrives as
  a literal `0` that no grep for `nullptr` will find.** This hazard hit twice, and
  both times the failure was a segfault a long way from the edit. Before converting
  a pointer that carries a string, grep its writers for a bare `0` as well as for
  `nullptr` — and remember a null guard (`if (prefix)`) becomes `.empty()`.
- **Fixed-width on-disk fields stay fixed-width**: the savegame's 24-byte
  description and 16-byte version fields are written with `fillField`
  (zero-padded, deterministic) and read back bounded.
- **A message pointer needs storage that outlives the frame.** `Player::message`
  is a `std::string_view`; everything assigned to it is a string constant or an
  Engine-owned/static `std::string`, reassigned in the same breath and never freed.
  **The constraint is trivially copyable, not `const char*`** — `Player` is memcpy'd
  whole, which rules out an *owning* type but not a view. `unArchivePlayers` clears
  it right after the memcpy.
- **What deliberately stays raw, and the rule that decides it.** The question is
  never "is this a `char*`" but **"does it carry text"**. Every survivor answers no:
  the WAD overlay structs and `WadFile`'s `matches` (a `memcmp` over 8 raw directory
  bytes), `Game/PlayerTypes.h`'s memcpy'd arrays, `doom_memset`/`doom_memcpy`,
  `nameView`/`fillField`'s own raw parameters (they are the primitives that *bound*
  raw bytes into a view), `Host/Net.cpp`'s `setsockopt` casts, `main`'s
  `char** argv` and the `initGame` overload that receives it, `UI/Hud.cpp`'s
  `shiftxform` (a 128-entry translation *table*), the PNAMES lump walk in
  `Render/Data.cpp`, and `Host/Sound.cpp`'s PCM and MUS cursors (11kHz 8-bit
  samples — the thing that most *looks* like a string and is furthest from one).

**The command line owns itself.** `myargv` is a `std::vector<std::string>`
(`Game/Args.h`), and `initGame` copies what the host hands it rather than keeping
the pointer — `DOOM.h` carries a `std::vector<std::string>` overload for hosts not
forwarding `main()`. The tokeniser keeps vanilla's own character class (a token runs
while the byte is `>= ' '+1` and `<= 'z'`), empty leading token included.
`myargCount()` returns `myargv.size()`, so the count and the container cannot drift.

#### References vs pointers

**A pointer parameter that can never be null is a reference — 108 functions are.**
The whole specials family, the playsim core (`tryMove`, `checkPosition`,
`setMobjState`, `removeMobj`, `removeThinker`, `touchSpecialThing`, `changeSector`,
`slideMove`, `lineAttack`, the `give*` family), the seven `Event*` responders, the
`Ticcmd` path, and the automap's line helpers. Nullability was established per call
site, not assumed.

**What deliberately stays a pointer, because null is a real value it carries:**

| Stays `T*` | Why |
|---|---|
| `damageMobj`'s `inflictor` and `source` | `MapAction.cpp`'s crush and `Specials.cpp`'s slime pass a literal null; the body tests both |
| `radiusAttack`'s `source` | `explode()` passes `thingy.target` unguarded, and the path is *designed* to accept null |
| `spawnMissile`'s `dest` | the `fatAttack1/2/3` family passes `actor.target` with no guard |
| `checkSight`'s `t1`, `aimLineAttack`'s `t1` | `fire()` and `bfgSpray` pass `.target` unguarded |
| `killMobj`'s `source` | inherits `damageMobj`'s |
| `Render/Planes`' `checkPlane` | `BSP.cpp` nulls `floorplane`/`ceilingplane` outright |
| `drawMaskedColumn`, `drawColumnInCache` | the parameter is a loop *cursor*, reassigned to walk posts |
| `startIntermission` | it **stores** the pointer well past the call |
| the `drawPatch` family | ~45 call sites pass `cacheLumpName(...)` straight in; the real fix is a `Patch&`-returning lookup |

`SaveGame.cpp` nulls **every** mobj's `target` on load, which is what turns those
unguarded `.target` reads from theoretical into reachable. That single fact holds up
most of the column.

Three things that bite:

- **Capturing a now-reference parameter *by value* in a lambda copies the whole
  object, and the goldens will not see it.** `Sim/Switches.cpp`'s `startButton` had
  `[line](const Button& b) { return b.line == &line; }` — with `line` a `Line&`,
  `&line` is the address of the closure's *copy*, so the test could never match. It
  compiled, and **all tests passed with it in**. After any pointer→reference sweep,
  grep every lambda capture list in the touched files.
- **`sizeof` is the one place this does *not* bite.** `sizeof(*ptr)` becomes
  `sizeof(ref)`, and a reference's `sizeof` is the referent's.
- **Rewriting `param->` to `param.` over-reaches on `other->param->`.** Harmless:
  a pointer member reached with `.` is always a compile error.

---

- `Tests/` — the test suite. See **Testing**.
- `examples/EACP/` — the eacp port. `Main.cpp` boots the engine, `View.h` is the
  eacp platform layer and GPU renderer, `Audio.h/.cpp`, `Genmidi.h/.cpp` and
  `OplPlayer.h/.cpp` are the sound device and the music synth (the three files here
  that need no GPU besides `EngineAccess`, and are compiled into the test binary for
  it), and `EngineAccess.h/.cpp` is the snapshot interface to engine internals. `EngineAccess.cpp` is an ordinary translation unit
  that includes the engine's headers; nothing DOOM-typed leaks out through the `.h`,
  and the renderer never sees a `fixed_t`.

  **It is `namespace PureDoom::Engine`, and it is ordinary C++** — `Engine::camera`,
  `Engine::buildGeometry`, `Engine::hudSprites`, not the `eacpDoom*` free functions
  and `typedef struct`s it began as. Read the name as "ask the engine"; it is this
  port's *view* of the engine, and has nothing to do with `Doom::Engine`, the
  engine's own composition root, which is always spelled with its namespace. The
  vocabulary is the rest of the repository's: `Vector`/`Array` (re-exported into
  `namespace PureDoom` by `EngineAccess.h`, the way `src/DOOM/Containers.h` does it),
  `bool` where a `bool` is meant, references where null is not a value, and
  `std::span` for the out-parameters — eacp has no span, and a view onto a caller's
  buffer is the one thing the EA containers do not spell.

  Two carve-outs, both forced by eacp rather than chosen. The **vertex layouts**
  (`WorldVertex`, `AutomapVertex`) hold `std::array<float, N>`, because
  `GPU::ShaderValueOf` — which is what turns `vertexInput(&WorldVertex::position)`
  into a `Float3` — is specialised for `std::array<float, N>` and raw `float[N]` and
  for nothing else. And the shader uniforms are assigned `std::array` for the same
  reason.

  The nine shaders share `DoomShader.h`, which holds all three bases: `DoomShader`
  resolves a palette index the way the software renderer does (index → COLORMAP row
  → palette) or writes the index out unresolved (`setIndexFragment`, which is what
  the world target holds); `ScreenQuadShader` adds the screen-space quad the
  full-frame passes draw; and `WorldViewShader` adds the camera and the projection,
  shared by the two shaders that draw off the world's vertex buffer — the surfaces,
  and the spectres' silhouettes marked over them, which have to agree to the pixel.
  Every shader is the difference from those, and nothing else.
- `doom1.wad` — the shareware data file the game boots with.

Upstream's SDL reference port used to live in `examples/SDL` and was the best worked
example of how the engine expects to be driven. It was deleted with the single
header; what it knew is written down under **What the engine expects of its host**.
`git log -- examples/SDL` still has it.

### Renderer status

Two paths, toggled at runtime with **Shift+F8**:

- **Software frame** (Stage A): the engine's palette-indexed framebuffer as an R8
  texture, palette looked up in the fragment shader. No CPU pixel conversion.
- **GPU world** (Stages B1-B3): the level drawn as real hardware 3D, at the
  window's resolution rather than 320x200.
  - Geometry is re-read from the live level every frame, so moving sectors, doors,
    animated textures and moving monsters need no invalidation. Walls come from the
    linedefs (both sides, with vanilla's pegging rules); floors and ceilings come
    from subsector polygons reconstructed by clipping a large square down the BSP,
    because vanilla nodes carry only split planes and the segs alone would leave
    holes at BSP cuts; every thing is a camera-facing billboard using the same
    eight-rotation frame the engine would pick.
  - Textures are composed from their patches, which is what makes a masked
    texture's holes come out as holes: the engine's cached columns are post data,
    not pixels, for exactly those textures. Masked textures and sprites carry
    coverage in alpha and are alpha-tested in the shader.
  - Shading is DOOM's own, not an imitation: the texture yields a palette index,
    the COLORMAP row chosen by sector light and distance remaps it, and the palette
    resolves the colour. Light banding, diminishing, fullbright frames and palette
    flashes all come out exact.

    A powerup can take that choice away: `R_SetupFrame` reads
    `player->fixedcolormap` and puts everything through one row with no light and
    no distance — the invulnerability sphere's inverse map (row 32), the visor's
    brightest row (row 1). The whole COLORMAP lump is therefore uploaded, all 34
    rows, and each vertex carries not just its row but *how much of the distance
    term applies*. Which is also why that is a vertex attribute rather than a
    uniform: **the sky is exempt from the powerups** and stays on row 0, a vanilla
    quirk its own source calls out.
  - The sky is a cylinder pinned to the camera, its texture repeating four times
    around, mapped so a screen row lands where the engine would put it.
  - The weapon and muzzle flash are drawn in screen space over the world. The
    weapon is **not** lit at its sector's start map, and lighting it that way draws
    it far too dark in almost every room: `R_DrawPSprite` reads
    `spritelights[MAXLIGHTSCALE-1]` — the *nearest* entry, the weapon being right
    against the camera — which is 23 rows brighter than the start map at a
    320-column view.
  - Geometry is grouped by texture into one draw per texture; textures upload
    lazily on first use (a WAD holds well over a thousand sprite lumps).
  - **The world is rendered into a texture, not onto the screen** — and what it
    writes there is DOOM's own frame, one palette index per pixel, resolved to
    colour by a full-screen pass afterwards (`ResolveShader`). Spectre fuzz is why
    (below): a pass cannot sample the target it is drawing into, so the world has to
    be finished before anything can read it. Keeping it in index space to that point
    is what makes the fuzz vanilla's own arithmetic rather than an imitation of it
    in colour space.

    The target is RGBA8 with the index in red and the fuzz mark in green, at the
    window's pixel size, rebuilt only when that changes. Its depth buffer is its
    own (`TextureDescriptor::depth`, eacp's E1), and a texture pass is
    single-sampled whatever the view is — so the two pipelines that draw into it
    are built at sample count 1 and name the target's format, which is what
    `View::prepareTargetShader` exists to say.
- **Spectre fuzz** (B4): what DOOM does with a thing carrying `MF_SHADOW`, which is
  not to draw it. `R_DrawFuzzColumn` fills the sprite's shape with the pixels
  already in the frame behind it, one row up or down, remapped through COLORMAP row
  6 — so the sprite supplies only a silhouette.

  Three pieces, and the awkward one is preserving what is underneath:
  - `Engine::buildGeometry` hands the spectres back as their own runs
    (`WorldGeometry::fuzzDraws`) of the same vertex buffer.
  - `FuzzShader` draws them last, into the world target, **additively** — which is
    what leaves the index the world wrote in red exactly as it was while raising the
    mark in green. Overwriting it with a silhouette would leave nothing to distort.
    Depth is the world's, so a spectre behind a wall is not marked at all; drawing
    them last is what makes both true at once.
  - `ResolveShader` replaces a marked pixel with its neighbour a frame-row away,
    through row 6. Which neighbour comes from `drawFuzzColumn`'s own 50-entry table,
    indexed by the pixel's place in DOOM's 320x200 frame rather than in the window's
    — so the grain stays the size it was in 1993 however large the window is. The
    phase is the engine's own `fuzzpos` (`Engine::fuzzPhase`), read once a tic: the
    software renderer is still drawing the same spectres, and its walk advances a
    step per fuzz pixel it lays down, so the animation is content-driven exactly as
    vanilla's is and stands still when nothing in view is fuzzed. What cannot be
    reproduced is *where* in the table a given column starts — vanilla's cursor runs
    down one column and on into the next, which a fragment shader cannot know — so
    the column contributes a fixed step instead.

  The **weapon** is the same effect on the player's own sprite while the
  invisibility sphere is up (`drawPSprite` nulls the colormap before it looks at the
  light or at a powerup's fixed one, so it outranks both, and blinks as the sphere
  runs out). `HudFuzzShader` marks it into the world target with the spectres rather
  than drawing it over the screen with the rest of the HUD, so it needs no second
  copy of any of the above.
- **GPU automap**: the map as geometry rather than a rasterized frame. What it
  draws and the colour it picks are `AM_Drawer`'s own choices, mirrored in
  `Engine::buildAutomap`; only its Bresenham walk is replaced, by a quad per line
  the vertex shader widens, and its Cohen-Sutherland clip, by the scissor rect the
  draw is bounded with — the map window is routinely smaller than the level, so
  without that bound the lines beyond it spill over the status bar. Two things vanilla's rasterizer cannot do come out of
  it: the lines keep their real endpoints instead of snapping to whole pixels, and
  the map is recentred on the *interpolated* view rather than the player's last
  tic, so it glides at the display's rate instead of crawling at 35Hz.

  **The one place the port departs from vanilla on purpose**
  (`Engine::revealAutomap`). A wall is revealed as a *side effect of being drawn*:
  `R_StoreWallRange` sets `ML_MAPPED` as the software renderer lays it down. But
  `D_Display` skips `R_RenderPlayerView` entirely while the automap is up, so
  vanilla's map stops filling in the moment you look at it. Most source ports
  quietly fix this, and so does this one: the BSP is walked once a tic while the map
  is up. It stops there — the planes and sprites are never drawn, and
  `R_RenderPlayerView`'s four `NetUpdate` calls are not wanted, as they drain the
  event queue. The walls it *does* draw land in the frame the automap had just drawn
  itself into (the column drawers write through `ylookup`, aimed at `screens[0]`),
  so the map is drawn again afterwards to put it back.
- **Overlay** (`Engine::buildOverlay`): the layers the engine draws over the view
  in software and nothing else reproduces — HUD messages, the level name, PAUSE,
  the menu, the automap's marks. The engine offers no way to draw them anywhere but
  over the frame it has just rendered, so they are captured: `screens[0]` is pointed
  at scratch, those drawers alone are run, and the real frame is put back. Coverage
  cannot be read off one pass — a pixel the menu legitimately drew may hold whatever
  the scratch was primed with — so each layer is drawn twice over two differently
  primed buffers and counts as covered exactly where the two agree. They are pure
  (the skull blinks on `M_Ticker`, not `M_Drawer`).

  It is captured as *two* layers, because a menu darkens the frame it finds and
  then draws itself over it: a message, the level name and PAUSE dim with the world,
  while the menu stays bright. The green channel says which.
- **Menu darkening** is applied to the GPU view rather than to a framebuffer: one
  extra COLORMAP lookup in the world, automap, weapon and overlay shaders. That is
  exactly what `M_Drawer` does to its 64000 pixels, and it leaves the world at full
  resolution behind the menu. Row 0 is the identity, so playing costs the lookup and
  nothing else. The status bar needs none of this: the engine darkens its own frame,
  which is where the strip is sampled from.
- **The frame is composited into a texture and blitted**, rather than drawn
  straight onto the drawable, whenever the GPU renderer owns the view. Not for the
  picture — the blit is the identity — but so that the finished frame is still
  somewhere at the window's resolution *after* it has been presented, which is the
  only thing that makes the melt below full-resolution. Nothing is composited into
  it while a melt is running, which is what leaves it holding the frame the melt
  began over. Measured: the display holds 120.0 refreshes a second with the extra
  pass and 120.0 without it.
- **Screen melt**: drawn over the frame the renderer produced rather than instead
  of it. What it composites is

      column c, row r = the outgoing frame's row (r - offset[c]) when
                        r >= offset[c], and the incoming frame's row r otherwise

  which is `doMelt`'s own rule written the other way round — the engine copies
  runs of rows into place as the columns move, and a shader has to answer for one
  pixel with no memory of the last frame. `Tests/Port/WipeTests.cpp` holds the two
  against each other every tic of a real melt, which is the only gate either has
  ever had.

  **The outgoing frame comes from one of two places, and which one is the whole
  point.** A melt out of the title, an intermission or the finale is sliding away
  320x200 artwork, and `Engine::buildWipe`'s copy of it is exactly right. A melt
  out of a *level* — the end of every level, and every `IDCLEV` or load — is
  sliding away a frame this renderer drew, and that is the capture above.

  Getting there needed one thing the shape of the port did not offer. A melt out
  of a level runs with `gamestate` already moved on to the intermission, so
  `Engine::viewActive()` is false and the whole transition is on the **software
  path**, where `screens[0]` is the engine's own 320x200 composite of *both*
  halves — which is why the level used to drop to 320x200 at the instant it
  started sliding, and why nothing about the GPU melt path could have fixed it.
  `Engine::wipeIncoming` exports the incoming screen (`screens[3]`) so the port
  can draw that half alone and put the capture over it. Drawing the capture over
  the engine's composite instead would leave the copy it replaces showing wherever
  the two disagreed by a pixel.

  One departure from vanilla, at `CaptureShader`: the engine slides palette
  *indices* and resolves them through whatever palette is current, while the
  capture was resolved when it was taken. The two differ only if the palette
  changes during a melt — a damage flash on a level's last tic, which entering the
  intermission clears — and the captured answer is the one that matches what was
  on the screen.

  Two things it has to respect. The engine raises `is_wiping_screen` at the end of
  the frame that renders the incoming screen and only sets the melt up on the *next*
  one, so on that first frame there is no column table yet. And `wipe_exitMelt`
  frees the column table without clearing the pointer, so `go` is the only safe
  thing to test.
- **Screen size** (the menu's, which persists in `~/.doomrc` — default **9**, not
  10, so this is never hypothetical). The GPU renderer honours the two layouts that
  change what is on the screen: with the status bar (the 168 rows above it) and
  without it (screenblocks 11, the whole frame). At a *smaller* size it keeps
  drawing the full-width view rather than shrinking it into a border, that having
  been a concession to 1993 hardware.

  Ignoring 11 was not an option, only a bug: the engine then renders the view over
  all 200 rows and `ST_Drawer` draws no bar at all, so the strip composited from the
  software frame stops being the status bar and becomes a slice of the world. A
  taller view also wants a wider vertical field of view, which is one more scale on
  the projected y — the horizontal 90 degrees is unchanged.
- Anything outside a level (title, intermission, finale) falls back to the software
  frame automatically, which is right — those screens *are* 320x200 — and the status
  bar is always composited from it.

**Nothing on this list has a golden over it.** `Tests/Port` covers the *builders*
(`Engine::buildGeometry`, `Engine::buildAutomap`, and now the spectre split) as
data, and the frame goldens run the software renderer, which does not execute a line
of any shader here. A shader change is eyeball-verified, and the standing measurement
for one is a window capture with `MTL_DEBUG_LAYER=1` on: on Apple silicon a depth
test appears to work with no attachment at all, so a picture that looks right is only
half the evidence and a silent validation layer is the other half.

## Audio

Sound and music both play, out of the box, with nothing shipped alongside the WAD.

The output device comes from
[MakeASound](https://github.com/eyalamirmusic/MakeASound) (miniaudio behind
`DeviceManager`, RtMidi behind `MidiManager`), a CPM dependency of `examples/EACP`
alone — audio is the *host's* job, not the engine's. The music is voiced by an
emulated OPL3, [Nuked-OPL3](https://github.com/nukeykt/Nuked-OPL3), which is
fetched at the **root** instead, because unlike the rest of the audio path it is
testable and the test binary needs it too.

Four files, all GPU-free:

| File | What it is |
|---|---|
| `Audio.{h,cpp}` | the device, the producer, the mix |
| `Genmidi.{h,cpp}` | the IWAD's OPL instrument bank, decoded |
| `OplPlayer.{h,cpp}` | the synth: voice allocation over the emulated chip |

`View` owns an `Audio` and calls `audio.pump()` once a display refresh, **ahead of
the early return** that skips refreshes no tic falls on — the device wants feeding
either way.

Six things to know.

- **The mixer is pulled on the main thread, and this is the design, not a shortcut.**
  `Doom::soundBuffer()` mixes 512 frames *on the call*, reaching into the same engine
  state `Doom::updateGame()` writes. Pulling it from the device's callback — which is
  what `DOOM.h`'s comment about a lock describes, and what the deleted SDL example
  did — means holding a lock across the mix, on the audio thread. Pulling it from the
  thread that steps the engine removes the lock outright. What crosses the thread
  boundary is `MakeASound::SPSCQueue`, wait-free at both ends, carrying frames that
  are already finished.
- **The queue's occupancy is tracked separately, and it has to be.** `SPSCQueue` does
  not report its own size, and the producer needs it to know when to stop pulling.
  `Audio::queued` is an `std::atomic<int>` the producer adds to and the consumer
  subtracts from. The asymmetry is what makes it safe: the consumer only ever
  publishes a count *at or above* the true one, so a stale reading makes the producer
  wait a refresh — it cannot make it overrun. The capacity is then provably enough
  (`audioQueueFrames` = the headroom, plus the one chunk that tops it up, at the
  highest rate a stream is opened at).
- **The producer's clock is the stream, not the wall.** Frames are made one MIDI
  tick at a time — `sampleRate / 140`, with the remainder carried so 140 chunks come
  to exactly one second — and each tick's register writes land at the start of the
  frames they affect. This is what an in-process synth costs you and an out-of-process
  one does not: the producer runs *ahead* of the device by design, so pacing the music
  by the display's refresh would scatter every note by up to a whole tick.
- **512 frames at 11025 Hz is 46ms, and it is the floor under a sound effect's
  latency.** There is no smaller unit to ask the mixer for — one block is longer than
  a tic. It waits in a staging buffer and is drained across chunks, so it does not
  size the queue; music, which has no such granularity, is only ever the queue's 40ms
  behind.
- **The resampler carries its phase and its previous frame across the block
  boundary.** Restarting either per block puts a step at every join, 21 times a
  second.
- **The stream is built by hand rather than from `getDefaultConfig()`**, which also
  opens the default *input* — and asking for a capture device is what puts a
  microphone permission prompt in front of a game that never records anything.

### The music is an OPL, and that is why it needs no assets

`Doom::tickMidi()` hands back MIDI messages, not samples, so something has to voice
them. A General MIDI synth would need a sound bank shipped alongside the game — tens
of megabytes against a 4 MB shareware WAD. An OPL needs nothing, because **DOOM's
music was composed for the Adlib and the patches are already in the IWAD**:

```
GENMIDI  11908 bytes  ==  8 + 175 * (36 + 32)
```

`#OPL_II#`, then 175 instrument records, then 175 32-byte names — 128 General MIDI
melodic instruments and 47 percussion voices keyed by note. `Genmidi.cpp` decodes it
by offset rather than by casting a struct onto the bytes, so neither this machine's
padding nor its endianness can change what comes out.

Four things the bank turns out to say, none of which were assumptions worth making —
`Tests/Port/GenmidiTests.cpp` pins each, and the first two were written wrong first
and corrected by the test:

- Program 127 is `Gun Shot`, with a space.
- **Three of the 47 percussion voices are *not* fixed-pitch.** A player that assumes
  every drum ignores its note is wrong about three of them.
- 32 of the 128 melodic instruments are double-voice, so one note takes two of the
  chip's channels.
- Two *melodic* instruments are fixed-pitch.

`OplPlayer` is the synth over Nuked-OPL3: 18 two-operator voices across the OPL3's
two banks, stolen oldest-first (a voice the sustain pedal is holding goes before one
the score still has down). Notes become an F-number and a block by the chip's own
formula against its 49716 Hz clock rather than by a lookup table, so pitch bend is
just a fractional note. Velocity, channel volume and expression multiply into a gain
and then into attenuation steps at the chip's 0.75 dB each — **and only the carrier
is attenuated unless the patch is additive**, because in FM the modulator shapes the
timbre rather than carrying the level.

**The fallback path is still there and still justified.** A WAD with no GENMIDI, or
no audio device to render into, falls back to sending MIDI out of the process on a
**virtual port named `PureDOOM`** — which appears in the system's MIDI graph and
plays to nothing until the player routes it into a synth. RtMidi has no virtual ports
on Windows, which is also the one platform shipping a General MIDI synth as an
ordinary output port, so there the fallback's fallback is a real port, preferring one
that names itself a synth. That path alone uses `Engine::midiTime()` — the wall clock
in 140ths — because an external synth is not paced by our stream.

### What the tests reach, and what they still do not

`SimTests` compiles `Genmidi.cpp` and `OplPlayer.cpp` directly and links
`nuked-opl3`, exactly as it already does with `EngineAccess.cpp`. **This is the first
time anything in the suite has been able to assert on sound**, and it works for one
reason: the chip is emulated, so rendering is arithmetic rather than a device — the
player produces exactly the frames asked for, when asked, with no audio hardware and
no timing. Fifteen cases: the bank decodes and rejects rubbish, a note sounds, a
release decays to silence, a zero-velocity note-on releases (getting that wrong wedges
every voice on), stealing never exceeds 18, a doubled instrument takes two voices and
gives both back, percussion sounds, hard-panning is silent in the other channel, and
channel volume changes loudness.

They are **property checks, not a golden**. A golden over rendered samples would pin
Nuked-OPL3's exact output, which is upstream code this repository does not own and has
no business measuring.

Still with no gate at all: the mixer, the MUS reader, the device, the resampler and
the mix. Which is exactly how the two defects below survived.

Two engine defects this surfaced, both in `Host/Sound.cpp`, both previously
unreachable and so unfixed:

- **The MUS delay was a mis-parenthesised variable-length quantity** — the mask landed
  on the accumulator instead of the byte, truncating any rest longer than 127 ticks
  (0.9s). Corrected. It was documented at its site as preserved *because* nothing
  called it; the port calls it now.
- **Sound effects played at an eighth of their level.** The sound settings hold 0-15
  (the menu slider's range) and the mixer's volume tables are indexed 0-127.
  `setMusicVolume` already scaled by 8; `startSoundHost` did not. Fixed there rather
  than at the two sites carrying vanilla's own commented-out `/* *8 */`, because
  `soundSettings().sfxVolume` is one field doing two jobs and scaling it in place
  would put 120 into a slider that counts to 15. Measured: peak mix level went from
  ~2,500 to ~20,000 of 32,768 over the same attract demo.

Both were found the only way they could be: by instrumenting a running game and
reading the score out of the WAD. The same run measured the rest — the device opened
at 48 kHz, the queue held between 550 and 1920 frames against its 1920 target and
never emptied, sound effects peaked at 0.82 of full scale and the OPL at 0.28 (which
is where `musicGain` at 0.8 comes from, rather than from taste), MIDI ticked at
138/second against the nominal 140, and the message rate matched the score's own —
8.8/second against `D_E1M5`'s first-30-seconds density of 9.8.

That last figure is worth keeping. It looked like a bug for an afternoon: 8/second
against `D_E1M1`'s average of 60 is a sevenfold discrepancy, and the search went
through the tick clock, the drain loop and the MUS delay decode before the answer
turned out to be that **the attract demo plays E1M5, not E1M1**, and E1M5 opens on a
slow bass line at 9.8 messages a second. Measure the thing that is actually playing.

## Build

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug \
      -DCPM_eacp_SOURCE=$HOME/Code/eacp-puredoom
cmake --build build --target PureDoomEACP

./build/examples/EACP/PureDoomEACP.app/Contents/MacOS/PureDoomEACP
```

Targets: `doom-engine`, `PureDoomEACP`, `SimTests` and `PrimitiveTests`,
`record-goldens`, `port-bench` (the renderer's CPU cost — see **Measuring the
renderer**), `nuked-opl3` (the emulated OPL3 the music runs on, linked by the app
*and* by `SimTests`), and `doom-sim-probe` (the static library holding
`Tests/SimProbe.cpp`, which both test binaries link so the shim is compiled once).
Two build options: `PUREDOOM_BUILD_TESTS` and `PUREDOOM_BUILD_EACP_EXAMPLE`.

The tests need **no GPU**, so `-DPUREDOOM_BUILD_EACP_EXAMPLE=OFF` gives a fast loop.
CI does *not* use it: it builds every target, app included, because compiling the app
needs no GPU either — only running it does — and the app is otherwise ungated.
Both the tests and the engine link **`eacp-core`** for
platform work they would otherwise hand-roll per OS (today
`<eacp/Core/Utils/Environment.h>` — reading and writing an environment variable has
no portable spelling, and `std::getenv` is deprecated by Microsoft's CRT). With the
flag `OFF` the root `CMakeLists.txt` passes `EACP_BUILD_GRAPHICS OFF`, so eacp
compiles Core, SIMD and Network and stops. `doom-engine` links `eacp-core`
**PRIVATE**: `DOOM.h` stays a standard-library-only header with no eacp type in it.

**MakeASound** is fetched by `examples/EACP/CMakeLists.txt` rather than at the root,
because only the app needs it (see **Audio**). It brings miniaudio, RtMidi and Miro
with it; CPM dedupes by NAME, so its `EADataStructures` — reached through Miro — is
the one the root already added, and nothing is fetched twice.

eacp is fetched from GitHub via CPM. To co-develop against a local checkout, pass
`-DCPM_eacp_SOURCE=$HOME/Code/eacp-puredoom`. Use `$HOME`, not `~` — CMake does not
expand tildes, and a quoted `~/...` path silently configures against a non-existent
directory.

**`-DCPM_eacp_SOURCE` is not required**, and what makes that true has changed. It
used to be that everything the app needed was in eacp `main`. Today the app uses
`GPU::RenderPass::bind` (the gap log's **I1**), which is on eacp's `puredoom`
branch and not upstream — so the **root `CMakeLists.txt` fetches that branch**,
`GIT_TAG puredoom`, and says at the call so it goes back to `main` when `bind`
merges.

**Pinning the branch there rather than leaving it to the flag is the point.** A
developer with `-DCPM_eacp_SOURCE=$HOME/Code/eacp-puredoom` cannot tell the
difference either way; CI has no local checkout and would have failed every row
the moment the app was pushed. This repository's branch and eacp's move together
(`EACP_PLAN.md`), and the only place that can be *checked* is the build that
fetches rather than points.

Measured rather than assumed: a scratch tree configured with **no**
`CPM_eacp_SOURCE` at all — CI's own shape — builds every target including the app
and passes all 121 tests in `Release`.

`~/Code/eacp-puredoom` is still the checkout to co-develop in, and the flag is
still what to use for it (see `EACP_PLAN.md` for why it is a second checkout at
that path and not `~/Code/eacp`).

`doom-engine` and the tests were never affected either way — they link `eacp-core`
only, which is why the `-DPUREDOOM_BUILD_EACP_EXAMPLE=OFF` loop builds against any
checkout.

The app boots `doom1.wad` from the repository root by default: PureDOOM has no
`-iwad` argument — it locates WADs via `DOOMWADDIR` (falling back to the current
directory), so `main` points `DOOMWADDIR` at the repo root unless the user already
set it. Other classic DOOM arguments (`-warp`, `-skill`, `-episode`, …) pass
straight through.

## Testing

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug \
      -DCPM_eacp_SOURCE=$HOME/Code/eacp-puredoom
cmake --build build
ctest --test-dir build --output-on-failure
```

121 tests, roughly forty seconds. **Run it before and after anything you change in
`src/DOOM`.**

Two binaries, and which one a test lives in is not cosmetic. **`SimTests`** boots
the engine, and only it links `Tests/TestMain.cpp`, which points `DOOMWADDIR` at the
repository root — so **any test that boots belongs there**. **`PrimitiveTests`**
takes NanoTest's default `main` and covers what needs no boot. A booting test put in
`PrimitiveTests` finds the IWAD when you run the binary by hand from the repo root
and fails under ctest, which runs it from elsewhere; that reads as flakiness and is
not.

| File | Binary | What it holds |
|---|---|---|
| `Sim/DemoTests.cpp` | SimTests | the three attract demos, world + frame goldens |
| `Sim/ReplayTests.cpp` | SimTests | replay-twice and load-a-second-demo; the per-level reset |
| `Sim/LevelTests.cpp` | SimTests | the loaded geometry is well-formed (every cross-reference lands in its vector) |
| `Sim/WadTests.cpp` | SimTests | all 1,264 lumps against `doom1.lumps` |
| `Sim/MenuTests.cpp` `Sim/AutomapTests.cpp` `Sim/FinaleTests.cpp` | SimTests | the three screens no demo reaches — plus, in `AutomapTests`, the automap's vector shape tables, which its *frame* golden cannot reach (they are drawn only under IDDT) |
| `Sim/IntermissionTests.cpp` | SimTests | the fourth screen: the real E1M1 → scoreboard → E1M2 transition, its state machine and its frame golden |
| `Sim/ScenarioTests.cpp` | SimTests | place a mobj, move it, assert |
| `Sim/SaveGameTests.cpp` | SimTests | the save/load round trip, and `readFile`'s owner |
| `Sim/OwnershipTests.cpp` | SimTests | that destroying an `Engine` gives the memory back |
| `Sim/PrimitiveTests.cpp` `Sim/MathTests.cpp` `Sim/GeometryTests.cpp` | PrimitiveTests | the arithmetic underneath the simulation — including the endian swaps, which matter out of proportion to their size because `littleEndian()` is the identity on every machine this builds on |
| `Sim/CheatTests.cpp` | PrimitiveTests | the cheat-sequence matcher |
| `Sim/EngineTests.cpp` | PrimitiveTests | the composition root, and that `resetEngine` is genuine |
| `Sim/StateClusterTests.cpp` | PrimitiveTests | the `Engine`'s state clusters and accessor identity |
| `Port/GeometryTests.cpp` `Port/AutomapTests.cpp` `Port/WipeTests.cpp` | SimTests | the *port's* own decisions — `Engine::buildGeometry`, `Engine::buildAutomap`, and the screen melt's composite rule — driven headlessly. See below |
| `Port/GenmidiTests.cpp` `Port/OplTests.cpp` | SimTests | the IWAD's OPL instrument bank, and the synth over it — the only part of the audio path anything can measure. See **Audio** |

Run the binaries through ctest, not bare. NanoTest registers one ctest case per test
and re-runs the binary with `--test <name>`, so every test gets a fresh process —
which the engine needs. A bare `./SimTests` puts all of them in one process and only
the first can boot; it says so rather than quietly passing.

### The demo tests are the safety net

DOOM's simulation is exactly reproducible: fixed-point arithmetic, and a fixed
256-byte random table walked by an index. That is why demos work at all — a `.lmp`
is nothing but the player's input, one ticcmd per tic. Identical input against a
deterministic simulation must produce an identical world.

So a demo *is* the assertion. `Tests/Sim/DemoTests.cpp` replays the shareware WAD's
three attract demos (11,410 tics of real play: combat, damage, death, respawn,
doors, lifts, and a level's worth of monsters thinking), hashes the world after
every tic, and holds it against `Tests/Goldens/*.hashes`. On a mismatch it reports
**the first diverging tic**, with where the player was standing and what the random
index had reached.

It is extremely sharp. Changing `FRICTION` from `0xe800` to `0xe801` — one part in
59,392, invisible to any player — desyncs demo1 at tic 48.

`prndindex` is the canary. `P_Random` drives the simulation and `M_Random` does not,
and they keep separate indices for exactly that reason (the screen melt calls
`M_Random`, which is why a wipe cannot desync the game). Add, drop or reorder a
single `P_Random` call and everything after it shifts.

**When a change to behaviour was intended**, re-record with
`cmake --build build --target record-goldens`. The diff on `Tests/Goldens/` is then
the reviewable record. Re-recording to make a red suite go green is the one thing
that defeats the whole apparatus.

### The same replay watches the renderer

`D_DoomLoop` calls `D_Display` every tic, so the software renderer has been running
throughout the suite; now the replay hashes what it produces — `screens[0]`, the
finished 320x200 palette-indexed frame, together with the live palette (the damage,
pickup and invulnerability flashes are palette swaps). Every 4th tic, against
`Tests/Goldens/*.frames`.

They are **separate goldens on purpose**, and the failure says which moved. A
renderer refactor that desyncs the simulation is a very different bug from one that
merely draws it wrong. It is sharp: adding 1 to the light-level start map in
`R_SetViewSize` — one COLORMAP row — fails demo1 at tic 4 while the simulation
golden sails through.

**A second measurement of the same sharpness, worth knowing because it settles a
question that is otherwise pure reasoning.** `Doom::sortVisSprites` orders sprites
back-to-front, and vanilla selection-sorted with a strict `<`, so two sprites at
*equal* scale kept insertion order. Whether that stability is load-bearing depends
on whether equal fixed-point scales actually occur, which no amount of reading
answers. Substituting `std::sort` for `std::stable_sort` moves the **frame** golden
at demo2 tic 412 and demo3 tic 232 and leaves every simulation hash untouched — so
ties happen twice in three demos, the sort must stay stable, and the two golden
families separate exactly as designed.

Two things had to be true for it to work, and both were already true: **the melt
does not read the clock** (vanilla's `D_Display` busy-waits on `I_GetTime`;
PureDOOM's `D_UpdateWipe` advances one tic per call), and **the config is pinned**
(`M_LoadDefaults` reads the developer's real `~/.doomrc` unless given `-config`, and
`screenblocks` alone changes the shape of every frame — `SimProbe` boots against
`Tests/doom-tests.cfg`).

### The engine runs many scenarios per process

`Tests/Sim/ReplayTests.cpp` replays a demo a second time in one process (identical
tic for tic) and loads a *different* demo over the first. Together they prove the
per-level reset is clean — the thing that makes scenario tests possible and the only
test of `Doom::Level`'s reload path. `Tests/Sim/LevelTests.cpp` separately checks
that the geometry a loader wires together from raw WAD lump numbers is
well-formed after a load — every seg's vertexes/linedef/sidedef/sectors, every
subsector's seg range and sector, every sector's slice of the shared line buffer —
lands inside the vector it points into. (It used to check a *view* invariant, that
the loose `vertexes`/`numsegs`/… globals still equalled their `Level` vector's
`data()`/`size()`; those globals are gone, so that failure mode no longer exists.)

### What the tests do not cover

The engine's sound mixer and MUS reader, the port's `Audio` on top of them — the
device, the resampler, the mix — and `examples/EACP`'s GPU-bound half: `View`, the
shaders, the platform layer. **The music synth is no longer on this list**
(`Tests/Port/OplTests.cpp`), and the reason it could come off is worth generalising:
it renders into a caller's buffer on demand, so it needs no device and no clock.
What remains uncovered is what still insists on one. The stakes are higher than they
were, because this is live code now rather than dormant code: both defects listed
under **Audio** were found by instrumenting a running game and reading the score out
of the WAD, because nothing in the suite could have. That is the whole list — but it has been wrong twice, and the way it was wrong
each time is the lesson. **The cheat matcher** was live engine code with no gate over
it at all, and went unlisted because this section was organised around *screens* and
`checkCheat` is not a screen — so a category nobody had thought to name could not show
up as missing. **The port's automap transform** was worse: it was listed as uncovered,
correctly, and stayed broken for it (see `Tests/Port` below). **Before refactoring
anything, check what covers it by running the code, not by reading a list like this
one.**

The four screens a demo never reaches each have their own harness:

- **The menu** (`menu.frames`, via `Tests/MenuReplay.h`) — synthetic key events
  drive a scripted walk through the menus over the title screen, hashed every tic.
- **The automap** (`automap.frames`, via `Tests/AutomapReplay.h`) — loads E1M1
  directly, then walks the map (follow on and off, hand-panning, zoom, the big
  overview, the grid, marks), asserting the map actually opened at each transition
  rather than assuming a keypress landed.
- **The finale** (`finale.frames`, via `Tests/FinaleReplay.h`) — reached by calling
  `startFinale` after loading E1M8. Hashes the text crawl, the stage-transition wipe
  and the settled screen. The cast call and bunny scroll are DOOM II / episode-3
  only, so the test *asserts* the game mode is shareware.
- **The intermission** (`intermission.frames`, via `Tests/IntermissionReplay.h`) —
  unlike the other three it needs no direct entry-point call: `Doom::exitLevel()` is
  the real thing, so one script drives the genuine E1M1 → scoreboard → E1M2
  transition and **two tests read it differently** — `Sim/intermission` asserts the
  state machine and discards the frames, `Sim/intermissionFrames` holds them against
  the golden. That split is deliberate: a transition that stops happening is a
  different bug from a scoreboard that draws wrong, and only the second needs a
  golden to fail. The melts either side are warmed out unhashed, as the finale's
  entry wipe is; the 401 hashed tics are `drawIntermission`'s own output, including
  the last one, which draws *after* `endIntermission()` has unloaded.

  That last tic is where its first sanitizer run caught a real defect: `drawEL` read
  an `lnames` the unload had just cleared. Vanilla has the identical call order and
  survived it because `WI_unloadData` was `Z_ChangeTag(PU_CACHE)`, which left the
  memory readable. Fixed at `unloadIntermissionData`, and pinned twice.

  **The level clock is the only stat this harness can make non-zero**, and it is why
  the script spends 2,500 idle level tics before the exit. The player stands at
  E1M1's start, so kills, items and secrets all finish at 0 and their count-ups run
  without visibly rolling; the clock does not care what the player did. At 71 seconds
  against E1M1's 0:30 par, `sp_state` 8 counts both up three seconds a tic and
  `drawTime` draws a minutes digit. Ten tics — what the state test used before the
  golden existed — left the whole screen static but par, at 17 distinct frames out of
  401 rather than 30.

Each was demonstrated **sharp and non-redundant** when recorded: a one-palette-index
change to `WALLCOLORS` fails only `Sim/automap`, `TEXTSPEED` 3→4 fails only
`Sim/finale`, and moving `SP_STATSX` — the stats column's left edge — by one pixel
fails only `Sim/intermissionFrames`, each with the demo goldens green through it.
That matters — a golden recorded *after* a rewrite pins whatever the rewrite did, and
a golden that no plausible change would fail is worse than none, because it reads as
coverage.

The `-DPUREDOOM_BUILD_EACP_EXAMPLE=OFF` fast loop never builds the app target, so a
change there can break it with every test green. `EngineAccess.cpp` is the exception
and is covered — `SimTests` compiles it directly, see below — but `View.h`, the
shaders and the platform layer are not. Keep a second build directory with the app on
and treat its linking as a fourth gate — CI builds every row with the app on, so a
break there is caught on push, but not before it is pushed.

### `Tests/Port` covers the port's builders, and is why it exists

`EngineAccess.cpp` needs no GPU: it includes only the engine's headers and the
container vocabulary, and hands out plain data. So `Tests/CMakeLists.txt` compiles it
into `SimTests` — which makes an engine change that breaks it a build failure in the
ordinary suite, and lets `Engine::buildGeometry` and `Engine::buildAutomap` be driven
headlessly and their output inspected as data.

That coverage is not decorative. **Both port bugs found so far were invisible to
every other gate**, and for the same structural reason: the port re-implements the
engine's *decisions* as geometry, so the software renderer's frame goldens are green
whatever the port emits — they do not run a line of it.

- `Port/GeometryTests` grew out of the first Windows build's missing floors. It
  classifies triangles by their *vertices* rather than by texture id (a floor is the
  only horizontal triangle: walls are vertical quads and things are billboards), so
  it stays true through a rewrite of the emitter.

  `Port/spectresAreFuzzed` is the same test file's answer to the same problem one
  layer up: a spectre emitted as an ordinary sprite draws a perfectly solid demon on
  the GPU path and passes every other gate in the repository, the software renderer
  having fuzzed its own copy by hand in `drawFuzzColumn`. It spawns one into E1M1 —
  which holds none, so the counts move only for what was spawned — with an ordinary
  barrel as the control, and reads the split off the vertex counts. Demonstrated
  sharp: routing the spectre back to the textured runs fails it and nothing else.
- `Port/AutomapTests` grew out of the second, which is worth stating in full because
  it is the general hazard in its purest form. The map's transform read
  `((double) x1 - originX) * scale` with `x1` a raw `int` `fixed_t`. When `fixed_t`
  became a strong type, the sweep rewrote `(double) x1` as a whole-units conversion —
  correct in isolation, and it compiled clean — while `originX` beside it stayed in
  raw fixed-point. The two terms then disagreed by a factor of 65536, and the entire
  GPU automap collapsed to a point ninety pixels left of centre. That is exactly the
  lesson already recorded under **Read the warnings**, arriving without even a
  warning to ignore: *when a raw arithmetic type becomes a strong one, audit every
  site where the old type met a value of another kind, not only the ones that fail to
  compile.* Units are the ones that hide — nothing in the type system knows that one
  `double` is whole map units and the next is 65536ths of one. `Engine::Point` now
  says so in a comment at its declaration, and every `double` naming a map coordinate
  in that file is in whole units.

- `Port/WipeTests` is the third, and unlike the other two it was written before a
  bug rather than after one. The screen melt's composite is a *rule* the port and
  the engine both implement and neither shares — `doMelt` copies runs of rows into
  place as the columns move, and a fragment shader has to answer for one pixel with
  no memory of the last frame. `Port/meltCompositesLikeTheEngine` recomposites from
  what `Engine::buildWipe` and `Engine::wipeIncoming` hand over and holds it against
  `screens[0]`, every tic of a real melt, reached by loading E1M1 (a level load
  wipes exactly as any transition does).

  Its three vacuity guards are the interesting part: a melt between two *identical*
  screens composites correctly under any rule at all, so the test also asserts that
  the melt ran its full length, that the seam spent most of it inside the frame,
  and that the two screens differ over a quarter of it throughout. Demonstrated
  sharp three ways — the seam off by one (320 pixels disagree), the two screens
  swapped (63,878), and the negative-offset clamp dropped from `buildWipe`
  (37,513).

All three were demonstrated **sharp**, the same bar the frame goldens are held to,
and one of them failed the bar first: an earlier `automapSpansTheFrame` measured the
bounding box of the *whole* emitted map, which the arrow and the crosshair — drawn
from the camera and the frame, not from the map — held open on their own. It passed
with the bug reinstated. Measuring the walls alone fails by four orders of magnitude.
`Sim/automap`, the engine's own golden, passes with the bug in either way, which is
the non-redundancy demonstrated rather than assumed.

### `Sim/OwnershipTests.cpp` is the only test that can see a leak

The goldens hash the world and the picture, so memory that is never given back
changes nothing they measure until the process runs out. It installs a counting
`malloc`/`free` pair on `Doom::host()` and asserts that live blocks after
`resetEngine()` fall back to the post-boot figure — which is how the level pool's
missing destructor was found.

### The WAD directory has its own golden

`Tests/Sim/WadTests.cpp` walks all 1,264 lumps of `doom1.wad` and hashes each one's
bytes as `W_CacheLumpNum` hands them over, against `Tests/Goldens/doom1.lumps`. A
demo would notice a corrupt lump only as a desync at some tic with no explanation;
this names the lump.

### The primitive tests give locality

`Tests/Sim/PrimitiveTests.cpp` covers the arithmetic underneath the simulation. When
a demo desyncs at tic 48, these say *which primitive* stopped agreeing with itself.
Several pin things that look like bugs and are not — see rule 4 above — and two more
deserve their own note:

- **DOOM reads past the end of a lump — tutti-frutti — and it is preserved, not
  fixed.** A wall texture shorter than the column it fills makes the renderer draw
  whatever memory follows the patch. `WadFile::data` gives each lump a **256-byte**
  zero tail, so the over-read still happens but draws a deterministic zero
  everywhere. Do not "fix" the over-read in the renderer.

  **It was 64, and 64 was not enough — this is the worked example of why a bound
  should be read off the code rather than sampled.** `drawColumn` indexes `dc_source`
  with `frac.toInt() & 127`, so 128 is reachable *by inspection*, and
  `drawTranslatedColumn` does not mask at all (255). The measurement had been taken
  on macOS, where the bytes past the tail happened to be zero anyway — so the frame
  goldens recorded there were **correct**; they simply could not distinguish the
  guard from the allocator's luck. On Windows arm64 the same over-read found non-zero
  heap. Raising the tail to 256 made every test pass on both Windows toolchains
  **with no golden re-recorded**, which is the proof.

- **There are four side tests and they are four different formulae.** Three are in
  `Sim/MapGeometry.h` — `pointOnLineSide` shifts one factor of the cross product by
  `FRACBITS` and has no fast path; `pointOnDivlineSide` shifts *both* by 8 and has a
  sign-bit fast path; `pointOnPartitionSide` takes one of each (shift by `FRACBITS`,
  *and* the fast path) and is what the BSP descends by. The fourth, `Sim/Sight.cpp`'s
  `divlineSide`, returns 2 for "on" and multiplies as plain ints — and compares `x`
  against `origin.y` in one branch, which is vanilla's own typo and load-bearing.

  They answer the same for a point clearly off the line but not identically at the
  margins, and the collision/BSP/sight code depends on the specific one it calls.
  Merging any two desyncs the demos.

  What *was* merged, safely, is the pair that were already character-for-character
  identical: `R_PointOnSide` and `R_PointOnSegSide` were the same arithmetic over a
  node's partition and a seg's two endpoints. `Render/Main.cpp` keeps both names —
  they are the two ways of naming the line — and both forward to
  `pointOnPartitionSide`. Establish byte-identity before doing that again; three of
  the four look mergeable and are not.

Those are spot-checks, which is the right shape for a property and the wrong shape
for a transcription. So the tables are *also* checksummed whole — `finesine`,
`finetangent`, `tantoangle`, `rndtable`, `states[]` and `mobjinfo[]`, every entry. A
spot-check would happily pass over one mistyped digit in the middle of 16,000
numbers. A failure prints the new checksum, so you can see what you did and decide
whether you meant it. (`states[]` is hashed without its `action` pointer, which is a
function address and differs between builds.)

### Windows, and the four bugs it found

The engine builds and passes on **Windows arm64** under both **clang-cl** and
**MSVC**, in `Debug` and `Release`. Getting there was not a portability exercise: the
platform surfaced three genuine defects that macOS and Linux had been absorbing, and
**none was visible to any gate here**, because the goldens hash the world and the
picture and all three were correct right up until the heap gave out.

A fourth arrived later and is kept in its own section below, because it is a
different *kind* of defect and it walked through one more gate than these three
did — including the sanitizer this section recommends for exactly this situation.

- **A one-byte heap overflow, every boot.** `IdentifyVersion` sized seven WAD search
  paths by hand with the basename length written as a literal — and `"doomu.wad"` is
  nine characters counted as eight. On macOS the block's padding absorbed it; on
  Windows arm64 it corrupted the following block and demo1 and demo3 **segfaulted**.
  The literals are gone — `joinWadPath` measures the string it is about to write.
- **The tutti-frutti guard was too small** (above).
- **Three one-past-the-end `&arr[N]` subscripts** and one deliberate out-of-bounds
  write, all tripping MSVC's debug STL.

Two lessons generalise past Windows: **`Debug` and `Release` fail differently, and
`Debug` fails worse** (the debug CRT reports through a *modal dialog*, which under
ctest reaches no desktop, so the binary stops with no output, no exit code and no CPU
— flat CPU is the tell; `Tests/TestMain.cpp` now routes those reports to stderr); and
**run AddressSanitizer when a golden moves for no reason** (it found the overflow
immediately, with a stack, after an afternoon of reasoning had not). On Windows arm64
the LLVM toolchain ships no ASan runtime, but MSVC's does — build with
`cl /fsanitize=address` and `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL`.

`_WIN32` is the macro to test, never bare `WIN32`. `WIN32` is not a compiler macro at
all — it arrives from the Windows SDK or from a build system that adds `-DWIN32`,
which CMake happens to do for MSVC-style drivers.

### The fourth one: a lifetime bug, and what it walked through

**`Doom::host()` was being destroyed before the `Engine` whose destructor calls
it.** `Thinker::operator delete` goes through `host().free`, the `Engine` owns
every `Thinker`, and both were function-local statics — destroyed in reverse order
of construction, with the `Engine` constructed *first*. So every thinker in the
level was freed through a `std::function` whose lifetime had ended. `Host.cpp` now
never destroys it, and says at the site why an ordering fix would have been the
weaker answer.

The bug itself is ordinary. **How long it survived is the part to learn from: it
walked through four gates, each for a different reason, and one of them is the
tool this section tells you to reach for.**

- **The goldens could not see it.** They hash the world and the picture, and both
  were correct right up until `main` returned. Same blind spot as the three above,
  a different mechanism.
- **macOS could not see it.** The storage is still mapped and libc++ leaves the
  bytes alone, so the dead `std::function` still had a valid target and the call
  landed. It is UB that works.
- **AddressSanitizer could not see it** — and this is the one worth writing down,
  because the advice directly above is *"run AddressSanitizer when a golden moves
  for no reason."* That advice is still right; it is just not sufficient. **A
  lifetime violation is not a range violation.** Nothing read out of bounds, the
  object's storage was never unmapped, and all 120 tests passed clean under ASan
  with the bug live. When ASan comes back green and the failure is real, stop
  looking for a bad *index* and start looking at a bad *order* — of construction,
  of destruction, of who outlives whom.
- **CI saw it and nobody read it.** It had been failing every push for six days.

Three things about the *shape* of the failure did the actual work, and all three
generalise:

- **`0xC0000409` is not only a stack overrun.** `STATUS_STACK_BUFFER_OVERRUN` is
  also what the UCRT's `abort()` raises, which is where an unhandled
  `bad_function_call` ends up. Reading it as its name cost the first pass of the
  search, spent looking for a fixed-size buffer in the boot path.
- **The durations said "at exit".** Every crashing test ran for as long as its work
  takes — `Sim/demo1` for its full 1.65s replay, `Sim/wadDirectory` for 0.20s — and
  then died with no output, because `abort()` does not flush. A crash *at boot*
  would have taken the same few milliseconds in every case. That one reading turned
  the search around.
- **The survivors named the cause.** Of everything in `SimTests`, exactly
  `Port/genmidiRejectsRubbish` and `Port/oplRefusesABadBank` passed — the only two
  cases that never construct an `Engine`, and so never destroy one.
  `PrimitiveTests` is green for the same reason, and that had been read for days as
  "it boots nothing" without anyone asking what that implied. **When a suite fails
  in bulk, the passing tests carry more information than the failing ones.**

### Read the warnings — they are a fifth gate

The engine builds under `-Wall -Wextra -Wpedantic` with **zero warnings**. **Anything
at all is a regression.** That zero is measured on Apple Clang (`Debug` and
`Release`) and Windows arm64 with both clang-cl and MSVC — the configurations CI
builds. Real GCC and Ubuntu's gcc/clang are no longer a gate here (CI's matrix is
macOS and Windows only, and the local macOS GCC build is retired); `-Werror` waits on
that Linux work.

On Clang the zero includes **`-Wshorten-64-to-32`**, appended to `DOOM_WARNINGS_ON`.
The Xcode generator passes that flag on its own, which is how 15 `long`→`int`
narrowings sat visible in Xcode builds while every Ninja build read zero. Pinning it
makes the two generators agree on what zero means.

**The flags are chosen by the driver, not the compiler's name.** clang-cl's
`CMAKE_CXX_COMPILER_ID` is **`Clang`** with `CMAKE_CXX_COMPILER_FRONTEND_VARIANT`
**`MSVC`** — so selecting on the ID alone fed `-Wall` to an MSVC-style driver, where
**`-Wall` means `/Wall`, which clang implements as `-Weverything`**: ~44,000
warnings, and it silently dropped `-ffp-contract=off`, so the determinism the goldens
rest on was *not* in force on the one Windows toolchain most likely to contract. Ask
`CMAKE_CXX_COMPILER_FRONTEND_VARIANT`; clang-cl takes `/clang:-ffp-contract=off`.

**MSVC's `/W4` is not `-Wall -Wextra`, and six of its warnings are off on purpose**
(`/wd4244 /wd4267 /wd4459 /wd4457 /wd4702 /wd4805`). Each corresponds to a GCC/Clang
flag this project has deliberately not enabled, so leaving them on would make "zero
warnings" mean something different on each toolchain. 121 of the 136 were `C4244`,
DOOM's pervasive and load-bearing int→short/byte truncation. Raising that bar is a
real decision, and it should be taken for all compilers at once rather than arrived
at by accident on one.

**CI's five rows are five distinct configurations**, which they were not before: one
macOS universal build (AppleClang, `arm64;x86_64` — so the x86_64 half is compiled
though only the arm64 slice is run, the runner being Apple silicon and Rosetta not
installed on it), and Windows on **x64 and ARM64** under **both MSVC and clang-cl**.
All five are Ninja, `Release`, every target built and all 121 tests run. The earlier
matrix had a `macos-latest × gcc` row that was the clang row run twice: on a macOS
runner bare `gcc`/`g++` resolve to `/usr/bin`, which is Apple Clang wearing the name.

**They run on every branch**, not only `master`, and that was bought with six days
of red CI: the port work sat on a branch for fifteen commits and was never once
built on Windows or Linux, while a crash in every test that boots the engine failed
`master` 21 runs running. A branch nobody builds is a branch whose author is
guessing. The duplicate run a PR would otherwise cause is collapsed by a
`concurrency` group keyed on the branch name — which also cancels any run a newer
push has superseded, and at five configurations that is most of what would
otherwise be wasted.

Two Windows details the workflow depends on and states at its site: the MSVC command
line is set up by calling `vcvarsall.bat` (located through `vswhere`) and forwarding
`PATH`/`INCLUDE`/`LIB`/`LIBPATH` through `GITHUB_ENV`, since a step's environment
does not survive into the next; and clang-cl comes from the **standalone LLVM** on
the image rather than the Visual Studio generator's `ClangCL` toolset, which is a
separately installable VS component documented as present on the x64 image but not on
the ARM64 one.

The workflow has a **Report warning count** step that prints a per-configuration
count and fails on nothing.

Two things the first GCC build taught, neither visible from a Clang-only measurement:

- **A warning suppression is scoped to one compiler and spelled in its dialect**, so
  it fails silently in the direction that looks clean. Two generated tables carried
  `#pragma GCC diagnostic ignored "-Wwritable-strings"` — Clang's name for the flag.
  Clang went quiet; GCC did not recognise the option and then emitted **314** warnings
  the other compiler had never shown. Prefer fixing the type over naming the flag; if
  you must suppress, spell it per compiler and scope it tightly.
- **Nothing here is a C++20 module, but CMake scans for them anyway**, which puts
  `-fmodules-ts` on GCC's command line, makes `__has_feature(modules)` true, and
  leaves `rsize_t` undeclared in Apple's `<cstring>`. The root `CMakeLists.txt` sets
  `CMAKE_CXX_SCAN_FOR_MODULES OFF`.

**The goldens are compiler-independent**, checked rather than assumed: every test
passes built by GCC at `Release`. That is what `-ffp-contract=off` and the single
documented `double` in `fixedDivUnchecked` were supposed to buy. Also worth knowing: **CI
builds `Release` and the local instructions build `Debug`.** The goldens hold across
both, as they must, but run `Release` once before trusting a green `Debug` on
anything that touches optimisation-sensitive code.

This is not tidiness. The refactor's only real behaviour bug — `thintriangle_guy`,
the shape the automap draws every *thing* with, which silently collapsed to a point
when `fixed_t` became a strong type and `-.5 * FRACUNIT` started converting `-.5` to
`int` 0 — was named by the compiler in plain language in **every single build** and
went unread for months because 81 other warnings looked like scenery. The goldens
could not see it: the shape is drawn only under IDDT, which no demo and no test
script uses.

The general form: **when a raw arithmetic type becomes a strong one, the sites
needing an audit are not only the ones that fail to compile** — they are every site
where a *literal of another type* met the old one. Those compile, run, and warn in a
way that is easy to dismiss.

### Layers still to build

- **Scenario tests — started, not finished.** The pattern (load a level, place
  mobjs, run tics, assert) is live in `Tests/Sim/ScenarioTests.cpp`, with the probe
  surface it needs (`doomSimSpawnMobj`, `doomSimCheckPosition`, `doomSimTryMove`,
  `doomSimSetThingPosition`, `doomSimThingsInBlockOf`, …). Four cases run today.
  **Still uncovered**: `P_DamageMobj`, `P_CheckSight`, and the door and lift
  specials. Write them per-subsystem *as* you refactor it — they are how you get
  locality on code the demos only cover in aggregate.
- **Port-layer tests**: `View`'s tic/interpolation state machine is still not
  testable, because it lives inside a `GPU::GPUView` whose members construct GPU
  textures from `GPU::Device::shared()`. Extracting it into a plain GPU-free struct
  that `View` owns and delegates to is the prerequisite, and it is where the port's
  subtlest bugs have lived (the double-clock-read that drew frames a tic in the past,
  the five-tic input lag, mouse accumulate-and-flush).

## Measuring the renderer

```bash
cmake --build build --target port-bench && ./build/Tests/port-bench   # Release
```

`Tests/Bench/GeometryBench.cpp` builds a frame of world geometry three times per
tic through all three attract demos and reports what it cost. It is a **benchmark,
not a test** — ctest never runs it, it asserts nothing, and the output is a number
to read. It needs no GPU for the same reason `Tests/Port` does not, so the one part
of a real frame it cannot see is the upload.

**Build it in Release.** A Debug figure is a measurement of the standard library's
bounds checks.

Three things about it are worth keeping.

- **It carries its own correctness gate**, and that is what made acting on it safe.
  Every run hashes every vertex and every draw it emitted, so an optimisation is
  honest exactly when the hash is unchanged. Nothing else could say: the frame
  goldens run the software renderer, which never executes a line of the port, so
  "faster" and "quietly emitting less" would otherwise read identically. A benchmark
  wants a correctness gate as much as a test does.
- **It reports the walk separately from the stores.** Asking for a frame with a
  one-vertex buffer makes every run fail the layout's bounds check, so the emitter
  walks the whole world twice and writes none of it — which is how the double pass
  (count, then write) was priced without instrumenting the emitter at all. The
  stores are 7-10µs of it; the rest is the walk, done twice.
- **The app is the other half, and its absolute numbers are not to be trusted.**
  Everything in a frame scales together by up to 3x depending which core the
  display-link thread lands on, which is Apple silicon's scheduler rather than the
  renderer. The **proportions** hold: `buildGeometry` ~45-55% of `render()`'s CPU,
  submitting the per-texture draws ~26-31%, the upload 2.2-2.5%. The display holds
  120.5Hz without dropping a frame, and `render()` uses a few hundred microseconds
  of an 8.33ms refresh.

**What it found, and the rule it is a case of.** Just over half of `buildGeometry`
was the engine's *state accessors* rather than geometry: `Doom::level()`,
`Doom::graphicsData()`, `Doom::skyState()`, `Doom::playerState()` and
`Doom::lighting()` are out-of-line calls over a function-local static, each
reaching `Doom::engine()` and its guard, and `EngineAccess.cpp` can inline none of
them. The emitter asked per line, per side, per texture band, then walked the world
again — tens of thousands of calls a frame, **51.7% of the builder's disjoint self
time** against a fifth for the arithmetic they were fetching operands for. Hoisting
one reference per walk and passing it down cut `buildGeometry` by **43%** (80.9 →
45.7µs on demo1) with every geometry hash bit-identical.

That is the rule stated under **The `Engine` is the composition root** — hoist a
cluster's reference once per function rather than calling the accessor per access —
which had been written for the engine's per-pixel drawers and never applied to the
port. **It applies to anything that reaches engine state in a loop.** The port is
the worst case for it, being a separate translation unit where not one accessor can
be inlined.

Two things generalise past this measurement, both recorded in `EACP_PLAN.md`: the
item that pays is rarely the item on the list — three plan entries had been written
against a cost none of them had measured, and the real answer was in none of the
three — and the whole exercise found nothing a player could see, because the
renderer has 95% of its frame spare.

## Porting Rules

- eacp is never modified from this repository. When the port hits something eacp
  cannot do, implement a workaround here and record it in the gap log. eacp changes
  happen in the eacp repo and get picked up via `CPM_eacp_SOURCE`.
- **The engine is ours to change.** `src/DOOM` is edited directly. What holds you
  back is the engine's behaviour, which the demo tests pin exactly — the simulation
  *and* the frames drawn of it.

  The engine's headers are also the interface, and several things a renderer needs
  were `static` in a `.c` and only reachable because the single-header build made one
  translation unit of everything. Those are exported properly now
  (`UI/AutomapTypes.h` has the automap's state and shapes, `UI/Wipe.h` the melt's,
  `Render/Data.h` the texture composition types, `Sim/Random.h` the two random
  indices). Export the next one the same way rather than reaching around it.

  One older fix predates all this (`Game/Net.cpp`, `netUpdate`). PureDOOM runs with
  `singletics = true`, whose loop builds a tic's command and runs it in the same
  breath, advancing `maketic` and `gametic` together. But `netUpdate` is also called
  from `displayFrame` and `renderPlayerView` — vanilla called it there to keep the
  netcode fed while a slow frame rendered — and each of those advanced `maketic` with
  no `gametic` to match, so `maketic` climbed until it jammed against the
  `BACKUPTICS/2-1` cap and **every command was executed five tics (143ms) after it was
  built**. `netUpdate` now builds no command when `singletics` is set (it still drains
  events). This took the aim's input-to-screen lag from 163ms to 17ms.
- The engine is single-threaded: `Doom::initGame`, `Doom::updateGame`,
  `Doom::framebuffer` and all input calls happen on the main thread. **Audio is not
  an exception, and deliberately so.** `Doom::soundBuffer()` mixes on call, out of
  the same engine state `Doom::updateGame()` writes, so calling it from the device's
  callback would mean holding a lock across the mix — on the audio thread. This port
  pulls it from the main thread instead and hands finished frames across a wait-free
  queue, so there is no lock anywhere. See **Audio**.

### What the engine expects of its host

Two of these are not obvious, and getting either wrong makes the game feel broken
rather than fail outright.

- **Audio is two different shapes, on two different clocks.** Sound is a **pull**:
  `Doom::soundBuffer()` mixes the next 512 stereo frames at `Doom::DOOM_SAMPLERATE`
  (11025 Hz, 16-bit — 2,048 bytes) *on the call*, out of whatever the playsim has
  started. Music is a **push**: `Doom::tickMidi()` wants draining
  `Doom::DOOM_MIDI_RATE` (140) times a second and hands back MIDI messages, not
  samples, so it needs a synth on the other end. Resample if your device wants
  another rate; the engine produces the one. See **Audio** for how this port drives
  both.

- **The keys the app asks for do not stick by themselves.** DOOM cannot rebind a key
  from inside the game, yet it still writes every binding to `~/.doomrc` and, at
  startup, reads them back *over* whatever `Doom::setDefaultInt` asked for. A config
  left by an older build therefore pins that build's keys for good, and changing the
  binding in `Main.cpp` silently does nothing. `Main.cpp` calls `Engine::bindKeys()`
  after `Doom::initGame` to apply them again once the config has been read. What the
  player *can* change from the menu (mouse sensitivity, screen size, volumes) is left
  alone and still persists.

  Not every key can be bound. `HU_Responder` **eats** the key `HU_MSGREFRESH` sits on
  (Enter): `G_Responder` asks the HUD before it touches `gamekeydown` and returns the
  moment the HUD says it took the event, so Enter never reaches `gamekeydown`. `use`
  is therefore bound to vanilla's own Space.

- **Hand it the mouse once per tic, with the whole movement.** `G_Responder`
  *assigns* the mouse delta rather than adding to it, and `G_BuildTiccmd` consumes and
  zeroes it once a tic. Posting one `doom_mouse_move` per platform mouse event — which
  arrive several times per tic — throws away all but the last, and the aim crawls.
  Accumulate and flush once per tic (`View::flushMouse`). It also stops mouse motion
  from filling `D_PostEvent`'s 64-slot ring buffer, which silently overwrites rather
  than blocking, and so can swallow keystrokes.
- **The game only moves on a tic, 35 times a second.** The display refreshes two to
  four times as often. Step the engine when its own clock (`Engine::ticTime`) says a
  tic is due, and rebuild what derives from its state only then. Rendering still runs
  every refresh.
- **Do not draw the camera straight from the engine.** It would sit still for two or
  three frames and jump, which reads as lag however fast the frames arrive.
  `View::viewCamera` interpolates the position across the tic it is part-way through,
  and runs the *aim* ahead: the mouse movement gathered since the last tic is the turn
  the engine is about to make, so applying it now makes the view follow the mouse
  every frame with no lag.
- **Place everything between tics with the engine's clock, not the display's**
  (`Engine::ticTime`). A tic lasts 28.6ms and a frame 8.3ms, so a ramp paced by the
  display saturates early on some tics and is cut short on others.
- **Read that clock exactly once a frame**, and take both answers from the one
  reading: whether a tic is due, and how far into the tic the frame sits. Ask twice
  and a tic boundary can fall between the two asks, so the fraction wraps back to
  nothing while the state it is placed between is still the *previous* tic's. The
  frame is then drawn a whole tic in the past.
- **Everything that moves on the tic has to be placed between tics too**, or it
  jitters against a world that glides — and the engine keeps no previous state, so
  each is reconstructed differently:
  - the **heading** is split by where the turn came from (`View::viewAngle`;
    Shift+F7 drops back to plain interpolation to compare). What the *keyboard*
    turned is interpolated — a held key turns at a steady rate. What the *mouse*
    turned is applied at once, and the view then runs *ahead* by the movement the
    engine has not been handed yet. Interpolating the heading instead would cost a
    whole tic of lag on the one thing the hand is holding; GZDoom does exactly this.
    The mouse is *not* filtered, and must not be: what looked like noise in it was the
    system's pointer acceleration, and eacp now hands a locked window the raw device
    movement. Running ahead is safe because what it runs ahead on is the *accumulated*
    mouse, not the last delta. Measured against a deliberately ragged sweep, running
    ahead was far steadier than interpolating (frame-to-frame wobble 0.3ms against
    10.2ms).
  - **things** are wound back from where the tic left them by their own momentum,
    which the engine already stores.
  - **floors and ceilings** a door or lift is driving come from
    `Engine::snapshotTic`, taken before each tic runs. The walls that meet them read
    the same numbers, so nothing tears.
  - the **weapon** is interpolated from the previous tic's HUD sprites.
- **Billboards and the sky must be built around the camera being drawn from**, not
  the engine's. Built for the engine's heading, a sprite sits progressively edge-on as
  the view turns within a tic and visibly pulses; hence `Engine::buildGeometry` takes
  the view camera and the geometry is rebuilt per frame rather than per tic.

## eacp Gap Log

Found while porting, newest last. Remove entries once fixed in eacp.

Already merged to eacp `main` (the first batch this port surfaced):
`TextureFormat::R8Unorm`, so indexed data uploads as one byte per pixel;
`Buffer::update`, so the world's geometry buffer is re-uploaded rather than
reallocated; `ShaderProgram::setDiscardBelow`, an alpha test in the shader EDSL; and
three **input fixes** — `MouseEvent::rawDelta` (the *device's* movement alongside the
pointer's accelerated `delta`; the curve makes the same flick of the hand turn a
camera a different amount depending how fast it was made), the mouse lock's **cursor
warp** being reported as user motion (measured at −222 px in a single event), and
`GPUView::setFramesInFlight`.

Note on that last one: the two backends mean different things by it and **only DXGI's
is a latency knob**. On DXGI it is the depth of the present queue. On Metal it is
`maximumDrawableCount`: the size of the pool of buffers the layer hands out, *not* a
queue of finished frames. A display-link-driven view presents one frame per refresh
either way, so shrinking the pool dequeues nothing — it just means `nextDrawable` may
find no free buffer and block the calling thread, and that wait lands between sampling
the input and drawing with it. Lowering it to two therefore *raises* latency on Metal
(measured: sample-to-screen 23ms at three, 32ms at two). **This port should not lower
it.**

Merged since: **`WindowOptions::aspectRatio`**, the declarative window shape
constraint — honoured by macOS's native `setContentAspectRatio` and by a `WM_SIZING`
snap on Windows, so it anchors resize better than the callback did and also governs
zoom; and **the shader EDSL's full intrinsic set** — `floor fract abs min max clamp
step smoothstep mix sign fmod pow sqrt rsqrt exp log ceil round atan2 dot cross
normalize length distance reflect`, each taking a float literal in any argument
position, plus the `Int`/`Bool` vector families, statements (`var`, `select`,
`ifThen`, `loop`), `Array<T, N>`, and texel `fetch`.

**And merged since that**, the five this port built on the `puredoom` branch and
carried for a while as *answered but not shipped*: `TextureDescriptor::depth`
(entry 7), `RenderPass::setUniforms` (11), `RenderPipelineDescriptor::cullMode`
(8), `Graphics::primaryDisplay()` (4) and `View::getWindow()` (9). All five are in
eacp `main`, so **`-DCPM_eacp_SOURCE` is no longer needed to build the app** — see
**Build**, where that is measured rather than assumed.

Their entries below are marked **Closed** but kept rather than deleted, because
each carries a lesson that outlived the gap — the depth attachment's *"a passing
render test is not evidence the attachment happened"*, and cull mode's *"a
cross-backend convention cannot be established on one backend and inferred onto the
other"*, which Windows demonstrated by failing two `CullModeTests` cases on its
first run. `EACP_PLAN.md` Part 2 holds the full write-ups.

**Numbers are never reused**, so a hole in the sequence below is an entry that closed.

1. **No audio subsystem** — and eacp is not where this was answered. The output
   device comes from [MakeASound](https://github.com/eyalamirmusic/MakeASound), a CPM
   dependency of `examples/EACP`; the music is voiced by an emulated OPL3
   (Nuked-OPL3) out of the IWAD's own GENMIDI bank. Both sound and music play. See
   **Audio**.
2. **Modifier keys produce no key events**, on any platform — DOOM binds them as
   ordinary keys (Ctrl = fire, Shift = run, Alt = strafe). Workaround: they are diffed
   once per frame from polled `Window::getModifiers()` into synthetic down/up events.
   (The punctuation half of this entry is **fixed upstream**.)
2b. **`charactersIgnoringModifiers` is macOS-only**, and `characters` is filled on key
   *down* alone. This is the bug that made the first Windows build look like it had
   *partial* keyboard support: `Input.h` read the character first, so Space, Escape,
   Enter, Tab and the arrows worked while **every letter, digit and punctuation key
   returned `Key::Unknown`**.

   Workaround: `Input.h` maps the printable keys from the positional `KeyCode`, with
   the character kept only as a last resort. **Positional has to win even where both
   are available**, and the reason is key *up* rather than layout: with characters
   reported on key down alone, a character-first mapping identifies a key one way when
   pressed and another when released, so on any layout where those disagree the release
   never matches the press and the engine never clears the key.
3. **CPM consumers don't get app-bundle setup.** `eacp_default_setup()` only runs when
   eacp is the top-level project, so `set_default_target_setting()` on a consumer app
   target would stamp an empty Info.plist template. Workaround:
   `examples/EACP/CMakeLists.txt` sets the `EACP_MACOS_PLIST` cache variable itself.
4. **There was no display-metrics API.** **Closed** — merged into eacp `main`.

   Nothing public reported the screen's visible size, so an app could not pick an
   initial window size that fits the display, nor clamp or centre itself.
   `Graphics::primaryDisplay()` returns a frame, a **work area** (the frame less the
   menu bar and the Dock, or the taskbar) and a backing scale, all in **points** —
   the unit `WindowOptions::width` is already in, so a size read from it goes
   straight to a window.

   `Layout.h`'s `windowScale()` is what replaced the 3x guess: the largest whole
   multiple of 320x240 that fits 90% of the work area, capped at 4 and floored at 1.
   Whole multiples because a fractional one puts a texel grid on a pixel grid it does
   not divide into. On the machine this was built on it picks 3 — the same number the
   guess had, now derived rather than assumed, and 4 on a larger display.
7. **An offscreen pass had no depth attachment.** **Closed** — merged into eacp
   `main`.

   `TextureDescriptor::renderTarget` and `Frame::beginPass(target, …)` were real, so a
   pass could render into a texture a later pass sampled — but `Frame.h` said the limit
   outright: *"Multisampling and depth are deliberately absent."* The GPU world path
   sets `setDepth(true)` and depends on it, so **the world could not be rendered into a
   texture**, which is the one thing this port wants an offscreen target for. It blocked
   two things at once: **spectre fuzz** (B4), whose faithful implementation is
   world→texture then a fuzz pass sampling that texture at a jittered offset through
   COLORMAP row 6, and the **screen melt**, which composites an outgoing frame that
   stays a 320x200 software capture for the same reason.

   The fix is `TextureDescriptor::depth`, beside `renderTarget` and `computeWrite`: the
   buffer is created with the colour texture and dies with it, so a target stays one
   object with no second lifetime to keep in step, and every pass clears it to the far
   plane and stores nothing. `Texture::hasDepth()` is what a pipeline is built from.
   MSAA there stays absent and should — a texture target has nothing to resolve *into*.

   **What that work taught, and it generalises past eacp: on Apple silicon a depth test
   appears to work with no depth attachment at all.** The tile memory is there either
   way, so nulling the attachment left the new test green while Metal's validation layer
   reported `MTLDepthStencilDescriptor sets depth test but MTLRenderPassDescriptor has a
   nil depthAttachment texture` for every draw. A rendering test that passes is therefore
   *not* evidence the attachment happened — **run the GPU suite under `MTL_DEBUG_LAYER=1`
   and treat a silent validation layer as the other half of the measurement.** D3D12 has
   no such luck (`OMSetRenderTargets` with a null DSV genuinely disables the test), which
   is the second reason the Windows leg of CI is worth its cost.

   **Spectre fuzz is built on it and works** (see **Renderer status**), which is what
   turns the feature from plausible into measured. Two of its consequences were
   predicted here and one was not. The sample count *was* a consequence and cost
   nothing: this view has always been `setSampleCount(1)`, so the world's pipeline
   was already the single-sampled one a texture pass needs — it only had to name the
   target's pixel format as well. The unpredicted one is that the world moving into
   a texture makes the **melt's** remaining 320x200 capture a choice rather than a
   constraint.

   **That is done now, and the shape it took is not the one predicted here.** The
   outgoing frame could *not* be the world target: the target holds the 3D viewport
   in index space with no weapon, no status bar and no overlay, and — the part that
   settles it — a melt out of a level never reaches the GPU melt path at all,
   `gamestate` having already moved on to the intermission. What it wanted was the
   whole composited frame kept for a frame longer, which is a target of its own and
   one more pass. See **Renderer status**.
8. **There was no cull-mode state** in `RenderPipelineDescriptor`. **Closed** —
   merged into eacp `main`.

   `RenderPipelineDescriptor::cullMode` reaches Metal's encoder (culling being encoder
   state there, so it is set on *every* `setPipeline`, or a culled draw leaves its mode
   behind for the next one) and D3D12's rasterizer desc.

   **The winding turned out to be the whole of it, and it is worth knowing here
   because the same trap is one this port could have walked into.** Both backends
   default to "clockwise is front-facing" and mean different things by it: Metal
   decides facing in *clip* space — measured, not assumed — and D3D12 in *screen*
   space, one viewport y-flip apart, so left alone the two cull opposite faces of the
   same mesh. eacp now states the convention in the space a shader is written in
   (counter-clockwise in clip space, as glTF has it) and sets each backend to whatever
   produces it. That measurement corrected a first attempt that had reasoned it out
   and got it backwards.

   **Then Windows corrected the correction.** The Metal half was measured; the D3D12
   half was left as what its rasterizer rule *implies*, with `CullModeTests` there to
   say so if the implication was wrong. On its first Windows run it failed exactly
   two cases — `CullMode/backKeepsTheFrontFace` and `CullMode/frontKeepsTheBackFace`,
   on all four Windows rows — and the fix is upstream. **A cross-backend convention
   cannot be established on one backend and inferred onto the other**; that is the
   same lesson as the y-flip, one level up, and the only reason it cost a test run
   rather than an app finding its world inside out is that the gate was written
   before the answer was known.

   **This port does not enable it yet**, which is the honest position rather than a
   free win: `Engine::buildGeometry` emits walls from both sides of a linedef and
   floors from clipped subsector polygons, and nothing has ever measured whether their
   winding is consistent. A wrongly-wound triangle under culling does not draw wrongly,
   it does not draw at all — which is the Windows-missing-floors failure again, and
   `Tests/Port/GeometryTests` is where that measurement would belong.
9. **A `View` could not reach the `Window` it is in.** **Closed** — merged into
   eacp `main`.

   `View::getWindow()` returns the window or null. A pointer rather than the reference
   this port used to be handed, because a view can precede its window, outlive it or
   never have one; only the view a window adopts carries it, and everything under that
   walks up. `Window` owns the back-pointer as a member, so a view outliving its window
   reports none rather than a dangling one.

   `View` here no longer takes a `Graphics::Window&`, and `App`'s member order is now
   only an order rather than a constraint. The four call sites became `isAiming()` and
   one guarded block — the mouse lock and the polled modifier keys, which is entry 2's
   workaround and still needed.
10. **`-fno-gnu-unique` is added for every language.** eacp's top-level CMake adds it
    when the CXX compiler is GCC, but the option lands on all languages — including
    the OBJCXX its Apple platform files compile, and OBJCXX on macOS is always Apple
    Clang, which rejects the GCC-only flag as an unknown-argument *error*. A macOS GCC
    build then dies inside `eacp-core` before any engine code compiles. Workaround:
    the root `CMakeLists.txt` rewrites the flag to
    `$<$<COMPILE_LANGUAGE:CXX>:-fno-gnu-unique>` on every target under eacp's directory
    tree after `CPMAddPackage` — on the targets, not the directories, because a target
    snapshots its directory's `COMPILE_OPTIONS` at creation.
11. **eacp bound the uniform buffer to both stages.** **Closed** — merged into
    eacp `main`.

    `RenderPass::draw(program)` bound the block to both stages unconditionally, so
    every pass whose vertex or fragment function declares no uniform parameter drew
    an "unused binding" from Metal's validation layer — benign, but it is what
    Xcode's runtime-issues panel fills up with, and the validation layer's *silence*
    is this port's own measurement for a shader change.

    The answer was already computed and thrown away: the emitter decides per stage
    whether to declare the block at all. `vertexReadsUniforms`/`fragmentReadsUniforms`
    are public now, `GeneratedShader` carries both and `ShaderProgram` hands them on,
    so the bind and the signature it is aimed at come from one walk and cannot drift.
    `RenderPass::setUniforms(program)` is what `draw` calls — **and what app code
    hand-rolling a draw should call**, which is why this port's two hand-rolled draws
    (`View::drawGeometry`, the automap) did. Both have since stopped hand-rolling
    anything: `RenderPass::bind` calls `setUniforms` for them (**I1**), which is the
    same fix one level up — the list of what a draw binds now exists once.

    Two shaders here are the case it was written for: `FuzzShader` and
    `HudFuzzShader` write a constant colour, so their fragment stage declares no
    block and was being bound anyway, once per run, every frame.
12. **No texture arrays, and no atlas primitive.** There is no `Texture2DArray` and no
    array-slice binding anywhere in the GPU module. That is why the world is drawn as
    one draw per texture (`View::drawGeometry`), which is the largest draw count in the
    renderer, and why instancing the billboards would win CPU work rather than draws.
    With an array texture — or a sampler-visible atlas with a slice index per vertex —
    the whole level collapses into a single draw, and the group-by-texture bookkeeping
    in `Engine::buildGeometry` goes with it.

    **Measured** (see **Measuring the renderer**): 119 to 131 draws a frame across
    the three attract demos, costing 26-31% of `render()`'s CPU — the largest single
    item left, and still around 1% of a refresh. So the entry stands **for its shape
    rather than its speed**: the group-by-texture bookkeeping in `buildGeometry`
    goes with it. It used to lean on a second argument, that one draw is a far
    smaller ask of **I1** than 125 are — **I1** is answered now and `RenderPass::bind`
    serves 125 as readily as one, so that half has expired.
13. **`R8Unorm` is not a `PixelFormat`**, so a single-channel *render target* is not
    expressible: `PixelFormat` has BGRA8, RGBA8, RGBA16F and RGBA32F, while
    `TextureFormat` has had R8Unorm since this port asked for it.

    **The entry is worth less than it looked, and building spectre fuzz is what
    measured that.** It was written as the thing standing between this port and
    vanilla's own algorithm — render the post-COLORMAP *index* into an R8 target,
    remap it through row 6 as `R_DrawFuzzColumn` does, resolve the palette at the
    end — with RGBA8 as a fallback at four times the bandwidth. The port took the
    fallback and the algorithm is exact, because the format was never what made it
    faithful; keeping the frame in index space was, and RGBA8 round-trips an 8-bit
    unorm index exactly. What the extra channels then turned out to be *for* is the
    fuzz mask: a spectre has to raise a mark without disturbing the index beneath it,
    which is one more channel and an additive blend. So R8 would not have served this
    at all, and what it would save is two channels rather than three. It remains
    worth having for a single-channel target that genuinely wants one; it is not
    blocking anything here.

    Independently, and worth fixing either way:
    `pixelFormatFor(TextureFormat::R8Unorm)` falls through its `default:` and returns
    `PixelFormat::RGBA8Unorm` — a silent disagreement between a pipeline and its
    attachment, of exactly the kind that function's own comment says neither backend
    will accept. Unreachable today only because R8 cannot be a render target; it stops
    being unreachable the moment this entry lands.

### Interface findings

The same rule and the same log, but these are not missing features. eacp can do the
thing; the *shape* of the API made this port write something it should not have had
to. They are worth as much as any feature, and were recorded nowhere before.

I1. **A `ShaderProgram` owned its vertex buffer, so app-owned geometry fell off the
    supported path** — **answered**, on the `puredoom` branch, as
    `GPU::RenderPass::bind`.

    `draw(program)` assumed both halves of a draw belong to the program: its buffer
    and its whole vertex count. The world's geometry is a `Buffer` this port owns
    and updates in place, drawn as sub-ranges with a different texture per range, so
    it could supply neither — and `View::drawGeometry` inlined `draw(program)`'s body
    instead. **The copy had already fallen behind**, which is the part worth keeping:
    eacp grew `bindBuffers` and an indexed path, and the hand-rolled version had
    neither. A workaround that duplicates a body does not stay a workaround; it
    becomes a second implementation nobody is maintaining.

    `bind(program, vertices)` binds everything `draw(program)` binds and issues no
    draw; `draw(program, vertices, count, firstVertex)` is the one-shot form. Both
    halves of `draw(program)` now go through the same lines, so a seventh thing added
    there reaches app code too. `Tests/GPU/ExternalGeometryTests.cpp` pins it, and
    the `firstVertex` case had to be rewritten to be sharp — drawn from range 0, a
    `firstVertex` that never reached the draw call paints the same picture.

I2. **`bindTextures` is public only because `draw(program)` calls it** — **resolved,
    but not the way this entry expected, and the difference is the finding.**

    The prediction was that fixing I1 would hand it back to eacp. It does not: the
    port binds one texture *per run* over one buffer, so after a single `bind` the
    per-draw state is genuinely the caller's to restate. `bindTextures` is therefore
    app-facing on purpose now and says so at its declaration, rather than reading as
    an internal that leaked. **Splitting a convenience into state + draw does not
    make the state private again — it makes which half is whose explicit.**

I3. **A texture's sampling is fixed when the shader compiles.** Deliberate, documented,
    and with a Windows driver bug behind it (eacp's `SAMPLERS.md`), so this is a note
    rather than a request — but the consequence should be written where a reader meets
    it before hitting it: `WorldShader` has to set `texture.sampling` *before*
    `compile()`, and a program wanting one texture slot sampled two ways needs two
    programs.

I4. **`setFramesInFlight` means two different things** — the note above the list, which
    belongs at eacp's own declaration rather than here. One name, two meanings, and the
    wrong intuition is the natural one.

I5. **`prepare(int sampleCount, bool depth)` is a positional bool**, at the front door
    of every shader: this port writes `shader.prepare(sampleCount(), true)` and nothing
    at the call site says what the `true` is. A small descriptor, or a named enum,
    reads better — and would have somewhere obvious to put a depth *format* if entry
    7's offscreen depth ever needs one.

    **Rendering into a texture makes this sharper, because the tail of the parameter
    list is where a target's own answers live.** A shader drawing into the world
    target has to say sample count 1, depth, topology and pixel format, so the call
    becomes `prepare(1, true, GPU::PrimitiveTopology::Triangles, blend,
    GPU::pixelFormatFor(worldTargetFormat))` — five positional arguments of which
    three exist only to reach the fifth. The port hides it behind
    `View::prepareTargetShader`, which is the workaround and also the shape of the
    fix: the four that describe *where the draw lands* travel together and come from
    one place, so a `prepare(const Texture&, …)` overload — or a descriptor carrying
    them — would read as what it is.

    **Half-answered on the branch, by entry 8's work rather than on its own.**
    `ShaderProgram::prepare` takes a `RenderPipelineDescriptor` too now, so every
    field has a name at the call site — which is what let cull mode land at all,
    there being no sixth positional slot worth adding. The better half is still
    open: the descriptor is filled in by hand from a target the caller is holding,
    so `prepareTargetShader` still exists to do it.

## MakeASound Gap Log

The same rule as above: MakeASound is not modified from here. Found while wiring
audio, in the order the port hit them.

1. **No synth.** `MidiManager` carries MIDI faithfully to a port, but a port is not a
   sound. Worked around here rather than blocked on: DOOM's music was written for the
   Adlib, so this port voices it with its own OPL over Nuked-OPL3 and needs no bank
   shipped with it (see **Audio**). That is DOOM-specific and belongs where it is; the
   *general* gap is unchanged, and having built one, three things about its shape are
   now measured rather than guessed:

   - **The interface wants to be `render(Buffer&)` plus `send(MIDI::Event)`.** Both
     halves already exist in MakeASound's vocabulary, and `MidiBlockSync` already
     solves the input half — it assigns sample offsets, which is exactly what a
     synth's `send` needs and what `MusicDeviceMIDIEvent`'s last parameter takes.
   - **Rendering into the caller's buffer is what makes it testable**, and that turned
     out to matter more than anything else here: it is the *only* part of this port's
     audio path with a gate over it, because a synth that renders on demand needs no
     device and no timing (`Tests/Port/OplTests.cpp`, 10 cases).
   - **On Apple platforms it is nearly free.** `kAudioUnitSubType_MIDISynth` is a
     public C API with a General MIDI bank already on the machine, renders
     non-interleaved float — `Buffer`'s own layout — and compiles clean under
     `-Wall -Wextra -Wpedantic` with no Objective-C. Windows and Linux have no
     equivalent and need a bundled soft synth (TinySoundFont is MIT, single-header,
     no dependencies, and its `TSF_STEREO_UNWEAVED` mode is also planar).

2. **`getDefaultConfig()` always opens an input device**, so an output-only app gets a
   duplex stream and, on macOS/iOS, a **microphone permission prompt** it has no use
   for. There is no `getDefaultOutputConfig()`, and building the config by hand also
   means reimplementing the rate choice: `pickCompatibleSampleRate` takes two
   `DeviceInfo`s and has no output-only form. Workaround:
   `examples/EACP/Audio.cpp` builds the `StreamConfig` itself and picks the rate with
   a local `chooseSampleRate`.

3. **`SPSCQueue` does not report its occupancy.** `push` returning false is the only
   signal, and by then the producer has already generated the data it cannot place —
   which for a pull-model source like DOOM's mixer means the samples are *gone*, not
   merely delayed. A `size()`/`space()` on the producer side (a relaxed load of the
   two indices, no extra state) would let a producer decide *before* generating.
   Workaround: `Audio::queued`, an `std::atomic<int>` alongside the queue, which is a
   second copy of state the queue already holds.

4. **`openVirtualOutput` throws on Windows rather than reporting unsupported.** The
   platform capability is knowable in advance, so a `supportsVirtualPorts()` — or the
   call returning `bool` — would let a caller choose its fallback without using an
   exception for control flow. Workaround: `Audio::openMidi` catches and falls back to
   a real port.

5. **No typed decoder from raw MIDI bytes to `MIDI::Event`.** `MIDI::toBytes` exists
   for the send path, and `sendMessage(const std::uint8_t*, std::size_t)` takes raw
   bytes, but a caller holding a packed status/data triple — which is exactly what
   `Doom::tickMidi()` returns — has to know that program change and channel pressure
   are two bytes and everything else is three. A `MIDI::fromBytes` (or a
   `messageLength(status)`) would put that table in one place. Minor; the port spells
   it out at `Audio::sendMidi`.

## Code Style

Applies to everything here; there is no vendored code left.

Always use the most modern C++ and RAII practices. Use `auto` for variables whenever
possible; do not use `auto` for functions and member functions.

Don't use comments unless absolutely needed — use named functions to make code self
documenting. The exception is the class of comment this file is full of: a note
explaining why something non-obvious must stay as it is. Those earn their place.

Give `std::function` members a non-null default — a no-op lambda, or one returning an
empty value — so call sites invoke them directly without null checks.

Member variables use plain names (no trailing underscores); constructor parameters
that would shadow a member get a `ToUse` suffix. Pass by `const T&` whenever possible.

Use eacp's own containers as they are meant to be used. `Vector<T>` is deliberately
**`int`-indexed and `int`-sized** — call `resize`, `assign`, `size` and `operator[]`
on it directly and index it with plain `int`. Reaching through `getVector()` for the
underlying `std::vector`, or casting indices to `std::size_t`, is working against it.

Enforced via `.clang-format` (copied from eacp):
- Allman brace style
- 85 column limit
- 4-space indentation (no tabs)
- Pointer alignment: left (`int* ptr`)
- Break constructor initializers before comma

**Always run clang-format on edited files.**
