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

**The C++ refactor is finished.** `REFACTOR.md` records the end state, the short
list of what remains (audio, ~55 deliberately-dead macros, two unmeasured
toolchains before `-Werror`), the preserved 1993 defects, and the traps the work
turned up. Read it before a large change; read this file for how the code works
today.

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
   `FixedDiv2` goes through `double`; the trig tables are sampled at bucket
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

  Two carve-outs. **`DOOM.h` stays standard-library-only** and spells its argument
  vector `std::vector` — an embedder should not need eacp's containers to call
  `initGame`. And the ~16 declarations that sit at **`::` scope on purpose** (the
  ones `EngineAccess.cpp` reads by bare name — `gammatable`, `mapnames`,
  `chat_macros`, `player_names`, `sprnames`, `button_states`, …) are spelled
  `Doom::Array` / `Doom::Vector`, since the using-declarations are inside the
  namespace and do not reach them.

  **The eight subdirectories *are* the engine**, all real C++ in `namespace Doom`:

  | Directory | Files | What it is |
  |---|---|---|
  | `Sim/` | 73 | the whole playsim — `Mobj`, `Movement`, `MapAction`, `Enemy`, `Player`, `Weapon`, `Sight`, `Interaction`, the eight specials, `Thinker`, `Tick`, `Setup`, `SaveGame`, `Info`, plus `Random`/`Level`/`MapGeometry` |
  | `Game/` | 56 | game loop, netcode, config, args, sound dispatch, and most of the `Engine`'s state clusters |
  | `UI/` | 42 | menu, HUD, status bar, automap, intermission, finale, screen melt, cheats |
  | `Render/` | 37 | the software renderer, all eight units — `Main`, `BSP`, `Segs`, `Planes`, `Things`, `Draw`, `Data`, `Sky`, plus `Video` |
  | `Math/` | 12 | `Fixed`, `Angle`, `Trig`, `BBox`, `Vec2`, `Swap` |
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

**13 aliases survive on purpose**: the host callbacks (`doom_print`,
`doom_malloc`, …) are references onto `Doom::host()`, a deliberately separate
immortal singleton that must *not* be reset with a fresh Engine. The members are
`std::function`s constructed with working defaults (stdio, `gettimeofday`,
`eacp::getEnv` — `Host/Host.cpp`); an embedder overrides them by assigning
`Doom::host().print = …` directly. String-shaped hooks take `std::string_view`
(`getenv` returns `std::optional<std::string>`).

**What survives as loose names is data and views**, and it lives inside the
subdirectories: the pointer-and-count views (`vertexes`/`numsegs`/`sectors`/… onto
`Doom::Level`, defined in `Sim/Setup.cpp`; `textures`/`sprites` onto
`GraphicsData`; `finesine`/`finecosine` onto `Math/Trig`), each refreshed by its
loader after it fills the owning vector. The drawer function pointers
`colfunc`/`spanfunc` stay raw in `Render/Main` — they are the per-column inner
loop. The automap's vector shapes and the melt's state are deliberate exported
carve-outs (`UI/AutomapTypes.h`, `UI/Wipe.h`) that the eacp compositor reads by
name; `UI/AutomapTypes.h` is deliberately **all** at `::` scope, so keep it that
way.

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

### Types, containers and idiom

**The vanilla types and function names are gone.** All 107 types are PascalCase in
`namespace Doom` (`mobj_t`→`Mobj`, `line_t`→`Line`, `player_t`→`Player`, …), and
every call site calls the namespaced function (`Doom::drawPlanes`, `Doom::tryMove`,
`Doom::displayFrame`, `Doom::fatalError`, `Doom::cacheLumpNum`). No prefixed
spelling survives; do not go looking for one. Two families are pinned *by address*
and keep an adapter: the 75 state actions are `Doom::Actions::look`-style forwards
in `Sim/Actions.{h,cpp}` (because `Sim/Info.cpp`'s `states[]` stores them and every
entry needs one pointer shape), and the drawer function pointers stay raw.

**`fixed_t` and `angle_t` are aliases onto the strong types** — `using fixed_t =
Doom::Fixed;`, `using angle_t = Doom::Angle;` — not raw typedefs beside them.
`FixedMul`/`FixedDiv` survive as thin operator wrappers for readability.

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

The constant *macros* are essentially closed (629 → 309 across `src/DOOM`, of which
199 is `Game/StringsFrench.h`, which no build compiles). What remains is
deliberate: the ~55 dead-in-both-eras macros (see `REFACTOR.md` — **do not convert
them**), the string families that cannot leave the preprocessor because
adjacent-literal concatenation happens at translation phase 6 (`PRESSKEY`, `DOSY`,
`DEVDATA`, `DEVMAPS`), and the feature toggles read by `#ifdef` (`RANGECHECK`,
`Host/Platform.h`'s three).

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

**A pointer parameter that can never be null is a reference — 109 functions are.**
The whole specials family, the playsim core (`tryMove`, `checkPosition`,
`setMobjState`, `removeMobj`, `addThinker`, `touchSpecialThing`, `changeSector`,
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
| `Actions.cpp`'s 75 state actions, `callWeaponAction` | address-pinned by `states[]` |

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
  eacp platform layer and GPU renderer, and `EngineAccess.h/.cpp` is the plain-data
  snapshot interface to engine internals. `EngineAccess.cpp` is an ordinary
  translation unit that includes the engine's headers; nothing DOOM-typed leaks out
  through the `.h`.

  The six shaders share `DoomShader.h`: `DoomShader` resolves a palette index the
  way the software renderer does (index → COLORMAP row → palette), and
  `ScreenQuadShader` adds the screen-space quad the four full-frame passes draw.
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
- **GPU automap**: the map as geometry rather than a rasterized frame. What it
  draws and the colour it picks are `AM_Drawer`'s own choices, mirrored in
  `eacpDoomBuildAutomap`; only its Bresenham walk is replaced, by a quad per line
  the vertex shader widens. Two things vanilla's rasterizer cannot do come out of
  it: the lines keep their real endpoints instead of snapping to whole pixels, and
  the map is recentred on the *interpolated* view rather than the player's last
  tic, so it glides at the display's rate instead of crawling at 35Hz.

  **The one place the port departs from vanilla on purpose**
  (`eacpDoomRevealAutomap`). A wall is revealed as a *side effect of being drawn*:
  `R_StoreWallRange` sets `ML_MAPPED` as the software renderer lays it down. But
  `D_Display` skips `R_RenderPlayerView` entirely while the automap is up, so
  vanilla's map stops filling in the moment you look at it. Most source ports
  quietly fix this, and so does this one: the BSP is walked once a tic while the map
  is up. It stops there — the planes and sprites are never drawn, and
  `R_RenderPlayerView`'s four `NetUpdate` calls are not wanted, as they drain the
  event queue. The walls it *does* draw land in the frame the automap had just drawn
  itself into (the column drawers write through `ylookup`, aimed at `screens[0]`),
  so the map is drawn again afterwards to put it back.
- **Overlay** (`eacpDoomBuildOverlay`): the layers the engine draws over the view
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
- **Screen melt**: drawn over the GPU view, not instead of it, which keeps the
  level it is revealing at the window's resolution. It needs **no offscreen render
  target**: the melt only ever *reads* the outgoing frame, and what it composites is

      column c, row r = the outgoing frame's row (r - offset[c]) when
                        r >= offset[c], and the incoming frame's row r otherwise

  so "the incoming frame" is just the framebuffer left alone. Only the outgoing
  frame becomes a texture, and it is a 320x200 software frame whatever happens.

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
- Still missing (B4): spectre fuzz. Anything outside a level (title, intermission,
  finale) falls back to the software frame automatically, which is right — those
  screens *are* 320x200 — and the status bar is always composited from it.

## Build

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug -DCPM_eacp_SOURCE=$HOME/Code/eacp
cmake --build build --target PureDoomEACP

./build/examples/EACP/PureDoomEACP.app/Contents/MacOS/PureDoomEACP
```

Targets: `doom-engine`, `PureDoomEACP`, `SimTests` and `PrimitiveTests`,
`record-goldens`, and `doom-sim-probe` (the static library holding
`Tests/SimProbe.cpp`, which both test binaries link so the shim is compiled once).
Two build options: `PUREDOOM_BUILD_TESTS` and `PUREDOOM_BUILD_EACP_EXAMPLE`.

The tests need **no GPU**, so `-DPUREDOOM_BUILD_EACP_EXAMPLE=OFF` gives a fast loop,
and that is what CI builds. Both the tests and the engine link **`eacp-core`** for
platform work they would otherwise hand-roll per OS (today
`<eacp/Core/Utils/Environment.h>` — reading and writing an environment variable has
no portable spelling, and `std::getenv` is deprecated by Microsoft's CRT). With the
flag `OFF` the root `CMakeLists.txt` passes `EACP_BUILD_GRAPHICS OFF`, so eacp
compiles Core, SIMD and Network and stops. `doom-engine` links `eacp-core`
**PRIVATE**: `DOOM.h` stays a standard-library-only header with no eacp type in it.

eacp is fetched from GitHub via CPM. To co-develop against a local checkout, pass
`-DCPM_eacp_SOURCE=$HOME/Code/eacp`. Use `$HOME`, not `~` — CMake does not expand
tildes, and a quoted `~/...` path silently configures against a non-existent
directory.

**Build it with a second compiler before believing a warning count.** GCC catches
things Apple Clang does not:

```bash
brew install gcc   # /usr/bin/gcc on macOS is Apple Clang wearing a hat
cmake -G Ninja -B build-gcc -DCMAKE_BUILD_TYPE=Release \
      -DPUREDOOM_BUILD_EACP_EXAMPLE=OFF \
      -DCMAKE_C_COMPILER=$(brew --prefix)/bin/gcc-16 \
      -DCMAKE_CXX_COMPILER=$(brew --prefix)/bin/g++-16
cmake --build build-gcc && ctest --test-dir build-gcc
```

The GPU render paths need four eacp features this port surfaced
(`TextureFormat::R8Unorm`, `Buffer::update`, `ShaderProgram::setDiscardBelow`, and
the raw-mouse/warp input fixes). These have merged to eacp `main`, so the default
CPM fetch builds the app cleanly.

The app boots `doom1.wad` from the repository root by default: PureDOOM has no
`-iwad` argument — it locates WADs via `DOOMWADDIR` (falling back to the current
directory), so `main` points `DOOMWADDIR` at the repo root unless the user already
set it. Other classic DOOM arguments (`-warp`, `-skill`, `-episode`, …) pass
straight through.

## Testing

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug -DCPM_eacp_SOURCE=$HOME/Code/eacp
cmake --build build
ctest --test-dir build --output-on-failure
```

98 tests, roughly thirty seconds. **Run it before and after anything you change in
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
| `Sim/LevelTests.cpp` | SimTests | the geometry-view invariant after a load |
| `Sim/WadTests.cpp` | SimTests | all 1,264 lumps against `doom1.lumps` |
| `Sim/MenuTests.cpp` `Sim/AutomapTests.cpp` `Sim/FinaleTests.cpp` | SimTests | the three screens no demo reaches — plus, in `AutomapTests`, the automap's vector shape tables, which its *frame* golden cannot reach (they are drawn only under IDDT) |
| `Sim/IntermissionTests.cpp` | SimTests | the fourth screen: the real E1M1 → scoreboard → E1M2 transition |
| `Sim/ScenarioTests.cpp` | SimTests | place a mobj, move it, assert |
| `Sim/SaveGameTests.cpp` | SimTests | the save/load round trip, and `readFile`'s owner |
| `Sim/OwnershipTests.cpp` | SimTests | that destroying an `Engine` gives the memory back |
| `Sim/PrimitiveTests.cpp` `Sim/MathTests.cpp` `Sim/GeometryTests.cpp` | PrimitiveTests | the arithmetic underneath the simulation — including the endian swaps, which matter out of proportion to their size because `littleEndian()` is the identity on every machine this builds on |
| `Sim/CheatTests.cpp` | PrimitiveTests | the cheat-sequence matcher |
| `Sim/EngineTests.cpp` | PrimitiveTests | the composition root, and that `resetEngine` is genuine |
| `Sim/StateClusterTests.cpp` | PrimitiveTests | the `Engine`'s state clusters and accessor identity |

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
test of `Doom::Level`'s reload path. `Tests/Sim/LevelTests.cpp` separately checks the
view invariant after a load: that `vertexes`/`numsegs`/… still equal their `Level`
vector's `data()`/`size()`.

### What the tests do not cover

Audio, and the eacp platform layer and GPU renderer. That is the whole list — but it
was wrong until recently, and the way it was wrong is the lesson. **The cheat
matcher** was live engine code with no gate over it at all, and went unlisted because
this section was organised around *screens* and `checkCheat` is not a screen — so a
category nobody had thought to name could not show up as missing. **Before
refactoring anything, check what covers it by running the code, not by reading a list
like this one.**

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
- **The intermission** (`Tests/Sim/IntermissionTests.cpp`) — unlike the other three
  it needs no direct entry-point call: `Doom::exitLevel()` is the real thing, so it
  drives the genuine E1M1 → scoreboard → E1M2 transition and asserts the state
  machine. Its first sanitizer run caught a real defect: the intermission's last tic
  draws *after* `endIntermission()` has run, so `drawEL` read an `lnames` the unload
  had just cleared. Vanilla has the identical call order and survived it because
  `WI_unloadData` was `Z_ChangeTag(PU_CACHE)`, which left the memory readable. Fixed
  at `unloadIntermissionData`, and pinned twice. **No frame golden yet.**

Each was demonstrated **sharp and non-redundant** when recorded: a one-palette-index
change to `WALLCOLORS` fails only `Sim/automap`, and `TEXTSPEED` 3→4 fails only
`Sim/finale`, with the demo goldens green through both. That matters — a golden
recorded *after* a rewrite pins whatever the rewrite did, and a golden that no
plausible change would fail is worse than none, because it reads as coverage.

The `-DPUREDOOM_BUILD_EACP_EXAMPLE=OFF` fast loop has its own blind spot: it never
compiles `examples/EACP`, and `EngineAccess.cpp` includes engine headers directly, so
an engine change can break the app with every test green. Keep a second build
directory for it and treat the app linking as a fourth gate.

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

- **`pointOnLineSide` and `pointOnDivlineSide` are different formulae** (both in
  `Sim/MapGeometry.h`) and must stay different. The line version shifts one factor of
  the cross product by `FRACBITS`; the divline version shifts both by 8 and has a
  sign-bit fast path. They answer the same for a point clearly off the line but not
  identically at the margins, and the collision/BSP/sight code depends on the specific
  one it calls. Merging them desyncs the demos.

Those are spot-checks, which is the right shape for a property and the wrong shape
for a transcription. So the tables are *also* checksummed whole — `finesine`,
`finetangent`, `tantoangle`, `rndtable`, `states[]` and `mobjinfo[]`, every entry. A
spot-check would happily pass over one mistyped digit in the middle of 16,000
numbers. A failure prints the new checksum, so you can see what you did and decide
whether you meant it. (`states[]` is hashed without its `action` pointer, which is a
function address and differs between builds.)

### Windows, and the three memory bugs it found

The engine builds and passes on **Windows arm64** under both **clang-cl** and
**MSVC**, in `Debug` and `Release`. Getting there was not a portability exercise: the
platform surfaced three genuine defects that macOS and Linux had been absorbing, and
**none was visible to any gate here**, because the goldens hash the world and the
picture and all three were correct right up until the heap gave out.

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

### Read the warnings — they are a fifth gate

The engine builds under `-Wall -Wextra -Wpedantic` with **zero warnings**. **Anything
at all is a regression.** That zero is measured on Apple Clang (`Debug` and
`Release`), real GCC 16 (`Release`), and Windows arm64 with both clang-cl and MSVC.
Only Ubuntu's gcc/clang remain; `-Werror` waits on them.

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

**CI's five rows are four toolchains.** On a macOS runner bare `gcc`/`g++` resolve to
`/usr/bin`, which is Apple Clang wearing the name — so the `macos-latest × gcc` row
is the clang row run twice. The workflow has a **Report warning count** step that
prints a per-configuration count and fails on nothing.

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
documented `double` in `FixedDiv2` were supposed to buy. Also worth knowing: **CI
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
  `Doom::framebuffer` and all input calls happen on the main thread. Audio, once
  wired, is the only exception — it is pulled from the audio callback, on another
  thread, and must take a lock against `Doom::updateGame`.

### What the engine expects of its host

Two of these are not obvious, and getting either wrong makes the game feel broken
rather than fail outright.

- **Audio, when it is wired** (nothing here plays a sound yet — gap-log item 1). This
  is what the deleted SDL example demonstrated and nothing else records. Sound is a
  **pull** model: run an output stream at `Doom::DOOM_SAMPLERATE` (11025 Hz), 16-bit
  stereo, 512 samples — 2,048 bytes a buffer — and call `Doom::soundBuffer()` from the
  audio callback, taking the engine lock around it. Music is a **push** model: a
  140 Hz timer (`Doom::DOOM_MIDI_RATE`) draining `Doom::tickMidi()` into a synth for
  as long as it keeps returning messages. Resample if your device wants another rate;
  the engine only produces that one.

- **The keys the app asks for do not stick by themselves.** DOOM cannot rebind a key
  from inside the game, yet it still writes every binding to `~/.doomrc` and, at
  startup, reads them back *over* whatever `Doom::setDefaultInt` asked for. A config
  left by an older build therefore pins that build's keys for good, and changing the
  binding in `Main.cpp` silently does nothing. `Main.cpp` calls `eacpDoomBindKeys()`
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
  four times as often. Step the engine when its own clock (`eacpDoomTicTime`) says a
  tic is due, and rebuild what derives from its state only then. Rendering still runs
  every refresh.
- **Do not draw the camera straight from the engine.** It would sit still for two or
  three frames and jump, which reads as lag however fast the frames arrive.
  `View::viewCamera` interpolates the position across the tic it is part-way through,
  and runs the *aim* ahead: the mouse movement gathered since the last tic is the turn
  the engine is about to make, so applying it now makes the view follow the mouse
  every frame with no lag.
- **Place everything between tics with the engine's clock, not the display's**
  (`eacpDoomTicTime`). A tic lasts 28.6ms and a frame 8.3ms, so a ramp paced by the
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
    `eacpDoomSnapshotTic`, taken before each tic runs. The walls that meet them read
    the same numbers, so nothing tears.
  - the **weapon** is interpolated from the previous tic's HUD sprites.
- **Billboards and the sky must be built around the camera being drawn from**, not
  the engine's. Built for the engine's heading, a sprite sits progressively edge-on as
  the view turns within a tic and visibly pulses; hence `eacpDoomBuildGeometry` takes
  the view camera and the geometry is rebuilt per frame rather than per tic.

## eacp Gap Log

Found while porting, newest last. Remove entries once fixed in eacp.

Already merged to eacp `main` (all gaps this port surfaced):
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

1. **No audio subsystem.** Sound effects need a pull-model PCM output stream
   (`DOOM_SAMPLERATE` = 11025 Hz, 16-bit stereo, mixed via `Doom::soundBuffer()`);
   music needs a 140 Hz (`Doom::DOOM_MIDI_RATE`) tick draining `Doom::tickMidi()` into
   a synth. Both are unwired; the game runs silent.
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
4. **No display-metrics API.** Nothing public reports the screen's visible size, so an
   app cannot pick an initial window size that fits the display, nor clamp/center
   itself. Workaround: a conservative 3x default plus a resizable window with
   letterboxed rendering.
5. **No declarative window aspect-ratio constraint.** `WindowOptions::onWillResize`
   works for keeping a window 4:3, but macOS has a native
   `NSWindow.contentAspectRatio` that anchors resize better and also governs zoom.
6. **The shader EDSL has almost no scalar maths.** `sin`/`cos` exist but there is no
   `floor`, `fract`, `abs`, `min`/`max`/`clamp`, `step` or `mix` for `Float`. B2 dodged
   this by letting the samplers do the work — `Repeat` tiles wall textures instead of
   `fract`, `Nearest` rounds the COLORMAP row instead of `floor`, `Clamp` bounds it —
   but any shader wanting real arithmetic will hit this.
7. **No offscreen render targets.** `Frame` only ever renders into the view's
   drawable, so a pass cannot render into a texture and sample it later. Any
   post-processing pass — a CRT/scanline filter — needs exactly that. (DOOM's melt
   turned out not to; see **Renderer status**.)
8. **No cull-mode state** in `RenderPipelineDescriptor`. Not blocking (DOOM's walls
   are fine drawn double-sided), but every triangle is rasterised from both faces.
9. **A `View` cannot reach the `Window` it is in.** Anything a view needs from its
   window — the mouse lock, the modifier keys — has to be handed to it by the app.
   This port declares the window *before* the view and hands it over as a
   `Graphics::Window&` at construction, which makes it impossible to be null. That is
   a workaround, not a fix: it constrains member order in `App`. A `View::getWindow()`,
   or a window reference given on `setContentView`, would settle it.
10. **`-fno-gnu-unique` is added for every language.** eacp's top-level CMake adds it
    when the CXX compiler is GCC, but the option lands on all languages — including
    the OBJCXX its Apple platform files compile, and OBJCXX on macOS is always Apple
    Clang, which rejects the GCC-only flag as an unknown-argument *error*. A macOS GCC
    build then dies inside `eacp-core` before any engine code compiles. Workaround:
    the root `CMakeLists.txt` rewrites the flag to
    `$<$<COMPILE_LANGUAGE:CXX>:-fno-gnu-unique>` on every target under eacp's directory
    tree after `CPMAddPackage` — on the targets, not the directories, because a target
    snapshots its directory's `COMPILE_OPTIONS` at creation.
11. **eacp binds the uniform buffer to both stages**, so Metal's validation layer logs
    an "unused binding" warning for every pass whose vertex or fragment function
    declares no uniform parameter — benign, but it is what Xcode's runtime-issues panel
    fills up with. The emitter already computes `vertexUsesUniforms(graph)`, so gating
    the bind on it would settle it.

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
