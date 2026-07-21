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

**That end state has largely arrived.** It was stated as: no C left, no
`PureDOOM.h`, no zone allocator, no ~684 loose globals, and an `Engine` object
constructed rather than booted. All five are true today — Steps 0–8 are done and
`Engine/resetEngineMakesAFreshInstance` proves the last one. **Step 9's three
strands are now done too** — the idiom cleanup (including the C-array conversion,
which this file and `REFACTOR.md` both called finished twice before it was ever
counted), the alias retirement, and the RAII sweep, that last one bar a single
audio-blocked owner.

What is left of the whole refactor is a short, named, mostly *deliberate* tail:
audio (blocked outside this repository); the
~55 dead-in-both-eras macros whose deletion is a judgement call reserved for a
human; and measuring the warning count on the **one** CI configuration still
unmeasured (Ubuntu's gcc/clang), which is the prerequisite for `-Werror`. Windows
is now measured on both toolchains — see **Windows** below, because getting there
turned up three real memory defects and none of them was a portability nicety.
The runtime-accessor macros are converted — and the list of "six" this
paragraph used to name was wrong in both directions: two of them were dead in both
eras and belong to the ~55 pile, and one that had to be converted was missing from
it. Four were live and are now functions.

Two defects found by the first GCC build are **preserved and documented at their
sites, not fixed**, because both are behaviour changes no gate here can check:
`Host/Sound.cpp`'s MUS delay decode has an operator-precedence bug that truncates
any multi-byte delay, and `UI/Intermission.cpp`'s `drawAnimatedBack` tests the enum
*constant* `commercial` rather than comparing against it, so the intermission's
animated background has never drawn in this lineage. Both are in the 1993-lineage
source too. Fixing either is a deliberate behaviour change for a human to make.

Read `REFACTOR.md`'s progress table for the current state rather than assuming from
this paragraph — and if the two ever disagree, the table is the one that gets
updated as work lands.

(Scenario tests, once thought to depend on the constructible `Engine`, never did:
the engine runs many scenarios per process because loading a level resets the
simulation cleanly — see `Tests/Sim/ReplayTests.cpp`. They exist now, in
`Tests/Sim/ScenarioTests.cpp`. The `Engine` object is what retired the loose
globals, not what unblocked the tests.)

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
3. ~~A file leaves the engine's blanket `-w` and comes under `clang-format` the
   moment it is genuinely rewritten, and not before.~~ **Spent — every file is on
   the far side of it.** The rule governed a boundary that no longer has anything
   on the vanilla side: nothing is exempt from `-Wall -Wextra -Wpedantic` or from
   clang-format, and no vanilla filename survives to be diffed against the 1993
   source. New code is written to the strict flags without asking.
4. Some things that look like bugs are load-bearing and must survive: `FixedDiv2`
   goes through `double`; the trig tables are sampled at bucket centres;
   `slopeDiv` gives up under 512; `pointToAngle2` lands one unit below north; and
   `BBox::add` is `else if`, not an independent min and max.
   `Tests/Sim/PrimitiveTests.cpp` and `Tests/Sim/MathTests.cpp` pin these on
   purpose — a refactor will want to "fix" all of them, and each would desync
   every demo ever recorded.

## Layout

- `src/DOOM/` — **the engine, and the code we own.** Built as the `doom-engine`
  static library, which both the app and the tests link, so a change to the
  simulation reaches both and neither can run code the other is not.

  It is **C++20 as of Step 2 of `REFACTOR.md`**, and there is no C left anywhere
  in the repository. **There is no flat layer left either**: `src/DOOM` holds
  exactly three files at top level — `DOOM.h`, the public interface an embedder
  includes (a plain C++ header in `namespace Doom` since the string refactor —
  the `extern "C"` block is gone); `doomtype.h`, the `byte` foundation every
  layer including `Math/` depends on; and `Containers.h`, the container
  vocabulary described below. Zero `.cpp` files. The last one, `info.cpp`,
  became `Sim/Info.cpp`.

  **The containers are unqualified `Doom::` names.** `Containers.h` mirrors
  eacp's own `<eacp/Core/Utils/Containers.h>` — it pulls in the four
  `ea_data_structures` headers and re-exports `Array`, `Vector`, `OwnedVector`,
  `OwningPointer` and `makeOwned` into `namespace Doom`, so a signature reads
  `Vector<T>&`. Include it rather than the individual headers; every file that
  used to name one of those does. It is flat because it is a foundation of the
  same kind as `doomtype.h`, and `CMakeLists.txt` moves it off the "vanilla"
  glob by hand so the two lists keep meaning what their names say.

  Two carve-outs. **`DOOM.h` stays standard-library-only** and spells its
  argument vector `std::vector` — an embedder should not need eacp's containers
  to call `initGame`. And the ~16 declarations that sit at **`::` scope on
  purpose** (the ones `EngineAccess.cpp` and the `extern "C"` block read by bare
  name — `gammatable`, `mapnames`, `chat_macros`, `player_names`, `sprnames`,
  `button_states`, …) are spelled `Doom::Array` / `Doom::Vector`, since the
  using-declarations are inside the namespace and do not reach them.

  **The eight subdirectories *are* the engine**, all real C++ in `namespace Doom`:

  | Directory | Files | What it is |
  |---|---|---|
  | `Sim/` | 73 | the whole playsim — `Mobj`, `Movement`, `MapAction`, `Enemy`, `Player`, `Weapon`, `Sight`, `Interaction`, the eight specials, `Thinker`, `Tick`, `Setup`, `SaveGame`, `Info`, plus `Random`/`Level`/`MapGeometry` |
  | `Game/` | 56 | game loop, netcode, config, args, sound dispatch, and most of the `Engine`'s state clusters |
  | `UI/` | 42 | menu, HUD, status bar, automap, intermission, finale, screen melt, cheats |
  | `Render/` | 37 | the software renderer, all eight units — `Main`, `BSP`, `Segs`, `Planes`, `Things`, `Draw`, `Data`, `Sky`, plus `Video` |
  | `Math/` | 12 | `Fixed`, `Angle`, `Trig`, `BBox`, `Vec2`, `Swap` |
  | `Host/` | 12 | the platform boundary — `Video`, `System`, `Sound`, `Net`, `Api` (the public C API), `Host` |
  | `Wad/` | 3 | `WadFile` |
  | `Engine/` | 2 | `Engine`, the composition root |

  `src/DOOM/CMakeLists.txt` still splits compile flags between a "vanilla" bucket
  (blanket `-w`, formatting exemption) and a "rewritten" one (`-Wall -Wextra
  -Wpedantic`, clang-format from the first line). That split has now *finished
  moving*: the vanilla glob matches only those two top-level headers and no
  translation unit at all, so in practice everything compiled is under the strict
  flags. Keep the machinery — it costs nothing and documents the rule — but do not
  read it as evidence that unrewritten code remains.

  **The vanilla *function* API is gone.** `R_DrawPlanes`, `P_TryMove`, `D_Display`,
  `I_Error`, `W_CacheLumpNum`, `EV_DoDoor`, `HUlib_drawSText` and the other ~440
  prefixed names were retired: every call site calls the namespaced function
  (`Doom::drawPlanes`, `Doom::tryMove`, `Doom::displayFrame`, `Doom::fatalError`,
  `Doom::cacheLumpNum`, …). Retiring the shims was only half of it: 134 of the
  namespaced *targets* still carried the prefix as an initialism, and those were
  renamed again — `Doom::dDisplay` → `Doom::displayFrame`, `Doom::amTicker` →
  `Doom::automapTicker`, `Doom::I_Error` → `Doom::fatalError`. None of the middle
  spellings survive; do not go looking for them.
  Two families are pinned *by address* and therefore keep an adapter, though not a
  prefixed one: the 75 state actions are `Doom::Actions::look`-style forwards in
  `Sim/Actions.{h,cpp}` (because `Sim/Info.cpp`'s `states[]` stores them and every
  entry needs one pointer shape), and the drawer function pointers
  (`colfunc`/`spanfunc`) stay raw pointers because they are the per-column inner loop.

  **The vanilla *types* are gone too** — all 107 are PascalCase in `namespace Doom`:
  `mobj_t`→`Doom::Mobj`, `line_t`→`Doom::Line`, `sector_t`→`Doom::Sector`,
  `player_t`→`Doom::Player`, `vldoor_t`→`Doom::Door`, and so on. The last three
  exceptions (`doom_key_t`, `doom_button_t`, `doom_seek_t`) went with the
  `extern "C"` API: they are `Doom::Key`, `Doom::MouseButton` and
  `Doom::SeekOrigin` now, their enumerators (`DOOM_KEY_*`, `DOOM_SEEK_*`, the
  button names) keeping their spellings inside `namespace Doom`.

  **`fixed_t` and `angle_t` are done too, and that one was semantic.** They are now
  `using fixed_t = Doom::Fixed;` and `using angle_t = Doom::Angle;` — aliases onto the
  strong types, not raw typedefs beside them. Moving those ~862 uses changed
  arithmetic rather than spelling, which is why it needed its own verification; it is
  finished, not pending. `FixedMul`/`FixedDiv` survive as thin operator wrappers for
  readability at ~150 call sites, no longer a bridge between two representations.

  **The reference-alias layer is gone too**, in two waves: ~290 *cross-file* aliases
  (`fixed_t& viewx = engine().viewPoint.viewx`) first, then 247 *file-local* ones —
  ~537 in all. Every reader now reaches its cluster through the owner, hoisting a
  local reference once per function (`auto& draw = drawState();`) rather than calling
  the out-of-line accessor per access, which matters in the per-pixel drawers.

  It was the **file-local** wave that mattered structurally: those were the last
  bindings pinning the `Engine` to a fixed address at static-init, so retiring them is
  what let the Engine be **constructed** rather than booted. That is Step 9 strand (a),
  and it is worth knowing it was declared finished twice while a whole syntactic tier
  of aliases still stood — `REFACTOR.md` records the six spellings that hid from an
  apparently exhaustive search.

  **13 aliases survive on purpose**: the host callbacks (`doom_print`, `doom_malloc`, …)
  are references onto `Doom::host()`, not onto the Engine. `host()` is deliberately a
  separate immortal singleton that must *not* be reset with a fresh Engine, so pinning
  its address costs nothing. The members are `std::function`s now, constructed with
  working defaults (stdio, `gettimeofday`, `eacp::getEnv` — `Host/Host.cpp`), and an
  embedder overrides them by assigning `Doom::host().print = …` directly; the old
  `doom_set_*` setter functions are gone with the `extern "C"` block. String-shaped
  hooks take `std::string_view` (`print`, `open`, `getenv` — the last returning
  `std::optional<std::string>`).

  **What survives as loose names is data and views, and it lives inside the
  subdirectories** — there is no flat file holding any of it. The pointer-and-count
  views (`vertexes`/`numsegs`/`sectors`/… onto `Doom::Level`, defined in `Sim/Setup.cpp`;
  the `textures`/`sprites` tables onto `GraphicsData` in `Render/`; `finesine`/`finecosine`
  onto `Math/Trig`) are each refreshed by their loader after it fills the owning vector.
  The drawer function pointers `colfunc`/`spanfunc` stay raw in `Render/Main`. The
  automap's vector shapes and the melt's state are deliberate exported carve-outs
  (`UI/AutomapTypes.h`, `UI/Wipe.h`) that the eacp compositor reads by name.

  **The `Engine` is the composition root, and it is no longer three owners.**
  `Engine/Engine.h` aggregates ~83 state clusters reached through free accessors into
  the one `engine()` instance — `randomness()`/`wad()`/`level()`, and alongside them
  the renderer's (`viewPoint`, `graphicsData`, `drawState`, `spriteState`, …), the UI's
  (`menuState`, `automapView`, `hudState`, `statusBarState`, `wipeState`, …) and the
  game's (`gameSession`, `playerState`, `demoState`, `netState`, …).

  The globals this replaced are **in**, not migrating: `doomstat.h`, `r_state.h` and
  `p_local.h` no longer exist. The `~684` figure `REFACTOR.md` quotes is the Step-0
  *starting* count, not a live backlog. Each cluster moved with the subsystem that
  owned it as Steps 6–8 rewrote that code, which is why those steps and this one
  finished together.

  Three constraints died with the single header in Step 1, and the code may now
  rely on their absence: **two files may share a file-scope name** (the header
  concatenated every `.c` into one translation unit, which is why the automap's
  `plr` had to become `am_plr` — still its name, now in `UI/Automap.cpp`); a source
  file may include a system header; and the header include graph need not be acyclic.

  **The function-like macros are gone too.** `Math/Swap.h`'s `SHORT`/`LONG` are one
  deduced-width `Doom::littleEndian` across 154 sites; the automap's seven,
  `Sim/SaveGame`'s `PADSAVEP` and `UI/CheatTypes`' `SCRAMBLE` are functions. One
  family survives on purpose: `Host/Net.cpp`'s `ntohl`/`ntohs`/`htonl`/`htons` sit
  inside `#if defined(I_NET_ENABLED) && !defined(DOOM_APPLE)`, so **no build in this
  repository compiles them** and no gate here could check a change to them.

  The *constant* macros are **essentially closed — 629 → 309 across `src/DOOM`**, and
  199 of that remainder is `Game/StringsFrench.h`, which no build here compiles. Every
  header is done, every `.cpp` pile, and the one- and two-macro tail with them. What
  is left is deliberate rather than pending, and named in `REFACTOR.md` item 6:
  `UI/StatusBar`'s 42 dead ones and the ~55 dead-in-both-eras pile they belong to
  (deleting those is a human's call, and converting one is worse than leaving it —
  see below); the five string families that cannot leave the preprocessor at all,
  because adjacent-literal concatenation happens at translation phase 6; and six
  object-like macros whose bodies call a runtime accessor (`HU_TITLE`, `MAXPLMOVE`)
  and therefore cannot be `constexpr` at all.

  Five things to know before converting another one:

  - **Ask whether the constant already exists — fifteen did, and five of those were a
    latent overrun.** A fixed-size array in a state cluster is sized by that cluster's
    own constant (`PlaneScratch::maxVisplanes`/`maxOpenings`,
    `SpriteState::maxVisSprites`, `BSPScratch::maxDrawSegs`, `SolidSegs::maxSegs`,
    `DrawTables::maxWidth`/`maxHeight`, `SwitchList::maxSwitches`,
    `AnimatedSurfaces::maxAnims`/`maxLineAnims`, `AutomapView::numMarkPoints`,
    `Clip::maxSpecialCross`). **Every overflow guard must test that same constant**,
    not a second one of equal value — otherwise raising the bound moves the array and
    leaves the guard behind, silently and with no diagnostic. `Math/TrigTables.h` is
    the same rule at the other end: it holds the table *views* only, and the
    constants that go with them belong to `Math/Trig.h` and `Math/Angle.h`
    (`fineAngles`, `fineMask`, `slopeRange`, `slopeBits`, `slopeToFixedShift`,
    `ang45`/`ang90`/`ang180`/`ang270`, `Angle::angleToFineShift`). Do not add a
    second spelling of any of these.

    **The category is wider than "a macro with a `constexpr` twin", which is why it
    keeps growing.** Three more instances turned up after that list was written, and
    none of them looks like the others:
    - `FRACBITS` was a plain duplicate of `Fixed::fracBits`/`Doom::fracBits`, sitting
      two lines from `using fixed_t = Doom::Fixed` in `Math/FixedPoint.h`. It caused
      no defect — a shift count cannot overflow an array — so nothing ever surfaced
      it, and two sites had quietly drifted into using **both spellings in one
      expression** (`(topscreen.raw + fracUnit - 1) >> FRACBITS`). It is retired;
      use `Doom::fracBits`. `FRACUNIT` stays, being the `Fixed` value 1.0 rather than
      the integer 65536, and is now defined *from* `Doom::fracUnit`.
    - `Sim/Mobj.cpp` bounded an array sized `MAX_DM_STARTS` with a **bare literal**
      `10`. No second spelling exists, so no grep for duplicate constants could ever
      have found it. **The real category is "the guard and the array bound are not
      the same token."**
    - The savegame description length had **three** spellings, and one of them
      bounded a `doom_read` into a buffer sized by another — an overrun waiting for
      anyone who lowered it. Worse, `MenuState.h` carried a comment explaining that
      the compiler kept the two in step via `char(&)[24]` reference-to-array
      bindings; that was true until Step 9 strand (a) retired every such binding,
      which removed the mechanism and left the reassurance behind. **A refactor can
      delete the thing an older comment depends on, and nothing points at the
      comment.** When you retire a mechanism, grep the prose for its name.

    Where two constants must agree across a subsystem boundary, the fix is a
    `static_assert`, not a third spelling and not a comment (`Game/Game.cpp`'s
    `SAVESTRINGSIZE == menuSaveStringSize` is the worked example).
  - **Do not convert a macro that is dead in both eras.** ~55 of them are, and they
    are left as `#define`s on purpose. A sweep converted ten and every one came out
    as `[[maybe_unused]] constexpr` — an attribute whose only job is to silence the
    diagnostic saying the thing is unused, which is the signal it should not have
    been converted at all. All ten were reverted. (Live constants in *headers* need
    no such attribute either: Clang does not warn for const variables declared in a
    header, which is why `Sim/SimDefs.h`'s 21 never needed one.)
  - **A `constexpr` is implicitly parenthesized and several vanilla bodies are not**
    (`PLAYERRADIUS 16 * FRACUNIT` is the surviving example), so equivalence is a fact
    to establish per call site rather than assume. What breaks is dividing by, or
    taking `.`/`->`/`[]` off, a bare macro.
  - **The name moves into `namespace Doom`, which breaks *global-scope* readers.**
    `Tests/SimProbe.cpp`, `examples/EACP/EngineAccess.cpp`, `Host/Api.cpp`'s
    `extern "C"` block, `Game/Config.cpp`'s `defaults[]` table and the carve-outs at
    the bottom of `UI/Automap.cpp` have all needed qualifying. `UI/AutomapTypes.h` is
    deliberately **all** at `::` scope for this reason; keep it that way.
  - **Not every macro can go.** Feature toggles read by `#ifdef` (`RANGECHECK`,
    `Host/Platform.h`'s three) and string-literal building blocks that rely on
    translation-phase-6 concatenation (`PRESSKEY`, `DOSY`, `DEVDATA`, `DEVMAPS`) stay.
    A body that calls a *runtime accessor* is a different case and no longer a reason
    to stay: it rules out `constexpr`, not the preprocessor, so the live ones became
    inline functions (`hudTitle`, `hudTitleY`, `hudInputY`, `maxPlayerMove`). **Ask
    whether it is live before asking what it should become** — the list that named
    them also named two that are dead in both eras (`ST_MAPWIDTH`, `ST_MAPTITLEX`),
    classified by the interesting property and never against the prior question.

  **The fixed-size C arrays are `Array<T, N>` now** — 111 members across 39
  headers, in the state and scratch clusters. **19 are deliberately still raw**, and
  the distinction is by *struct*, not by file:

  | Still raw | Why |
  |---|---|
  | `Wad/MapFormat.h`'s 8 structs | `reinterpret_cast` onto raw WAD lump bytes |
  | `Render/RenderTypes.h`'s `Patch::columnofs` | the same, one file over — and a *flexible* array, declared `[8]` but indexed to `[width]`, with the pixel data starting at `&columnofs[width]`. `SpriteFrame` and `VisPlane` in that same header are engine-composed and *are* converted |
  | `Game/PlayerTypes.h`'s 9 | `Player` is `memcpy`'d whole by `Sim/SaveGame.cpp`; `IntermissionStart`/`IntermissionPlayer` are `memcpy`'d whole to the `-statcopy` address |
  | `Game/NetTypes.h`'s `NetPacket::cmds` | packed onto the wire, checksummed through a `reinterpret_cast<unsigned*>` and byte-swapped field by field |

  **Four `Array`s became `Vector`s, and the rule that picked them matters more
  than the list.** An `Array` earns a `Vector` only when its length is *data* —
  decided by the WAD or the map — so that the cap, the companion count and any
  terminator all collapse into `size()`. Those four were
  `AnimatedSurfaces::anims` (cap 32 + a `lastanim` cursor),
  `AnimatedSurfaces::linespeciallist` (64 + `numlinespecials`),
  `SwitchList::switchlist` (50 pairs + `numswitches` + a `-1` terminator nothing
  read) and `EnemyAI::braintargets` (32 + `numbraintargets`). Two of the four had
  a live defect in the shape they replaced: `initSwitchList` bounded its walk of
  the **source** table with the **destination's** constant (41 rows against a
  bound of 50 — only the terminator kept it in bounds), and `A_BrainInit`
  appended with **no overflow guard at all**.

  **What must not become a `Vector`, and why**, because the renderer's pools look
  like the same shape and are not:

  | Stays `Array` | Why a `Vector` breaks it |
  |---|---|
  | `PlaneScratch::visplanes`/`openings`, `BSPScratch::drawsegs`, `SpriteState::vissprites`, `SolidSegs::solidsegs`, `Clip::intercepts` | each hands out **interior pointers** that outlive the statement — `openings` worst of all, storing a *biased* pointer (`lastopening - start`) inside a `DrawSeg` |
  | anything `memcpy`'d or `memset` with `sizeof(container)` | `sizeof` silently becomes the vector's own three pointers, and **no compiler warns** |
  | `VisPlane::top`/`bottom` | the deliberate out-of-bounds sentinel writes `pad1`..`pad4` absorb |
  | ring buffers (`events`, `itemrespawnque`, `bodyque`, `chatchars`) | head/tail are wrap cursors, not a live count |
  | slot-indexed tables (`activeplats`, `activeceilings`, `buttonlist`) | the save game archives **by slot**; slot identity is the meaning |
  | `StatusBarWidgets`' arrays | the STlib widgets hold `&w_arms[i]` |
  | fixed domains (256 palette entries, `MAXPLAYERS`, 8 rotations, the trig tables) | the size is the domain, and several are type-punned |

  Before converting a fifth, `grep` the owning struct for `memcpy`, `memset`,
  `sizeof`, `reinterpret_cast` and `&…[`, and ask whether any pointer into it
  outlives the expression.

  Four things that bite when converting or reading these:

  - **`EA::Array` value-initializes; a raw C array does not.** Its sole member is
    declared `ContainerType container {}`, so `EA::Array<char, N> x;` zeroes where
    `char x[N];` left garbage. "I left the `= {}` off, so nothing changed" is false.
  - **It adds no storage**, so it is layout- and size-identical to the raw array —
    but that is an implementation fact about eacp, not a language guarantee. One
    place genuinely depends on it: **`VisPlane::top`/`bottom` are indexed out of
    bounds on purpose**, `Render/Planes.cpp` writing a `0xff` sentinel at
    `[minx - 1]` and `[maxx + 1]` so the span loop needs no bounds test in its inner
    loop. That is what `pad1`..`pad4` are for. `RenderTypes.h` pins it with a
    `static_assert` rather than a memory.
  - **`EA::Array` does not decay to `T*`.** Sites using the bare array as a pointer
    need `.data()`; `&arr[i]` is fine unchanged. The pointer-*difference* idiom is
    the one that hides — `player - players_.players` computes a player index in four
    places and none was in any survey. The compiler catches each individually, which
    makes this one of the few sweeps here that the build verifies for you.
  - **`&arr[N]` — the one-past-the-end pointer — is the exception to "`&arr[i]` is
    fine", and no compiler catches it.** On a raw C array it is the ordinary way to
    write an end pointer. On `EA::Array` it is `std::array::operator[]` with an
    out-of-range index, and **MSVC's debug STL asserts on it** — which is a *runtime*
    failure, in `Debug` only, on one toolchain, in code that compiles cleanly and
    runs correctly everywhere else. Three sites had it (`Sim/Mobj.cpp`'s
    `deathmatchstarts`, `Render/Segs.cpp`'s `drawsegs`, `Render/Things.cpp`'s
    `vissprites`); all are `.data() + N` now. `grep -nE '&[A-Za-z_.>-]+\[[A-Za-z_:]*(max|MAX|NUM)'`
    finds the family.

    Its cousin is the **deliberate** out-of-bounds subscript, which needs the same
    treatment for the same reason: `Render/Planes.cpp` writes a sentinel at
    `top[minx - 1]` and `top[maxx + 1]` and the span loop reads both ends, which is
    exactly what `VisPlane`'s `pad1`..`pad4` exist to absorb — but `operator[]`
    checks the *declared* extent and cannot know about the padding. Reached through
    `.data()` now; identical bytes, no assertion.

    **How this presented is the part worth remembering.** The debug CRT reports
    through a modal dialog, so under ctest the binary stopped dead with no output, no
    exit code and no CPU, and the suite hung on demo1 looking exactly like an
    infinite loop in the engine. `Tests/TestMain.cpp` now routes `_CRT_WARN`/
    `_CRT_ERROR`/`_CRT_ASSERT` to stderr so the next one fails loudly and says what
    it is.
  - **`EA::Array<char, N>` is not an aggregate**, so a bare string literal in a table
    stops binding — the reason `Sim/Switches.cpp`'s `alphSwitchList[]` once spelled
    every row `EA::Array<char, 9> {{"SW1BRCOM"}}`. (The string refactor has since made
    those name tables `std::string_view`, which binds literals plainly; the quirk
    still holds for any `EA::Array<char, N>` that remains.) Verify any bulk string
    change by extracting every literal before and after and diffing them.

  **`doom_boolean` is gone** — the last vanilla type. All ~288 uses are a real
  `bool`, and the typedef is deleted, so a boolean in this engine is a boolean.
  What is left of it is a short list of declarations that must **stay `int`**, each
  saying so at its own site, because each is storage that only *looks* like a flag:
  `Render/Data.cpp`'s `MapTexture::masked` (overlaid on raw `TEXTURE1` lump bytes,
  where its four bytes hold the following fields in place), `Game/GameSession.h`'s
  `deathmatch` (tri-state: 0 coop, 1 deathmatch, 2 altdeath), `Sim/Specials.cpp`'s
  `AnimDef::istexture` (its table ends on a `-1` sentinel that would read as `true`),
  and `Host/Net.cpp`'s `trueval` (its address goes to `ioctl(FIONBIO)`, which reads
  a whole word back through it). `doomtype.h` keeps that list. The ARMS widget's
  `(int*) &plyr->weaponowned[i + 1]` pun — vanilla's own cast, and the reason the
  flip was blocked for so long — is untangled into `StatusBarWidgets::w_armsindex[6]`.

  **The strings are `std::string`/`std::string_view` now, and the C string layer is
  gone with the `extern "C"` API.** The `doom_strcpy`/`doom_concat`/`doom_itoa`/
  `doom_strlen`/`doom_strcmp`/`doom_toupper`/`doom_atoi` family is deleted;
  `Host/Text.h` is what replaced it — `concat(parts...)` (string-ish parts as text,
  `char` as one character, integrals via `std::to_string`), variadic `print(...)`/
  `printTo(handle, ...)`, variadic `fatalError(...)` (`Host/System.h`),
  `hexString`, `toUpper`/`equalsIgnoreCase`, and `parseInt`/`parseHex` (exact ports
  of `doom_atoi`/`doom_atox`, sign handling and all — the goldens pin their
  no-sign behaviour). The global `error_buf` and its ~250 build-then-abort chains
  are single `fatalError(...)` calls now; lump-name composition (`"STTNUM"+i`,
  `"WIMAP"+epsd`, `"ds"+sfxname`, `E?M?`) is `concat(...)` straight into the WAD
  lookups, which take `std::string_view`.

  **The raw `const char*` layer is gone too, and that was a second pass.** The string
  refactor above retired the C string *functions*; it left ~363 `const char*`/`char
  const*` declarations standing, which the follow-up sweep took to **10, four of them
  comments**, and a third pass has since taken it to **zero that carry text** — every
  `char*` left in the repository is a byte cursor, an OS entry-point signature or a
  cast a C API demands, and they are listed under **What deliberately stays raw**
  below. What moved: the file-I/O and name-taking parameters (`readFile`,
  `writeFile`, `WritePCXfile`, `getsfx`, `addMessageToSText`, the probe's boot and
  lookup entry points), the static name and message tables (`mapnames` and its three
  siblings, `player_names`, `gammamsg`, `endmsg`, `chat_macros`, `SfxInfo::name`,
  `MusicInfo::name`, `Finale`'s 22 per-ending text pointers), the config table's text
  plumbing (`ConfigDefault::name`/`text_location`/`default_text_value`), the two
  message-box holders (`MenuState::messageString`, `AttractMode::pagename`), and all
  **287** of `StringsEnglish.h`'s constants, which are `constexpr std::string_view`
  now. `StringsFrench.h` was deliberately left alone — it is still `#define`s, and no
  build here compiles it.

  Five things to know before touching strings here:

  - **An 8-byte WAD name field is NOT NUL-terminated when full**, and a
    `std::string_view` built from a bare `const char*` runs `strlen` off its end.
    `nameView(ptr, 8)` (`Host/Text.h`) is the bounded view; `WadFile.h` and
    `Render/Data.h` both say so at their lookup declarations, and `Sim/Setup.cpp`'s
    map-loader calls are the worked example. `Game/Game.cpp`'s savegame version
    check reads its 16-byte field through it for the same reason.
  - **A `std::string_view` built from a null pointer is UB, and the null arrives as a
    literal `0` that no grep for `nullptr` will find.** This is the one hazard the
    `const char*` → `std::string_view` sweep actually hit, twice, and both times the
    failure was a segfault a long way from the edit. `Game/SoundData.cpp`'s
    `S_music[0]` dummy entry was spelled `{ 0 }` — a null *name*, harmless as a
    pointer nothing dereferenced, fatal at static-init as a view (it is `{ "", 0 }`
    now). `Tests/SimProbe.h`'s "boot with no demo" was spelled `doomSimBoot(0)` at 13
    call sites, and 16 tests segfaulted at once; the parameter is defaulted
    (`doomSimBoot()`) rather than passed an empty literal, because "no demo" should
    not read as "a demo named nothing". **Before converting a pointer that carries a
    string, grep its writers for a bare `0` as well as for `nullptr`** — and remember
    that a null `const char*` guard (`if (prefix)`) becomes `.empty()`, not a
    truthiness test that no longer compiles.
  - **Fixed-width on-disk fields stay fixed-width**: the savegame's 24-byte
    description and 16-byte version fields are written with `fillField`
    (`Host/Text.h`) — zero-padded, deterministic — and read back bounded. The menu's
    `savegamestrings` are `std::string` in memory and a fixed field only at the
    file boundary.
  - **A message pointer needs storage that outlives the frame.** `Player::message`
    is a `std::string_view` — everything assigned to it is a string constant or an
    Engine-owned/static `std::string` (`turbomessage`, the IDMYPOS buffer,
    `NetState::exitmsg`, HUD chat's `lastmessage`), reassigned in the same breath as
    the view and never freed. It was `const char*` for a long time and the reason
    given was that `Sim/SaveGame.cpp` memcpy's the whole `Player`, but that argument
    only rules out an *owning* type: memcpy'ing a `std::string` would double-free on
    load, while a view is trivially copyable and copies as safely as the pointer did.
    `unArchivePlayers` clears it right after the memcpy, next to `mo` and `attacker`,
    so nothing stale survives a load either way. The same reasoning rejects
    `std::string*`, which would work mechanically but force each of the ~69
    compile-time pickup constants into a heap-allocating object to express what is
    already a literal. **The constraint is trivially copyable, not `const char*`** —
    that applies to every other field of `Player` too.
  - **What deliberately stays raw, and the rule that decides it.** The question is
    never "is this a `char*`" but **"does it carry text"**. Every survivor answers no:
    the WAD overlay structs (`Wad/MapFormat.h`) and `WadFile`'s `matches` (a `memcmp`
    over 8 raw directory bytes), `Game/PlayerTypes.h`'s memcpy'd arrays,
    `doom_memset`/`doom_memcpy`, `nameView`/`fillField`'s own raw parameters (they
    are the primitives that *bound* raw bytes into a `string_view`, so they cannot
    take one), `Host/Net.cpp`'s `setsockopt` casts, `main`'s `char** argv` and the
    `initGame` overload that exists to receive it, `UI/Hud.cpp`'s `shiftxform` (a
    128-entry translation *table*), the PNAMES lump walk in `Render/Data.cpp` (raw
    lump bytes, `const` now, feeding `nameView`), `Host/Sound.cpp`'s PCM and MUS
    cursors (11kHz 8-bit samples — the thing that most *looks* like a string here
    and is furthest from one), and the five translation-phase-6 string-literal macro
    families (`PRESSKEY` and friends).

    Three that used to be on this list have come off it, and each for a different
    reason, which is worth knowing before adding a fourth:
    - **The cheat sequences** were listed as permanently raw because `checkCheat`
      *writes into* them. That ruled out `std::string`, not a raw pointer:
      `CheatSequence` is a `std::span<unsigned char>` plus an `int position` now
      (`UI/CheatTypes.h`), which keeps every in-band `0`/`1`/`0xff` marker and the
      write-back exactly as it was. "It is mutable" is an argument against an
      *owning immutable* type, not against a typed view.
    - **`myargv`/`findResponseFile`**, called an open ownership question with no
      test driving it, was resolved by making `myargv` a `std::vector<std::string>`
      — see **The command line owns itself now** below.
    - **`addPrefixToIText`** kept its `char*` on the grounds that converting dead
      code dresses it as live. That reasoning was borrowed from the ten reverted
      `[[maybe_unused]] constexpr`s and does not transfer: an attribute whose only
      job is to silence an unused-warning is evidence the thing is dead, whereas a
      parameter type is not evidence of anything. It is `std::string_view`.
      (If dead code is the objection, the answer is deletion, not a worse type.)

    The two lineage bugs the sweep found —
    quicksave/quickload's third call is `strcpy` where `concat` was meant, so the
    prompt never shows the savegame name — are **preserved and documented at their
    sites**, not fixed, like the MUS-delay and `drawAnimatedBack` precedents.

  **The command line owns itself now.** `myargv` is a `std::vector<std::string>`
  (`Game/Args.h`), and `initGame` copies what the host hands it rather than keeping
  the pointer — `DOOM.h` carries a `std::vector<std::string>` overload for hosts not
  forwarding `main()`, and `Tests/SimProbe.cpp`'s three `static char[]`s, which
  existed *only* because the engine kept the pointer, are one `std::vector` now.
  What that resolved was not a spelling:

  - `findResponseFile` did two `doom_malloc`s that nothing ever freed — the file's
    contents and a fresh `MAXARGVS`-sized pointer array — and the second replaced
    `myargv` wholesale, so the original was leaked too.
  - It wrote a NUL terminator **one byte past** the contents buffer whenever the
    last token ran to EOF (`*(infile + k) = 0` with `k == size`). Same family as
    the `doomu.wad` overflow the Windows port found, and equally invisible here.
  - Two silent ceilings: `MAXARGVS` (100) tokens, and **20** arguments following
    the `@file`. Both walked off the end rather than reporting anything.

  All four are gone with the raw pointers, and the tokeniser keeps vanilla's own
  character class (a token runs while the byte is `>= ' '+1` and `<= 'z'`), empty
  leading token included. `myargc` is retired in favour of `myargCount()`, which
  returns `myargv.size()` — the count and the container cannot drift, which is the
  same rule the overflow-guard note above states for array bounds.

  **`Host/Sound.cpp`'s `paddedsfx` was the audio-blocked RAII owner, and it was
  also a live leak.** `initSoundHost` runs at *boot* (`Host/System.cpp`), not when
  audio starts, so every boot `doom_malloc`'d ~107 padded sample buffers and freed
  none — the buffers are a `std::vector<std::vector<unsigned char>>` now. Two things
  made this look blocked when it was not: nothing plays audio, so no gate could
  *hear* a regression; and `Sim/OwnershipTests.cpp` counts blocks from a post-boot
  baseline, so a leak taken during boot is subtracted out by construction. Ownership
  lives beside the table rather than in `SfxInfo::data` because a linked sound (the
  chaingun onto the pistol) deliberately points at its target's buffer — making the
  member owning would have turned that alias into a copy.

  **One latent bug found and deliberately left**: `UI/Hud.cpp`'s chat responder
  indexes `shiftxform` — 128 entries — with an `unsigned char`, so `chat.shiftdown`
  plus any key code above 127 reads past the table. Vanilla's own defect, in a
  multiplayer path no test reaches. It is also why `shiftxform` stays a raw pointer:
  as a `std::span` the same read becomes an **MSVC debug-STL assertion**, which is
  the Debug-only, one-toolchain, modal-dialog failure mode documented under
  **Windows** — a worse outcome than the silent read, arrived at by accident.

  **A pointer parameter that can never be null is a reference — 109 functions
  are, now.** The whole specials family (`doDoor`, `doPlat`, `doCeiling`,
  `doFloor`, `buildStairs`, `teleport`, `changeSwitchTexture`, the seven
  `Sim/Lights` spawners, the six `find*Surrounding` queries, `crossSpecialLine`,
  `shootSpecialLine`, `useSpecialLine`, `getNextSector`, …), the playsim core
  (`tryMove`, `checkPosition`, `teleportMove`, `setMobjState`, `removeMobj`,
  `addThinker`/`removeThinker`, `touchSpecialThing`, `changeSector`, `slideMove`,
  `lineAttack`, `useLines`, the `give*` family), the seven `Event*` responders,
  the `Ticcmd` path, and the automap's own line helpers. **The nullability was
  established per call site, not assumed** — three surveys over every call site in
  `src`, `Tests` and `examples`, plus a check for a null test inside each body,
  which is the strongest evidence a parameter *is* nullable.

  **What deliberately stays a pointer, because null is a real value it carries:**

  | Stays `T*` | Why |
  |---|---|
  | `damageMobj`'s `inflictor` and `source` | `MapAction.cpp`'s crush and `Specials.cpp`'s slime pass a literal null; the body tests both |
  | `radiusAttack`'s `source` | `explode()` passes `thingy.target`, unguarded, and the path is *designed* to accept null |
  | `spawnMissile`'s `dest` | the `fatAttack1/2/3` family passes `actor.target` with no guard, unlike every sibling attack |
  | `checkSight`'s `t1`, `aimLineAttack`'s `t1` | `fire()` and `bfgSpray` pass `.target` unguarded |
  | `killMobj`'s `source` | inherits `damageMobj`'s |
  | `Render/Planes`' `checkPlane` | `BSP.cpp` nulls `floorplane`/`ceilingplane` outright; the invariant that saves it is a three-fact argument across two files, not an address-of |
  | `drawMaskedColumn`, `drawColumnInCache` | the parameter is a loop *cursor*, reassigned in the body to walk posts |
  | `startIntermission` | it **stores** the pointer (`im.wbs = wbstartstruct`) well past the call |
  | the `drawPatch` family | ~45 call sites pass `cacheLumpName(...)` straight in; a reference makes every one `*static_cast<Patch*>(...)`. The real fix is a `Patch&`-returning lookup, not this |
  | `Actions.cpp`'s 75 state actions, `callWeaponAction` | address-pinned by `states[]`; they deref into the reference-taking implementations |

  `SaveGame.cpp` nulls **every** mobj's `target` on load, which is what turns those
  unguarded `.target` reads from theoretical into reachable. That is the single
  fact holding up most of the "stays a pointer" column.

  Three things to know before converting the next one:

  - **Capturing a now-reference parameter *by value* in a lambda copies the whole
    object, and the goldens will not see it.** `Sim/Switches.cpp`'s `startButton`
    had `[line](const Button& button) { return button.line == &line; }` — with
    `line` a `Line&`, `&line` is the address of the closure's *copy*, so the
    "this line already has an active button" test could never match. It compiled,
    and **all 98 tests passed with it in**, because no demo presses one switch
    twice inside its timer. The compiler caught it one call over only by accident,
    on a `const` mismatch. After any pointer→reference sweep, grep every lambda
    capture list in the touched files for a by-value capture of a converted
    parameter.
  - **`sizeof` is the one place this does *not* bite.** `sizeof(*ptr)` becomes
    `sizeof(ref)`, and a reference's `sizeof` is the referent's — same value.
    (Contrast the `EA::Array` note above, where `sizeof` silently changes.) What
    *would* bite is code that sized the pointer itself; none did.
  - **Rewriting `param->` to `param.` in a body over-reaches on `other->param->`**
    — the word boundary matches the inner `param->` of `player->mo->state` when
    the parameter is `mo`. Harmless in practice: a pointer member reached with `.`
    is always a compile error, so the compiler catches every instance. Do not take
    that as licence to skip reading the diff, but do not fear the sweep either.

  Two vanilla latent defects the surveys turned up, **left as they are** for the
  same reason as the MUS-delay and `drawAnimatedBack` precedents: `Enemy.cpp`'s
  `fatAttack1/2/3` pass `actor.target` to `spawnMissile` with no null guard where
  every sibling attack has one, and `Specials.cpp`'s `doDonut` dereferences
  `getNextSector`'s result unchecked (every other call site guards it), so a
  malformed donut sector faults.
- `Tests/` — the test suite. See **Testing**.
- `examples/EACP/` — the eacp port. `Main.cpp` boots the engine, `View.h` is the
  eacp platform layer and GPU renderer, and `EngineAccess.h/.cpp` is the plain-data  snapshot interface to engine internals (camera, wall geometry, view state).
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
`SimTests` and `PrimitiveTests` (see **Testing**), `record-goldens`, and
`doom-sim-probe` — the static library holding `Tests/SimProbe.cpp`, which both
test binaries link so the shim is compiled once rather than per binary. You never
invoke that last one directly; it builds as their dependency. There are only two
build options, `PUREDOOM_BUILD_TESTS` and `PUREDOOM_BUILD_EACP_EXAMPLE`.

The tests need **no GPU**, so `-DPUREDOOM_BUILD_EACP_EXAMPLE=OFF` still gives a
fast loop while refactoring, and that is what CI builds. They do **not** link
`doom-engine` alone any more, and neither does the engine: both link **`eacp-core`**,
for the platform work they would otherwise hand-roll per OS (today
`<eacp/Core/Utils/Environment.h>` — reading and writing an environment variable has
no portable spelling, and `std::getenv` is deprecated outright by Microsoft's CRT).

That flag now controls more than the app: with it `OFF` the root `CMakeLists.txt`
passes `EACP_BUILD_GRAPHICS OFF`, so eacp compiles Core, SIMD and Network and stops
— no Graphics, GPU, WebView, Camera or Video. `doom-engine` links `eacp-core`
**PRIVATE**: it is an implementation choice, `DOOM.h` stays a standard-library-only
header with no eacp type in it, and an embedder still overrides any of it through
the `doom_set_*` callbacks. The tests name `eacp-core` themselves because they use
it directly.

**Build it with a second compiler before believing a warning count.** GCC catches
things Apple Clang does not, and — as of the session that first tried it — the
engine is clean under both, goldens included:

```bash
brew install gcc   # Homebrew GCC; /usr/bin/gcc on macOS is Apple Clang wearing a hat
cmake -G Ninja -B build-gcc -DCMAKE_BUILD_TYPE=Release \
      -DPUREDOOM_BUILD_EACP_EXAMPLE=OFF \
      -DCMAKE_C_COMPILER=$(brew --prefix)/bin/gcc-16 \
      -DCMAKE_CXX_COMPILER=$(brew --prefix)/bin/g++-16
cmake --build build-gcc && ctest --test-dir build-gcc
```

Note `/usr/bin/gcc` is Apple Clang under another name, so pointing CMake at it
measures nothing new — the Homebrew path is the point.

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

98 tests, roughly thirty seconds for the lot. **Run it before and after anything you change
in `src/DOOM`.**

Two binaries, and which one a test lives in is not cosmetic. **`SimTests`** boots
the engine, and only it links `Tests/TestMain.cpp`, which points `DOOMWADDIR` at
the repository root — so **any test that boots belongs there**. **`PrimitiveTests`**
takes NanoTest's default `main` and covers what needs no boot. A booting test put
in `PrimitiveTests` finds the IWAD when you run the binary by hand from the repo
root and fails under ctest, which runs it from elsewhere; that reads as flakiness
and is not.

What each source covers, since the sections below do not name them all:

| File | Binary | What it holds |
|---|---|---|
| `Sim/DemoTests.cpp` | SimTests | the three attract demos, world + frame goldens |
| `Sim/ReplayTests.cpp` | SimTests | replay-twice and load-a-second-demo; the per-level reset |
| `Sim/LevelTests.cpp` | SimTests | the geometry-view invariant after a load |
| `Sim/WadTests.cpp` | SimTests | all 1,264 lumps against `doom1.lumps` |
| `Sim/MenuTests.cpp` `Sim/AutomapTests.cpp` `Sim/FinaleTests.cpp` | SimTests | the three screens no demo reaches — plus, in `AutomapTests`, the automap's vector shape tables, which its *frame* golden cannot reach (they are drawn only under the IDDT cheat) |
| `Sim/ScenarioTests.cpp` | SimTests | place a mobj, move it, assert — see **Layers still to build** |
| `Sim/SaveGameTests.cpp` | SimTests | the save/load round trip, and `readFile`'s owner |
| `Sim/OwnershipTests.cpp` | SimTests | that destroying an `Engine` gives the memory back |
| `Sim/PrimitiveTests.cpp` `Sim/MathTests.cpp` `Sim/GeometryTests.cpp` | PrimitiveTests | the arithmetic underneath the simulation — including the endian swaps, which matter out of proportion to their size because `littleEndian()` is the identity on every machine this builds on, so a typo in `swap16`/`swap32` would surface only on a big-endian host |
| `Sim/CheatTests.cpp` | PrimitiveTests | the cheat-sequence matcher — the in-band `1`/`0xff` marker protocol, the parameter slots `checkCheat` writes and `getParam` clears, and the restart on a wrong key |
| `Sim/EngineTests.cpp` | PrimitiveTests | the composition root, and that `resetEngine` is genuine |
| `Sim/StateClusterTests.cpp` | PrimitiveTests | the `Engine`'s state clusters and accessor identity |

`Sim/OwnershipTests.cpp` is the odd one and worth knowing about: it is the only
test that can see a **leak**. The goldens hash the world and the picture, so
memory that is never given back changes nothing they measure until the process
runs out. It installs a counting `malloc`/`free` pair on `Doom::host()` and
asserts that live blocks after `resetEngine()` fall back to the post-boot
figure — which is how the level pool's missing destructor was found.

Run the binaries through ctest, not bare. NanoTest registers one ctest case per
test and re-runs the binary with `--test <name>`, so every test gets a fresh
process — which the engine needs, being several hundred globals and a zone
allocator that `Doom::initGame` does not undo. A bare `./SimTests` puts all of them in
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

**A second measurement of the same sharpness, worth knowing because it settles a
question that is otherwise pure reasoning.** `Doom::sortVisSprites` orders the
sprites back-to-front, and vanilla selection-sorted them with a strict `<`, so
two sprites at *equal* scale kept the order they were added in. Whether that
stability is load-bearing depends on whether equal fixed-point scales actually
occur, which no amount of reading the code answers. Substituting `std::sort` for
`std::stable_sort` moves the **frame** golden at demo2 tic 412 and demo3 tic 232
and leaves every simulation hash untouched — so ties happen twice in three
demos, the sort must stay stable, and the two golden families separate exactly
as designed. If you change that function, run this experiment rather than
trusting the diff.

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

Audio, and the eacp platform layer and GPU renderer. That is now the whole list —
but it was wrong until recently, and the way it was wrong is the lesson. **The
cheat matcher** (`UI/Cheat.cpp`) was live engine code with no gate over it at all:
no demo enters a cheat, and none of the four screen harnesses below types one
either. It went unlisted because this section was organised around *screens*, and
`checkCheat` is not a screen — so a category nobody had thought to name could not
show up as missing. It has `Tests/Sim/CheatTests.cpp` now, written when the
`CheatSequence` was converted and demonstrated sharp against two mutations (drop
`getParam`'s slot clearing → `Cheat/canBeEnteredTwice` fails; drop the restart on a
wrong key → `Cheat/wrongKeyRestartsTheMatch` fails). **Before refactoring anything,
check what covers it by running the code, not by reading a list like this one.**

The four screens a demo never reaches each have their own harness: the first
three a frame golden, all built the same way and all built *before* the code they
cover was rewritten, and the fourth a transition test awaiting its golden:

- **The menu** (`Tests/Goldens/menu.frames`, via `Tests/MenuReplay.h`) — nothing
  in a demo opens one. Synthetic `doom_key_down` events drive a scripted walk
  through the menus over the title screen, hashed every tic, the same Step-0
  technique the renderer got. Built before `m_menu` became `UI/Menu`, and holds
  it byte-identical.
- **The automap** (`automap.frames`, via `Tests/AutomapReplay.h`) — no demo opens
  it either, and for a reason worth knowing: `D_Display` skips
  `R_RenderPlayerView` entirely while the map is up, which is the same quirk that
  made `eacpDoomRevealAutomap` necessary. Loads E1M1 directly, then walks the map
  — follow on and off, hand-panning, zoom, the big overview, the grid, marks —
  asserting the map actually opened at each transition rather than assuming a
  keypress landed.
- **The finale** (`finale.frames`, via `Tests/FinaleReplay.h`) — reached by
  calling `startFinale` after loading E1M8, that being the map whose completion
  triggers it. Hashes the text crawl, the stage-transition wipe and the settled
  screen. The cast call and bunny scroll are DOOM II / episode-3 only, so the
  test *asserts* the game mode is shareware rather than leaving "unreachable" as
  a comment.
- **The intermission** (`Tests/Sim/IntermissionTests.cpp`, via
  `Tests/IntermissionReplay.h`) — the fourth screen, found uncovered *after* the
  list above was declared complete: no demo completes a level, so nothing had
  ever run `doCompleted`, `drawIntermission`, or `doWorldDone`. Unlike the other
  three it needs no direct entry-point call — `Doom::exitLevel()` is the real
  thing, so it drives the genuine E1M1 → scoreboard → E1M2 transition and asserts
  the state machine rather than frames. Its first sanitizer run caught a real
  refactor-era defect: the intermission's last tic draws *after*
  `endIntermission()` has run — `updateNoState` unloads and queues `ga_worlddone`,
  but gamestate stays `GS_INTERMISSION` until the next tic's `doWorldDone` — so
  `drawEL` read a `lnames` the unload had just cleared. Vanilla has the identical
  call order and survived it because `WI_unloadData` was `Z_ChangeTag(PU_CACHE)`,
  which left the memory readable; the Step-9 RAII conversion turned that harmless
  1993 idiom into a read past a vector's size that only worked because `clear()`
  keeps the old buffer. Fixed at `unloadIntermissionData` (which now deliberately
  keeps the table), and pinned twice: `Sim/intermission` re-reads it under any
  sanitizer run, and `Sim/intermissionLastDraw` asserts the drawer's
  precondition — a tic that ends still on the intermission drew it, so the
  level-name table must still be filled — which is red in *any* build if the
  unload order regresses. **No frame golden yet**: recording one is the natural
  follow-up now that the sanitizers run clean through the transition.

Each was demonstrated **sharp and non-redundant** when recorded: a
one-palette-index change to `WALLCOLORS` fails only `Sim/automap`, and
`TEXTSPEED` 3→4 fails only `Sim/finale`, with the demo goldens green through
both. That matters — a golden recorded *after* a rewrite pins whatever the
rewrite did, and a golden that no plausible change would fail is worse than none,
because it reads as coverage.

**Before refactoring a file, check what actually covers it.** Two of these three
were found only because a sweep was about to land under a green suite that never
ran the code.

The `-DPUREDOOM_BUILD_EACP_EXAMPLE=OFF` fast loop has its own blind spot: it
never compiles `examples/EACP`, and `EngineAccess.cpp` includes engine headers
directly, so an engine change can break the app with every test still green. Keep
a second build directory for it and treat the app linking as a fourth gate.

### Windows, and the three memory bugs it found

The engine builds and passes all 87 tests on **Windows arm64**, under both
**clang-cl** and **MSVC**, in `Debug` and `Release`. Getting there was not a
portability exercise: the platform surfaced three genuine defects that macOS and
Linux had been absorbing, and **none of them was visible to any gate in this
repository**, because the goldens hash the world and the picture and all three were
correct right up until the heap gave out.

- **A one-byte heap overflow, every boot.** `IdentifyVersion` sized seven WAD search
  paths by hand as `strlen(dir) + 1 + <basename length> + 1`, with the length written
  as a literal — and `"doomu.wad"` is nine characters counted as eight. The
  terminating NUL landed one byte past the allocation. On macOS the block's padding
  absorbed it; on Windows arm64 it corrupted the following block and demo1 and demo3
  **segfaulted** before rendering a frame. AddressSanitizer names it in one line. The
  literals are gone — `joinWadPath` measures the string it is about to write.
- **The tutti-frutti guard was too small**, which is written up under
  **The primitive tests give locality** above. 64 → 256, no golden re-recorded.
- **Three one-past-the-end `&arr[N]` subscripts** and one deliberate out-of-bounds
  write, all tripping MSVC's debug STL — see the `EA::Array` notes under **Layout**.

Two lessons generalise past Windows:

- **`Debug` and `Release` fail differently, and `Debug` fails worse.** Release
  segfaulted, which at least says *something*. Debug hung: the debug CRT reports a
  corrupted heap or a failed assertion through a **modal dialog**, which under ctest
  reaches no desktop, so the binary stopped with no output, no exit code and **no
  CPU**, indistinguishable from an infinite loop. Flat CPU is the tell — a slow test
  burns CPU, a blocked one does not. `Tests/TestMain.cpp` now routes those reports to
  stderr.
- **Run AddressSanitizer when a golden moves for no reason.** It found the overflow
  immediately, with a stack, after an afternoon of reasoning had not. On Windows arm64
  the *LLVM* toolchain ships no ASan runtime (only `clang_rt.builtins`), but **MSVC's
  does** — build with `cl /fsanitize=address` and `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL`
  (ASan cannot link the debug runtime). Note that overriding `CMAKE_CXX_FLAGS` drops
  CMake's injected `/DWIN32`, which is its own trap; see `Host/Platform.h`.

`_WIN32` is the macro to test, never bare `WIN32`. `WIN32` is not a compiler macro at
all — it arrives from the Windows SDK or from a build system that adds `-DWIN32`,
which CMake happens to do for MSVC-style drivers. `Host/Platform.h` and `Host/Api.cpp`
both tested it, so every Windows build was one build system away from silently taking
the `DOOM_LINUX` branch and failing to compile on `<sys/time.h>`. Both test `_WIN32`
now. (`Host/Api.cpp` also included `<winsock.h>` — Winsock *1* — purely to reach
`windows.h` for `GetSystemTime`, which would conflict with `Host/Net.cpp`'s
`<WinSock2.h>`; it includes `<windows.h>` directly now.)

### Read the warnings — they are a fifth gate

The engine builds under `-Wall -Wextra -Wpedantic` with **zero warnings**. **Anything
at all is a regression.** It was 81 before the session that cleared them and 1 for a
while after — `Sim/Weapon.cpp`'s type-erased `states[].action` cast, which is a round
trip and therefore well-defined, and which now sits behind a scoped suppression
spelled for each compiler. This is a state to hold, not a number to admire.

That zero is measured on Apple Clang (`Debug` and `Release`), on **real GCC 16**
(`Release`), and now on **Windows arm64 with both clang-cl and MSVC** (`Debug` and
`Release`, all 87 tests green on each). Only **Ubuntu's gcc and clang** are still
unmeasured — a different standard library, so a different set of transitive
includes. `-Werror` waits on that: see `REFACTOR.md` item 7.

On Clang the zero now also includes **`-Wshorten-64-to-32`**, appended to
`DOOM_WARNINGS_ON` in `src/DOOM/CMakeLists.txt`. The Xcode generator passes that
flag on its own (it is an Xcode project default), which is how 15 `long`→`int`
narrowings sat visible in Xcode builds while every Ninja build read zero — most
of them `State`'s `long` fields (`tics`/`frame`/`misc1`/`misc2`, now `int`, the
value-mixing checksum in `Tests/Sim/PrimitiveTests.cpp` proving the table
unchanged). Pinning the flag makes the two generators agree on what zero means;
GCC has no such flag, and its nearest spelling (`-Wconversion`) stays the
deliberate future decision of `REFACTOR.md` item 7.

**The flags are chosen by the driver, not by the compiler's name, and getting that
wrong is not a no-op.** `src/DOOM/CMakeLists.txt` used to select them with
`$<CXX_COMPILER_ID:MSVC>`. clang-cl's `CMAKE_CXX_COMPILER_ID` is **`Clang`** with
`CMAKE_CXX_COMPILER_FRONTEND_VARIANT` **`MSVC`** — so it took the GNU branch and fed
`-Wall -Wextra -Wpedantic` to an MSVC-style driver, where **`-Wall` means `/Wall`,
which clang implements as `-Weverything`**. That one substitution produced **~44,000
warnings**, nearly all `-Wold-style-cast`, `-Wpadded` and `-Wunsafe-buffer-usage`, and
most of them raised inside eacp's headers rather than in any code here. It also
silently dropped `-ffp-contract=off` ("unknown argument ignored in clang-cl", once per
TU, lost in the noise) — so the determinism guarantee the goldens rest on was *not*
in force on the one Windows toolchain most likely to contract. Ask
`CMAKE_CXX_COMPILER_FRONTEND_VARIANT`; clang-cl takes `/clang:-ffp-contract=off`.

**MSVC's `/W4` is not `-Wall -Wextra`, and six of its warnings are switched off on
purpose** (`/wd4244 /wd4267 /wd4459 /wd4457 /wd4702 /wd4805`). Each corresponds to a
GCC/Clang flag this project has deliberately *not* enabled — `-Wconversion`,
`-Wshadow`, `-Wunreachable-code` — so leaving them on would make "zero warnings" mean
something different on each toolchain rather than the same thing everywhere. 121 of
the 136 were `C4244`, DOOM's pervasive and load-bearing int→short/byte truncation.
Raising that bar is a real decision, and it should be taken for all compilers at once
rather than arrived at by accident on one; the reasoning is written out at the flags.

**CI's five rows are four toolchains.** On a macOS runner bare `gcc`/`g++` resolve to
`/usr/bin`, which is Apple Clang wearing the name — so `.github/workflows/tests.yml`'s
`macos-latest × gcc` row is the `macos-latest × clang` row run twice. Do not read it
as gcc coverage. The workflow now has a **Report warning count** step that prints a
per-configuration count into the job summary and fails on nothing, so the two genuinely
unmeasured toolchains measure themselves on the next push.

**Two things the first GCC build taught, both of which cost a working day and neither
of which is visible from a Clang-only measurement:**

- **A warning suppression is scoped to one compiler and spelled in its dialect.**
  Two generated tables carried `#pragma GCC diagnostic ignored "-Wwritable-strings"`
  — Clang's name for the flag; GCC's is `-Wwrite-strings`. Clang went quiet, GCC did
  not recognise the option, warned about *that*, and then emitted **314** warnings the
  other compiler had never shown. A third copy of the pragma was suppressing a warning
  its table could no longer raise. So a suppression fails silently in the direction
  that looks clean — prefer fixing the type (these wanted `const char*`) over naming
  the flag, and if you must suppress, spell it per compiler behind `__clang__` /
  `__GNUC__` and put it around the smallest possible scope.
- **Nothing here is a C++20 module, but CMake scans for them anyway**, which puts
  `-fmodules-ts` on GCC's command line, which makes `__has_feature(modules)` true,
  which sends Apple's `<cstring>` down a Clang-only path and leaves `rsize_t`
  undeclared. The root `CMakeLists.txt` sets `CMAKE_CXX_SCAN_FOR_MODULES OFF` for
  this reason. Turn it back on only when something here actually becomes a module.

**The goldens are compiler-independent**, and this has now been checked rather than
assumed: all 87 tests, the seven golden cases included, pass built by GCC at
`Release`. That is what `-ffp-contract=off` and the single documented `double` in
`FixedDiv2` were supposed to buy.

Also worth knowing, since it is easy to develop for a week without noticing: **CI
builds `Release` and the local instructions above build `Debug`.** The goldens hold
across both (checked), as they must — the simulation is integer arithmetic plus one
documented `double` operation — but run `Release` once before trusting a green
`Debug` on anything that touches optimisation-sensitive code.

This is not tidiness. The refactor's only real behaviour bug — `thintriangle_guy`,
the shape the automap draws every *thing* with, silently collapsed to a point when
`fixed_t` became a strong type and `-.5 * FRACUNIT` started converting `-.5` to
`int` 0 — was named by the compiler in plain language in **every single build**
("implicit conversion from 'double' to 'int' changes value from -0.5 to 0") and
went unread for months because 81 other warnings looked like scenery. The goldens could
not see it: the shape is drawn only under the IDDT cheat, which no demo and no
test script uses.

The general form, worth carrying into any strong-type work: when a raw arithmetic
type becomes a strong one, the sites needing an audit are not only the ones that
fail to compile — they are every site where a *literal of another type* met the old
one. Those compile, run, and warn in a way that is easy to dismiss.

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
- **`slopeDiv` gives up on any denominator under 512** and answers
  `Doom::slopeRange`, so `slopeDiv(0, 1)` is 2048, not 0.
- **`pointToAngle2` lands one unit below the exact cardinal** — due north is
  `Doom::ang90 - 1` — inheriting the same half-bucket offset. (Due south is exact,
  being reached by negating rather than by a lookup.)
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
  keeps that true by giving each lump a **256-byte** zero tail, so the over-read
  still happens but draws a deterministic zero everywhere. Do not "fix" the
  over-read in the renderer — it is a visible 1993 behaviour, and the frame goldens
  are recorded with it.

  **It was 64, and 64 was not enough — this is the worked example of why a bound
  should be read off the code rather than sampled.** The comment said the over-read
  was "under that, measured". It is not: `drawColumn` and `drawColumnLow` index
  `dc_source` with `frac.toInt() & 127`, so 128 is reachable *by inspection*, and
  `drawTranslatedColumn` does not mask at all (a patch post's length is a byte, so
  255). The measurement had been taken on macOS, where the bytes past the tail
  happened to be zero anyway — so the frame goldens recorded there were **correct**;
  they simply could not distinguish the guard from the allocator's luck. On Windows
  arm64 the same over-read found non-zero heap, and demo1 at tic 5328 and demo2 at
  tic 392 drew differently. Raising the tail to 256 made all 87 tests pass on both
  Windows toolchains **with no golden re-recorded**, which is the proof that the
  goldens were right and the guard was wrong.
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

- **Scenario tests — started, not finished.** The pattern (load a level, place
  mobjs, run tics, assert) is live in `Tests/Sim/ScenarioTests.cpp`, and the
  probe surface this entry used to call the missing piece has landed with it
  (`doomSimSpawnMobj`, `doomSimCheckPosition`, `doomSimTryMove`,
  `doomSimSetThingPosition`, `doomSimThingsInBlockOf`, …). Four cases run today,
  covering `Doom::tryMove`/`Doom::checkPosition`, a solid thing blocking a spot,
  `MF_NOCLIP` bypassing it, and blockmap linking.

  **Still uncovered, and the reason to keep writing these**: `P_DamageMobj`,
  `P_CheckSight`, and the door and lift specials. Write them per-subsystem *as*
  you refactor it — they are how you get locality on code the demos only cover
  in aggregate.
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
- The engine is single-threaded: `Doom::initGame`, `Doom::updateGame`,
  `Doom::framebuffer` and all input calls happen on the main thread. Audio,
  once wired, is the only exception — it is pulled from the audio callback, on
  another thread, and must take a lock against `Doom::updateGame`.

### What the engine expects of its host

Two of these are not obvious, and getting either wrong makes the game feel
broken rather than fail outright.

- **Audio, when it is wired** (nothing here plays a sound yet — gap-log item 1).
  This is what the deleted SDL example demonstrated and nothing else in the
  repository records. Sound is a **pull** model: run an output stream at
  `Doom::DOOM_SAMPLERATE` (11025 Hz), 16-bit stereo, 512 samples — 2,048 bytes a
  buffer — and call `Doom::soundBuffer()` from the audio callback, taking the
  engine lock around it, because `Doom::updateGame` is on another thread. Music is
  a **push** model: a 140 Hz timer (`Doom::DOOM_MIDI_RATE`) draining
  `Doom::tickMidi()` into a synth for as long as it keeps returning messages.
  Resample if your device wants another rate; the engine only ever produces that
  one.

- **The keys the app asks for do not stick by themselves.** DOOM cannot rebind a
  key from inside the game, yet it still writes every binding out to `~/.doomrc`
  and, at startup, reads them back *over* whatever `Doom::setDefaultInt` asked
  for. A config left behind by an older build therefore pins that build's keys
  for good, and changing the binding in `Main.cpp` silently does nothing at all —
  the game keeps the old key with no sign anything was ignored. `Main.cpp` calls
  `eacpDoomBindKeys()` after `Doom::initGame` to apply them again once the config has
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
   `Doom::soundBuffer()`); music needs a 140 Hz (`Doom::DOOM_MIDI_RATE`) tick
   draining `Doom::tickMidi()` into a synth. Both are unwired; the game runs
   silent.
2. **Modifier keys produce no key events**, on any platform — DOOM binds them as
   ordinary keys (Ctrl = fire, Shift = run, Alt = strafe). Workaround: they are
   diffed once per frame from polled `Window::getModifiers()` into synthetic
   down/up events. (The punctuation half of this entry has been **fixed
   upstream**: `Graphics::KeyCode` now carries `Minus`, `Equals`, the brackets,
   `Semicolon`, `Quote`, `Comma`, `Period`, `Slash`, `Backslash` and `Grave`, and
   `Input.h` maps them from the KeyCode rather than from a character.)
2b. **`charactersIgnoringModifiers` is macOS-only**, and `characters` is filled on
   key *down* alone. Only `Keyboard-macOS.mm` assigns either;
   `CompositionHostWindow-Windows.cpp`'s `dispatchKeyEvent` sets `keyCode`,
   `characters` (down only) and the modifiers, and leaves
   `charactersIgnoringModifiers` empty always.

   This is the bug that made the first Windows build look like it had *partial*
   keyboard support. `Input.h` read the character first and the KeyCode only for
   the handful of keys in its switch, so Space, Escape, Enter, Tab and the arrows
   worked while **every letter, digit and punctuation key returned
   `DOOM_KEY_UNKNOWN`** and `View::keyDown` dropped it — w/a/s/d did nothing, and
   neither did the weapon-select number row.

   Workaround: `Input.h` now maps the printable keys from the positional
   `KeyCode`, with the character kept only as a last resort for keys eacp has no
   constant for. **Positional has to win even where both are available**, and the
   reason is key *up* rather than layout: with characters reported on key down
   alone, a character-first mapping identifies a key one way when pressed and
   another when released, so on any layout where those disagree the release never
   matches the press and the engine never clears the key. Down and up must resolve
   identically, which only the KeyCode does on both platforms.
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
10. **`-fno-gnu-unique` is added for every language.** eacp's top-level CMake
    adds it via `add_compile_options` when the CXX compiler is GCC, but the
    option lands on all languages — including the OBJCXX its Apple platform
    files compile, and OBJCXX on macOS is always Apple Clang, which rejects the
    GCC-only flag as an unknown-argument *error*. A macOS GCC build (the
    second-compiler gate above) then dies inside `eacp-core` before any engine
    code compiles. Workaround: the root `CMakeLists.txt` rewrites the flag to
    `$<$<COMPILE_LANGUAGE:CXX>:-fno-gnu-unique>` on every target under eacp's
    directory tree after `CPMAddPackage` — on the targets, not the directories,
    because a target snapshots its directory's `COMPILE_OPTIONS` at creation.
    The proper fix in eacp is to scope the `add_compile_options` call the same
    way.
11. **eacp binds the uniform buffer to both stages**, so Metal's validation layer
    (on by default in Xcode schemes) logs an "unused binding in encoder at Buffer
    index 16" warning for every pass whose vertex or fragment function declares no
    uniform parameter — benign, but it is what Xcode's runtime-issues panel fills
    up with. The emitter already computes `vertexUsesUniforms(graph)`, so gating
    the bind on it would settle it. (Found alongside a real bug this entry used to
    describe, since **fixed in eacp**: the packed uniform block stopped at the last
    member's end where MSL pads `sizeof(Uniforms)` up to the widest member's
    alignment, so a block ending off-boundary bound 4 bytes short — invisible
    everywhere except under the validation layer, which aborted the first such
    draw the moment demo1 entered the level. `ShaderUploadVisitor::finish` now
    pads the block, and `GPU/shaderProgramPadsUniformBlock` pins it.)

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
