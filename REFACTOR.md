# The C++ refactor

> ## The goal
>
> **Every single source file in `src/DOOM` is clean, modern, RAII C++ in eacp's
> style — and the simulation behaves *exactly* as it always has.**
>
> Not "it compiles as C++." Every file reads as C++ someone *wrote*: RAII ownership
> end to end (no raw `malloc`/`free`, no `new`/`delete`, no zone — resources freed by
> destructors), eacp's containers and types (`EA::Vector`/`EA::Array`, `OwningPointer`),
> real classes with methods, no loose globals, no shim/alias layer, no 1993 C idiom —
> save the handful of quirks that are load-bearing and pinned by a test. A file is
> **done** only when it clears that bar; the refactor is **finished** only when *every
> file does.* The demo/frame goldens hold the "behaves exactly" half at every step.

`src/DOOM` is 43,889 lines of 1993 C across 62 `.c` files: ~684 global data
symbols, ~1,534 file-scope statics, a 12 MB zone arena that is never handed
back, and warnings disabled wholesale (`-w`). This fork owns that code, and this
document is the plan for rewriting **the whole of it** into the C++ above —
playsim, renderer, WAD, UI, game loop and host boundary alike — **without changing
what the simulation does.**

Four principles frame the work:

- **The whole engine goes.** No permanent C/C++ seam; every `.c` becomes real
  C++, not C a compiler now accepts.
- **It becomes modern C++, not transcribed C — and RAII owns everything.** Every
  resource is held by an object that releases it in a destructor, not by a manual
  `doom_free`, a `Z_FreeTags` or a bare `new`/`malloc`: the WAD's lumps and file
  handles, the level's geometry, the mobjs and thinker specials, the renderer's
  tables and scratch, the host's buffers. State lives in real types with methods;
  the containers are eacp's `EA::Vector`/`EA::Array`; a pointer that *owns* is an
  `OwningPointer`, a pointer that merely *refers* stays raw. A file is not *done*
  when it compiles under C++ — it is done when it reads as C++ someone wrote. That
  bar has three parts, and a file clears it only on all three: (1) it is rewritten
  out of vanilla C into `namespace Doom` (the flat list getting shorter); (2) its
  resources are RAII-owned and its idioms modern (the sweep across the code already
  in `namespace Doom`); and (3) it reaches state through owners, not the
  reference-alias *shim* layer — so the shim `.cpp`s and the vanilla-name aliases
  are themselves retired, not left as a permanent seam. **Every** file, including
  the headers, ends up on the far side of all three.
- **The globals go with it.** The ~684 loose globals become members of one
  `Engine` object — one face of the same RAII goal, state with an owner. A
  *consequence* is that the engine can eventually be **constructed** rather than
  booted, so a test owns its world outright; but the practical form of that — a
  fresh world per scenario — is already delivered by level reload (Step 4), so the
  fully-constructible engine is a nicety the modernization arrives at, not the
  point of it. (Scenario tests — *load MAP01, place an imp, run 20 tics, assert* —
  need only the level reset, which the engine has today: it runs many scenarios
  per process. See Step 4.)
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
| 4 | Ownership: kill the zone allocator | **done** — `z_zone.cpp`/`z_zone.h` deleted. Mobjs and thinker specials moved to a level-scoped malloc pool (`Sim/Tick`'s `levelAlloc`/`freeLevelAllocations`); the renderer's boot-once `PU_STATIC` and the scratch buffers to `doom_malloc`; the WAD and `Level` geometry already owned theirs. The vestigial `PU_*` tags `W_CacheLumpNum` ignores moved to `w_wad.h`. All goldens byte-identical throughout |
| 5 | The `Engine` object: globals become members | **done, bar a short named tail** — composition root owns `Random`/`WadFile`/`Level`/`Clip`/`ViewPoint`; `Clip` holds all of p_maputl's + p_map's movement/collision scratch (blockmap descriptor on `Level`, intercept list, opening window + trace, the `tm*` clipping state, the aim's `linetarget` and shot's `attackrange`); `ViewPoint` holds the renderer's camera (`viewx`/…/`viewplayer`), `ViewProjection` its screen projection (`centerx`/…/`projection`, the `viewangletox`/`xtoviewangle` tables), `ViewWindow` the view's on-screen geometry (`viewwidth`/…/`viewwindowy`), `Lighting` its light selection (`fixedcolormap`/`extralight`, the `scalelight`/`zlight` tables) and `GraphicsData` its loaded WAD graphics (`textures`/`colormaps`/`sprites`/…, the R_InitData tables) and `RenderScratch` its per-frame BSP scratch (`rw_*`/`sscount`/`floorplane`/`ceilingplane`) — the renderer's own state, now fully off the loose globals; the game state has followed — **all three headers (`doomstat.h`, `r_state.h`, `p_local.h`) are now nearly empty of loose globals**, and the same pattern has reached the other headers too — ~22 cohesive clusters migrated: from `doomstat.h`, `LevelStats`/`LaunchOptions`/`GameVersion`/`GameSession`/`StartupDefaults`/`PlayerState`/`GameFlow`/`DemoState`/`RefreshFlags`/`OverlayState`/`NetState`/`MapSpawns`/`GameClock`/`AmmoLimits`/`IntermissionInfo`/`SkyState`/`CorpseQueue`; from `p_local.h`, `ItemRespawnQueue` and `clipammo` (folded into `AmmoLimits`); from `d_event`/`d_main`, `EventQueue`/`AttractMode` and `gameaction` (folded into `GameFlow`); from `p_spec`, `ActiveSpecials`/`EndLevelTimer` — `r_state.h` was already done (its externs are geometry views onto `Level`). Three vestigial globals were *deleted* rather than migrated (`viewangleoffset`, `linecount`, `loopcount` — all always-zero or dead). What is left loose is the config-backed set (`snd_*Volume`/`mouseSensitivity`, blocked by `Config.cpp`'s static address capture until the config rework — proven, a naive migration segfaulted every test), `thinkercap` (**since migrated** as `Doom::ThinkerList`, the `Thinker` rewrite it waited on having landed), and a short tail of single scalars (`basedefault`, config-blocked; `save_p`/`savebuffer`/`savename` **since migrated** as `Doom::SaveGameState`). The lone engine-global scalar `validcount` — owned by no subsystem — has moved in as a `Doom::ValidCount` `Engine` member, and DoomMain's boot-path string globals are off the cloud (`wadfiles[]`/`title[]` made file-local `static`, the dead `wadfile[]`/`mapdir[]` deleted). The **file-scope-statics sweep — the last Step-5 phase — is well advanced** — g_game's per-tic input (`TiccmdInput`), its demo buffer (folded into `DemoState`), its deferred new-game params (`DeferredNewGame`), its `consistancy` array (folded into `NetState`), its par-time tables (`ParTimes`), its movement-speed tables (`MovementSpeeds`), the `-timedemo` benchmark (`TimeDemo`) and the pending-command flags (`PendingCommands`) are the first Game-local internal state into the `Engine`, and the sweep has since reached the UI's *file-local* `static`s, **emptying `UI/Hud` and `UI/StatusBar` of data statics**: the HUD (`HudMessage`/`HudChat`/`HudState`) and the status bar (`StatusBarFace`/`StatusBarWidgets`/`StatusBarGraphics`/`StatusBarState`), then the automap's own view state (`AutomapView`), the intermission's residual state (`IntermissionState`), the finale's runtime state (`FinaleState`), the melt's scratch framebuffers (`WipeState`) and the menu's transient interaction state (`MenuState`), and then the renderer's file-local scratch (all of Render: `CompositeCache`/`WallScratch`/`SpriteScratch`/`DrawTables`/`SolidSegs`/`PlaneScratch`/`RenderMainState`) and most of the playsim's (`ActionScratch`/`WeaponScratch`/`SightScratch`/`EnemyAI`/`SwitchList`/`PlayerScratch`/`AnimatedSurfaces`/`LevelPool`), plus doomstat's internal-parameter scalars (`EngineParams`), with a `StateClusterTests` net pinning the golden-neutral tables. **The config-backed category is now complete** — the `doom_config`→`Host` rework's config half landed as a runtime bind: `Config.cpp`'s `defaults[]` no longer captures `&member` at static-init (the race that segfaulted every test) but binds each config-backed entry to its `Engine` member at runtime (`bindEngineDefaults`, called before `mLoadDefaults`/`mSaveDefaults` touch a `location`), and the app reaches them the same way, through `defaults[].location`. On that, the config-backed globals migrated in: `SoundSettings` (`snd_SfxVolume`/`snd_MusicVolume`/`numChannels`), `MenuSettings` (`mouseSensitivity`/`showMessages`/`detailLevel`/`screenblocks`/`usegamma`), `ConfigPaths` (`basedefault`/`defaultfile` — never actually captured), and `InputConfig` (all 22 key/mouse/joy bindings + the device enables + the crosshair/always-run toggles). **And the renderer's cross-read view-globals are now swept** — the state the flat `r_*.cpp` shims still owned and exported through `r_bsp.h`/`r_draw.h`/`r_segs.h`/`r_things.h`/`r_plane.h`/`r_sky.h`: `SkyState` gained `skytexture`/`skytexturemid`, and new clusters landed for `BSPScratch` (the BSP-walk pointers + drawseg pool), `SegState` (the wall-segment texture/mark/light state), `SpriteState` (the vissprite pool + sprite clip window), `DrawState` (the dc_*/ds_* drawer inputs), `VideoState` (`dirtybox`), plus `PlaneScratch` extended with the clip/projection arrays and the dead `floorfunc`/`ceilingfunc_t` deleted; `HudFlags` took the cross-read `chat_on`/`message_dontfuckwithme`. The load-bearing trap throughout: a bare `extern int X;` (not via the header) that *writes* `X` clobbers the low half of the reference's pointer — every cross-file extern must move to `extern T&` in lockstep (caught once as the `0x100640000` `skytexturemid` fault). **What is actually left, counted rather than estimated**: of the 84 `extern` variable declarations remaining in `src/DOOM`'s headers, all but a handful are *deliberate* and documented above — the 13 host callbacks, the `const` trig/random table views, the generated data tables (`states`/`mobjinfo`/`S_sfx`/`weaponinfo`/`defaults`/`endmsg`), the pointer-and-count views onto `Level` (including the blockmap ones, which are views onto `Level::blockmap`) and `GraphicsData`, `screens[]`, the `UI/AutomapTypes.h` and `UI/Wipe.h` carve-outs the eacp compositor reads, and `myargc`/`myargv`. The genuine remainder is `UI/Menu.h`'s `messageToPrint` and `Host/Platform.h`'s `error_buf`/`doom_flags` (host state, arguably already where it belongs). The row's `~684 globals` framing describes where this step *started*, not what is left — worth saying, because the old **in progress** made a nearly finished step read as an open front. |
| 6 | The playsim | **done** — **every** `p_*.cpp` is now a shim over a `namespace Doom` `Sim/` unit: the actor core (`MapUtil`/`Movement`/`MapAction`/`Sight`/`Interaction`/`Player`/`Mobj`/`Weapon`/`Enemy`), the specials (`Lights`/`Plats`/`Ceilings`/`Floors`/`Doors`/`Switches`/`Teleport`/`Specials`), `Tick`, `Setup` and `SaveGame`. The `thinker_t` function-pointer union is kept — the `T_*`/`P_MobjThinker` addresses stay global shims so p_saveg's pointer-identity serialisation is untouched — and virtualising it into a real `Thinker` with a virtual `tick()` has since **landed in Step 8** |
| 7 | The renderer | **done** — all 8: `r_sky`→`Sky`, `r_data`→`Data`, `r_main`→`Main`, `r_plane`→`Planes`, `r_bsp`→`BSP`, `r_segs`→`Segs`, `r_things`→`Things`, `r_draw`→`Draw`, all holding the frame goldens byte-identical and the app linking |
| 8 | UI, game loop, host boundary; `thinker_t`→`Thinker`; delete the zone | **done, bar audio (externally blocked)** — UI (menu included), game loop and utils done: `f_wipe`→`UI/Wipe`, `hu_lib`→`UI/HudWidgets`, `st_lib`→`UI/StatusWidgets`, `hu_stuff`→`UI/Hud`, `st_stuff`→`UI/StatusBar`, `f_finale`→`UI/Finale`, `am_map`→`UI/Automap`, `wi_stuff`→`UI/Intermission`, `m_cheat`→`UI/Cheat`, `m_menu`→`UI/Menu` (behind a new frame golden built for it first); `g_game`→`Game/Game`, `d_main`→`Game/DoomMain`, `d_net`→`Game/Net`, `m_argv`→`Game/Args`, `m_misc`→`Game/Config`, `s_sound`→`Game/Sound`; `v_video`→`Render/Video`. **The host boundary is now complete**: `i_video`→`Host/Video`, `i_system`→`Host/System`, `i_sound`→`Host/Sound`, `i_net`→`Host/Net`, and `DOOM.cpp`→`Host/Api` (the public `doom_*` C API — no shim, its `extern "C"` symbols stay global). The small remainders are done (`m_swap`→`Math/Swap.h`, `doomstat`→`Game/State`, `dstrings`' `endmsg` folded into `UI/Menu`, empty `doomdef.cpp` deleted), as are the two ready data tables (`d_items`→`Sim/Items`, `sounds`→`Game/SoundData`). **The p_saveg save/load net is built** (`Tests/Sim/SaveGameTests.cpp` + `doomSimSaveLoadPreservesWorld`), and on it **the zone was deleted** (Step 4 above): mobjs/specials to a level pool, renderer `PU_STATIC`/scratch to `doom_malloc`, `z_zone` gone. The flat vanilla list was down to the shims plus `info.cpp` alone. **The `thinker_t`→`Thinker` virtualisation is now done**: `Doom::Thinker` (`Sim/Thinker.h`) is a real base with a virtual `tick()`/`kind()`, `mobj_t` and the eight specials inherit it, `P_RunThinkers` dispatches virtually, the old function-pointer sentinels became base flags (`removed` = the `-1` sentinel, `stopped` = null/stasis), the ~15 `function.acp1 == P_MobjThinker` identity tests became `kind() == Mobj && !removed`, spawners `placement-new` (the vtable sets up dispatch), and p_saveg keeps its whole-struct memcpy but preserves the vtable pointer across the copy (`unarchiveThinker`). **A load-bearing trap it turned on:** `mobj_t : Thinker` reuses the base's tail padding, placing its first field 4 bytes earlier than a `thinker_t thinker` *member* would — so `degenmobj_t` (a sector's sound origin, cast to `mobj_t*` by the sound code) had to inherit `Thinker` too, or the origin's x/y read from the wrong offset (it silently made a door sound inaudible and dropped one `M_Random`, the whole simulation otherwise bit-identical). **And `info.cpp` — the last real vanilla source — is now migrated too** (`Sim/Info.cpp`, the generated state/mobjinfo/sprite-name tables kept verbatim; the `states[].action` function-pointer *union* retired for a single type-erased pointer the two dispatch sites cast back to the exact signature), so **the flat vanilla list is now *only* the shims**. **The `doom_config`→`Host` fold is now done too** — the 13 host callbacks live in a `Doom::Host` singleton (`Host/Host.h`, `host()`), deliberately separate from `engine()` (embedder-set platform state, not world state); the vanilla names are references onto it, so no call site or `doom_set_*` API changed. Left: audio alone — the engine side is built and it is blocked outside this repository, on an eacp audio stream. (This row read **in progress** while the handoff four lines below it read **8 is done**; the handoff was right. The row had also been counting the globals-into-`Engine` sweep as its own remainder, which is Step 5's work, not Step 8's.) |
| 9 | Modern C++ / RAII across the board — the second half of the goal | **all three strands done; one audio-blocked item** — Steps 4–5 made the state *owned*; this makes it *read like C++ someone wrote*. Three strands: (a) retire the reference-alias layer so every reader reaches state through an owner, which unpins the `Engine`'s address; (b) the RAII sweep over the manual `doom_malloc`/`doom_free` owners; (c) the idiom cleanup over code already in `namespace Doom`. **(c) is done** — `#pragma once`, `typedef struct {…} T;` → `struct T {…};` (106 sites), C arrays → `EA::Array` (**now genuinely all of them** — this row twice claimed it finished while it had not been counted; the category was enumerated at 130 members across 43 headers, 111 converted in three batches and 19 correctly left raw, see item 8), `(void)` parameter lists (326), pointer→reference where provably safe — as is the whole vanilla-name retirement: no prefixed function name and no `_t` type survives, the flat layer is gone, and `fixed_t`/`angle_t` **are** `Doom::Fixed`/`Doom::Angle`. **(a) is done**, after being declared done twice while a whole syntactic tier stood: 247 file-local aliases fell in seven batches, and on that the **`Engine` is constructible** with a test that proves it. `MovementSpeeds`/`VideoState::dirtybox` are `int`; the hitscan traversers return `AimResult`; `UI/Automap` and `UI/Finale` gained the frame goldens they never had. **(b) is now done bar one audio-blocked item**: the eight remaining manual `doom_malloc`/`doom_free` owners are RAII-owned (`UI/Wipe`'s melt offsets, `UI/Intermission`'s `lnames`, `Game/Game`'s save and demo buffers, `Game/Config`'s string defaults, `DoomMain`'s seven WAD paths + `addWadFile`), `readFile`'s `byte**` out-parameter is retired, and the dead `Host/System` zone vestiges are deleted — and the level pool has a destructor now (it was leaking every mobj on `resetEngine()`), leaving only `Host/Sound`'s audio-blocked `paddedsfx` and `findResponseFile`/`myargv` (a real ownership question, documented). `AutomapView::min_w`/`min_h` were investigated against the 1993 source and deleted as an id leftover, not a lost read. **`doom_boolean` is now done too** — all ~288 uses are a real `bool` and the typedef is deleted, in four verified batches (Render 19, Host 4, UI 104, Sim+Game 161), leaving four deliberate `int`s that only look like flags. **The function-like macro layer is now retired too** — `Math/Swap.h`'s `SHORT`/`LONG` across 154 sites became one deduced-width `Doom::littleEndian`, and `UI/Automap`'s seven, `Sim/SaveGame`'s `PADSAVEP` and `UI/CheatTypes`' `SCRAMBLE` became functions; `Host/Net`'s `ntohl`/`ntohs` stay, deliberately, being under an `#if` for a platform no gate here compiles. That sweep found the refactor's **one real behaviour bug** (`thintriangle_guy`, see the traps section) and two more little-endian-invisible defects. **The constant macros followed and are now essentially closed — 629 → 309 across `src/DOOM`**, of which 199 of the remainder is `Game/StringsFrench.h`, which no build here compiles: `Sim/SimDefs.h`, `Sim/SpecialTypes.h`, `Render/Lighting.h`, `UI/Hud.h`, `UI/HudWidgetTypes.h`, `UI/StatusBarTypes.h`, 290 of `Game/StringsEnglish.h`'s 293, then `Game/GameDefs.h`, `UI/AutomapTypes.h`, `Wad/MapFormat.h`, `Game/NetTypes.h`, `UI/StatusWidgetTypes.h`, `Sim/WeaponTypes.h` and every `.cpp`-local pile. **That sweep's largest finding was not about macros**: eleven constants existed twice, once as the vanilla macro and once as a `constexpr` on the owning `Engine` cluster, and in five of them the overflow guard tested one while the array was sized by the other — see item 6. `Math/TrigTables.h` was the same thing at eleven-in-one-file scale and became a deletion rather than a conversion. **The warning count went 81 → 1**, the survivor being the deliberate type-erased-action cast. **See "Where Step 9 actually stands" in the handoff for the authoritative state** — this row is a summary, and the handoff is where the detail and the traps live. |

## Where this is — session handoff

Everything below is committed on branch **`C++Refactor`**; the working tree is
clean and the suite is green (**87 tests**, ~25s: `ctest --test-dir build`).

**Read the `thintriangle_guy` entry under "Traps this work added to the list"
before doing anything else.** It is the refactor's first and so far only real
behaviour bug — a `double * Fixed` that silently multiplied by zero, from the
`fixed_t` → `Doom::Fixed` migration — and both of the reasons it survived are
general: the migration had fixed two of the three sites and left no sign the third
was outstanding, and the compiler had been printing the exact defect in every build
inside an 81-warning haystack. **That haystack is gone** — the engine builds with
exactly one warning, the deliberate one in `Sim/Weapon.cpp`. Note the measurement's
scope before reaching for `-Werror`: that count is Apple Clang on macOS, and CI
builds five configurations including MSVC on a different flag set. See item 7.

**Steps 0–8 are done**, with one externally blocked item: audio, whose engine side
is built and which waits on an eacp audio stream. The whole UI, game loop, netcode,
utility layer and host boundary are migrated, the zone allocator is deleted, the
`thinker_t`→`Thinker` virtualisation has landed, and the flat vanilla sources are
gone — `src/DOOM` has no `.cpp` at top level at all. **Step 9 is the only open
front**, and within it strand (c) and strand (a) are finished, strand (b) is down to
the one audio-blocked owner. Step 5 keeps a short named tail (see its row).

**The preprocessor and the warning count were the open front, and both are now
largely closed** (items 6 and 7 under "What is left"). The function-like macros are
retired; the string table is converted; the warning count went 81 → 1, the survivor
being deliberate. The constant macros went **629 → 312**, and 199 of that remainder is
`Game/StringsFrench.h`, which no build here compiles — every header named as open in
the previous draft of this paragraph is done, as are all the `.cpp` piles.

**Item 8 is done**: 130 members enumerated, 19 that must stay raw, **111 converted** in
three batches (UI 35, Render 29, Game+Sim+Wad 47). Its hazard list was wrong in both
directions and is corrected there — as written it would have led someone to convert
`Patch::columnofs`, a flexible array over raw lump bytes, while carefully avoiding two
headers that were never at risk.

**With that, Step 9's three strands are all done**, and what remains of the whole
refactor is the short, named, mostly-deliberate tail: audio (blocked outside this
repository), `Host/Sound`'s `paddedsfx` and `findResponseFile`/`myargv` behind it, the
~55 dead-in-both-eras macros whose deletion is a judgement call reserved for a human,
the six runtime-accessor macros that want to be inline functions, and item 7's
prerequisite — measuring the warning count on the four CI configurations this
repository has never measured, before anyone reaches for `-Werror`.

**Item 6 is now essentially closed.** The one- and two-macro tail went in a single
batch (17 names, `326 → 309`), and what remains is deliberate rather than pending:
`Game/StringsFrench.h`'s 199 that no build here compiles, the ~55 dead-in-both-eras
macros whose *deletion* is a judgement call reserved for a human, the five
concatenation families that cannot leave the preprocessor at all, and six object-like
macros whose bodies call a runtime accessor and therefore have no `constexpr` available
(they want to be inline functions — call-site churn, a separate change, unclaimed).
**Item 8, the 128 C arrays, is the open one**, and part of it is deliberately closed.

Two things that batch turned up are worth reading before the next sweep of any kind:
`FRACBITS` was a **thirteenth** duplicate constant hiding in plain sight next to
`Doom::fracBits`, and a work list built by grepping for "files with one or two macros
left" silently overlapped the pile this document deliberately leaves alone — producing
ten `[[maybe_unused]] constexpr`s whose annotation was itself the evidence they should
not have been converted. Both are under item 6.

**The macro work's largest finding was not about macros.** Eleven constants were
defined twice — once as the vanilla macro, once as a `constexpr` on the `Engine`
cluster that owns the array — and in five of those the overflow guard tested the macro
while the array was sized by the constant, with no compile-time relationship between
them. Read that entry under item 6 before doing any more of this kind of sweep: it
includes the one-line grep that enumerates the category, and the fact that this sweep
*created* a twelfth instance before that grep was written.

None of it is hard, and all of it is the kind of work under which a
`thintriangle_guy` hides. Do it with the four gates, and **read the warnings the
build prints** — at one warning, a second one is now a signal rather than noise,
which is the whole return on clearing them.

*Until this session the table said Step 8 was "in progress" while this paragraph
said "8 is done", four lines apart. Both were edited by hand at different times.
Where a status appears twice, expect it to disagree eventually — the table is now
the one place a step's state is written, and this paragraph summarises it.*

Build the tests with `-DPUREDOOM_BUILD_EACP_EXAMPLE=OFF` for a fast engine-only
loop — but **that configuration never compiles `examples/EACP`**, and
`EngineAccess.cpp` reaches into engine headers directly, so a refactor can break
the app without a single test noticing. Keep a second build directory
(`cmake -B build-app -DPUREDOOM_BUILD_TESTS=OFF -DCPM_eacp_SOURCE=$HOME/Code/eacp`)
and build `PureDoomEACP` from it as a fourth gate, alongside build, tests and goldens.

**The golden set is ten files, recorded by seven cases.** The files are
`demo1/2/3.hashes`, `demo1/2/3.frames`, `doom1.lumps`, `menu.frames`,
`automap.frames` and `finale.frames`; the cases are the seven in
`Tests/CMakeLists.txt`'s `record-goldens` list (`demo1 demo2 demo3 wadDirectory
menu automap finale`). Older sections below say "all four goldens"; that was true
when written. The rule is unchanged and applies to every one of them: a `*.hashes`
golden is **never** re-recorded, and a `*.frames` golden only for a change whose
moved pixels are provably not part of any lump.

*This line previously read "the golden set is now six, not four", which was wrong
under every reading — six is neither the file count (10) nor the case count (7);
it was the file count at some earlier moment, carried forward unchecked. It is
noted rather than quietly fixed because the number had since been copied into
commit messages ("all six goldens byte-identical") as though it had been verified.
A count in prose is exactly the kind of claim that rots silently, and `ls
Tests/Goldens/` settles it in one command.*

### Where Step 9 actually stands

**`src/DOOM` has no flat layer left.** It is eight subsystem directories —
`Math/ Wad/ Sim/ Render/ UI/ Game/ Host/ Engine/` — plus exactly two files:
`DOOM.h`, the public `extern "C"` interface an embedder includes, and
`doomtype.h`, the `byte` foundation every layer including `Math/` depends on and
which therefore cannot live inside any one subsystem (it carried `doom_boolean`
too until Step 9 retired it; what is left there is `byte`, the `DOOM_MAX*`/`DOOM_MIN*`
limits, and the standing list of declarations that must not become `bool`). It began
this work as 53 flat `.cpp` and 61 flat `.h`.

Concretely, all of the following are **done**:

- **No vanilla-prefixed function name survives** (`R_`/`P_`/`D_`/`G_`/`M_`/`S_`/
  `V_`/`W_`/`I_`/`EV_`/`T_`/`HU_`/`ST_`/`AM_`/`WI_`/`F_`/`HUlib_`/`STlib_`).
  Retiring the shims was only half of it: 134 of the *namespaced targets* still
  carried the prefix as an initialism (`Doom::dDisplay`, `Doom::amTicker`) or
  outright (`Doom::I_Error`), and a further 158 internal functions did too —
  the latter worse than they looked, because an earlier pass had "dropped"
  prefixes by lowercasing and concatenating them (`ST_refreshBackground` →
  `strefreshBackground`), which a grep for `_` reads as clean.
- **All 107 vanilla `_t` types are PascalCase in `namespace Doom`**, and their
  headers are rehomed — `r_defs.h` split by owner into `Sim/MapTypes.h` and
  `Render/RenderTypes.h`, the rest merged into the unit that owns them.
- **`fixed_t` IS `Doom::Fixed` and `angle_t` IS `Doom::Angle`.** These were the
  last two raw typedefs and the only genuinely *semantic* migration in the
  refactor.
- **Blockmap callbacks carry their own context**, so six globals that existed
  only to bridge a call are gone, and `pathTraverse` takes a
  `Doom::FunctionRef` rather than a bare function pointer.
- **Strand (a) is finished — genuinely this time, and verified.** See below; it
  took seven batches and 247 aliases after this document had already declared it
  complete twice.
- **The `Engine` is constructible.** `engine()` is a heap-owned
  `EA::OwningPointer<Engine>`; `resetEngine()` drops the live instance and builds
  a fresh one. `Engine/resetEngineMakesAFreshInstance` proves it — checking both
  that the address changed *and* that the state is clean, since either alone
  passes for the wrong reason. At `-O2` the new `engine()` compiles to a body
  byte-identical to the old function-local static: `Engine` is ~293 KB with ~90
  non-trivial members, so Clang was already lowering that static to a guarded
  heap allocation behind a pointer slot. Naming it cost nothing.

### What was actually re-checked, and how

Four completeness claims in this document have now been retracted — strand (a) twice,
the `Host/System` deletions, and strand (c)'s C arrays. That is enough that a reader
would be right to distrust the rest by default, so the remaining Step 9 claims were
re-verified against the tree rather than left on their reputation. **Each of these is
one command, and each came back clean**, so they can be trusted until something
changes them:

| Claim | Check | Result |
|---|---|---|
| No vanilla-prefixed function name survives | `git grep -nE '\b(R_\|P_\|D_\|G_\|M_\|S_\|V_\|W_\|I_\|EV_\|T_\|AM_\|WI_\|F_\|HUlib_\|STlib_)[a-zA-Z]+\s*\('` | 0 |
| No `_t` type survives bar the three public-API ones | grep `_t` less `doom_key_t`/`doom_button_t`/`doom_seek_t` and the std types | 0 |
| The flat layer is gone | `ls src/DOOM/*.cpp` | none |
| `fixed_t`/`angle_t` **are** the strong types | `grep 'using fixed_t\|using angle_t'` | aliases, as claimed |
| Every header is on `#pragma once` | `git grep -L '#pragma once' -- '**/*.h'` | 0 |
| No `typedef struct` survives | `git grep -c 'typedef struct'` | 0 |
| `(void)` parameter lists retired | `git grep -n '(void)'` | 17 hits, **all `(void) param;` unused-parameter suppressions**, which is a different idiom — the claim is about parameter *lists* and holds |

That last row is worth keeping as a worked example of the opposite failure: the first
count looked like a retraction and was a bad grep. **Read what the hits actually are
before writing the correction** — a false retraction costs the next reader as much as
a false completion, and this document has now made both mistakes.

(~~The 17 `(void) param;` suppressions are themselves a C idiom C++ has better answers
for — omit the parameter name, or `[[maybe_unused]]`. Fourteen are in `UI/Wipe.cpp`,
where a family of stubs shares a function-pointer signature. Small, safe, unclaimed.~~
**Already done, and this entry never noticed.** `UI/Wipe.cpp`, `UI/StatusWidgets.cpp`
and `Host/Sound.cpp` all carry `[[maybe_unused]]` parameters today; `git grep '(void)'`
returns **one** hit, `Host/Sound.cpp:1136`'s `(void) mus_data[mus_offset++];`, which is
a different idiom entirely — discarding an expression's result to advance a cursor, not
suppressing an unused parameter.

**This is the first claim in this document to be wrong in the *other* direction**, and
that is why it is worth keeping rather than deleting. Four earlier retractions were
work called finished that was not; this is work called outstanding that was finished.
Both come from the same habit — writing a status from memory instead of from the tree —
but they fail differently: an overstated completion lets a sweep land under a green
suite that never ran the code, while an *understated* one just wastes the next reader's
afternoon re-doing something. The rule covers both: **a status line is a measurement,
and it is stale the moment it is written down.**)

### Strand (a), and the three times it was called done

This is worth reading before trusting any completeness claim in this document,
because the same mistake was made three times in a row.

**The work itself.** ~290 cross-file aliases went first (`extern T& viewx =
engine().viewPoint.viewx`), then 247 file-local ones. The transform is a
**per-function hoist**, not a token substitution: the accessors are out-of-line
functions in `Engine.cpp`, so rewriting each `dc_x` as `Doom::drawState().dc_x`
adds a call per access inside the per-pixel drawers — a regression **no golden
could catch, since they hash pixels and not time**. Each function takes
`auto& draw = drawState();` once instead.

The 247 fell in seven batches: Render 40, StatusBar+Intermission 90,
Menu+Hud+Wipe 46, Render+Sim remainder 31, Automap 35, Finale 12, MapAction 11.

**Why it kept being declared finished.** Each sweep searched for the *spelling it
had just learned about* and generalized the result to the *category*. Six
distinct forms hid aliases from a search that looked exhaustive:

| # | Form | Why the search missed it |
|---|---|---|
| 1 | `extern T& x = cluster().x;` in a flat `.cpp` | — (the original tier) |
| 2 | the same, inside an already-namespaced file | survey only scanned flat files |
| 3 | `static T& x = cluster().x;` at file scope | searches looked for `extern` |
| 4 | `static Patch* (&tallnum)[10] = …` | reference-to-array; no type-shaped regex admits it |
| 5 | `static std::function<void(int)>& r = …` | the *type* contains parentheses |
| 6 | `T& x = cluster().x;` with no `static` | pattern required `static` |

Two more hid from *any* definition search: an initializer wrapping to the next
line (invisible to a line-oriented count), and a **macro** reading an alias —
`UI/Automap`'s `PUTDOT` read one *per pixel* inside a Bresenham loop, and a macro
is a declaration of nothing.

Form 6 was the dangerous one. Five such aliases in `Sim/Sight.cpp` survived every
sweep and would have made `resetEngine()` actively unsafe — `checkSight` reading
through a dangling reference into a destroyed `Engine`. **Writing the test is
what found them**; a green build had shipped happily over them for months.

**The counting method that finally held**: a brace-depth scan that assumes
nothing about the spelling — walk the file tracking `{}` depth, split on `;`, and
match any statement at depth 0 that binds a reference to `accessor().member`,
where the accessor may be qualified. It finds exactly the 13 deliberate
references onto `Doom::host()` and none onto the `Engine`.

**The lesson, stated once**: verify a completeness claim against the **category**,
not against the pattern that found its members — and prefer a claim a *test* can
fail over one only a grep can support. `Engine.h`'s own comment still described
the address pin as live all the way through, which was the available evidence
that the claim was wrong, sitting in the file the claim was about.

**13 aliases stay, deliberately**: the host callbacks (`doom_print`/`doom_malloc`/…) are
references onto `Doom::host()`, not the Engine. `host()` is a separate immortal singleton
that must *not* be reset with a fresh Engine, so pinning its address blocks nothing —
in fact `resetEngine()` depends on that split — and retiring them would rewrite ~249
call sites to `host().print(...)` for no gain.

### Coverage was not what it looked like

Two files had **no test coverage of any kind**, and the demo goldens' green was
silent about both. Retiring their aliases under that green would have been
honest-looking and meaningless.

- **`UI/Automap.cpp`** — no demo opens the automap. Vanilla's `D_Display` skips
  `R_RenderPlayerView` entirely while the map is up, which is exactly why this
  port had to add `eacpDoomRevealAutomap`. `StateClusterTests` only checked
  `AutomapView{}`'s default field values.
- **`UI/Finale.cpp`** — no demo reaches a finale.

Both now have frame goldens (`automap.frames`, `finale.frames`), built on the
`m_menu` precedent — **recorded against the unmodified file, before the sweep**,
which is the only ordering that proves anything. Each was then shown to be
*sharp* and *non-redundant*: a one-palette-index change to `WALLCOLORS` fails
`Sim/automap` and leaves all six demo goldens green; `TEXTSPEED` 3→4, or the text
crawl's start `y` moved one pixel, fails `Sim/finale` alone.

`FinaleReplay.h` is worth copying from in one respect: the cast call and bunny
scroll are DOOM II / episode-3 only, and rather than *assert in a comment* that
they are unreachable, the test asserts the game mode is shareware — so the
"deliberately uncovered" claim is enforced rather than aspirational.

**The rule this establishes**: before refactoring a file, check what actually
covers it. A suite that is green because it never runs the code is worse than a
red one.

### Traps this work added to the list

- **A `double` scaled by `FRACUNIT` silently became zero under the strong type, and
  the compiler had been saying so all along.** This is the one real *behaviour* bug
  the refactor has produced, and it survived every gate for months.

  `UI/Automap.cpp` builds its vector shapes by scaling literals: vanilla wrote
  `#define R (FRACUNIT)` and `(fixed_t)(-.5 * R)`, which with `fixed_t` an `int` and
  `FRACUNIT` `65536` truncated to `-32768`, exactly as intended. Once `FRACUNIT`
  became a `Doom::Fixed`, `-.5 * R` is `double * Fixed` — and the only viable
  operator takes an `int`, so **`-.5` is converted to `0` before the multiply ever
  happens**. Every scaled vertex of `thintriangle_guy` collapsed to the origin, and
  the shape the automap draws every *thing* with became a single degenerate line
  from `(0,0)` to `(65536,0)`.

  Three things make it worth a full entry:

  - **The neighbouring table was right.** `triangle_guy`, twenty lines above, scales
    off `FRACUNIT.raw` and goes through `amFixed`, and `INITSCALEMTOF` on the next
    page uses `.raw` too. So the migration *did* catch this class — it caught it two
    times out of three and left no sign that the third was outstanding. A partially
    applied fix reads exactly like a completed one.
  - **No golden could see it, by construction.** `drawThings` is gated on
    `cheating == 2` — the IDDT cheat pressed twice — and neither the automap script
    nor any recorded demo cheats. The proof is not the argument, it is the
    measurement: the fix left `automap.frames` **byte-identical**, which is precisely
    what "the golden never ran this code" looks like. `automap.frames` was recorded
    to cover this file and does cover it well; it simply cannot reach a cheat path.
  - **The compiler had already found it.** Every build printed eight
    `-Wliteral-conversion` warnings naming the exact values —
    *"implicit conversion from 'double' to 'int' changes value from -0.5 to 0"* — and
    they were read as noise from "vanilla's C-style casts" and left alone. The engine
    builds with 70-odd warnings under `-Wall -Wextra -Wpedantic`, which is what let a
    warning that named a real bug in plain language sit in the output unexamined.
    **A warning count that is not zero is a place bugs hide.**

  It is fixed, and pinned by `Automap/shapeTablesAreScaled` (`Tests/Sim/AutomapTests.cpp`),
  which asserts the vertex values directly rather than through a picture — a frame
  golden would have had to be re-recorded to cover a cheat path, and the numbers are
  sharper anyway. Shown sharp by restoring the old definition: it fails on the
  vertex values *and* on "no edge of the thin triangle is degenerate".

  **The general rule**: when a raw arithmetic type becomes a strong one, the sites
  that need auditing are not only the ones that fail to compile — they are every site
  where a *literal of another type* met the old one. `Fixed{n}` vs `Fixed::fromInt(n)`
  was already on this list; `double * Fixed` is the same hazard one step further out,
  and it is worse because it compiles, runs, and warns in a way that is easy to
  dismiss.

  **The category has since been swept, which is the part this document keeps failing
  to do.** Every decimal literal in an arithmetic expression anywhere in `src/DOOM`
  or `examples/EACP` was enumerated —

      git grep -nE '[0-9]*\.[0-9]+ *[*/]' -- 'src/DOOM/*' 'examples/EACP/*'

  — and there are nineteen. Fifteen are `UI/Automap.cpp`'s, and every one of them
  now scales `FRACUNIT.raw` or goes through `amFixed`; two are `Math/Angle.h`'s
  degree conversions, which are `double` throughout and cast at the end; two are
  `examples/EACP`'s, which are genuinely floating-point. **`thintriangle_guy` was the
  last instance, and this is a claim a single command re-checks** — which is the bar
  a completeness claim in this file should meet, and mostly has not.

- **`git grep -E` and BSD `sed` accept `\b` and never match it — a sweep run through
  either is a silent no-op that reads as a clean result.** This bit three times in one
  session and once it cost a real miss. `git grep -lE '\bML_[A-Z]+\b' -- Tests examples`
  returned nothing, so a survey concluded there were no `ML_*` readers outside
  `src/DOOM`; there are eight, all in `examples/EACP/EngineAccess.cpp`, and the tests
  stayed **87/87 green while the app did not compile**. `sed -i '' 's/\bX\b/Y/'` on
  macOS is the same hazard with the same signature: exit status 0, file unchanged.

  What makes it worse than an ordinary bad grep is that **the failure and the success
  look identical** — "no matches" is exactly what a finished sweep prints. The
  ANG*→ang* rename ran as a complete no-op and its own leftover check, using the same
  broken pattern, confirmed it was done.

  Use `perl -pi -e` for edits and `grep -rnE` (or a pattern with no `\b`) for surveys,
  and **verify a sweep by what is left in the tree, not by what the sweep reported**.
  The app-link gate is what caught this one, exactly as this document says it will.
- **"Its signature can't change" is a claim to check, not assume.** The RAII sweep
  first bridged `readFile`'s `doom_malloc`'d `byte**` out-parameter into an owner by
  copying and freeing — treating the signature as fixed. It is not: `readFile` has
  **one caller** and no users outside `src/DOOM`, so it simply takes the owner now,
  and the copy and the free both vanish. Count the callers before designing around
  an interface; in an engine this size the answer is often one.
- **RAII sweeps meet pointers that own *sometimes*.** `demobuffer` (owned while
  recording, a borrowed WAD lump on playback) and `SaveGameState::buffer` (owned on
  load, a framebuffer view on save) both look like plain owners at the allocation
  site. Converting either in place would free memory the engine does not own. The
  shape that works: a *separate* owning member, the vanilla name left a raw view —
  which is also just the RAII rule already stated, that a pointer which merely
  refers stays raw. Check *every* assignment to a pointer before making it an owner,
  not the one you are standing on.
- **An owner-of-owners invalidates views on growth.** Where an inner buffer's
  `.data()` is published to something that outlives the call (`defaults[].text_location`,
  `wadfiles[]`), the outer vector must be sized or reserved *before* any inner one is
  filled. Otherwise filling entry N reallocates and silently dangles the pointer
  entry 0 handed out.
- **`Clip::attackrange` looks like a traverse result and is an input.** It is set
  by `lineAttack` *before* the traverse, and `Sim/Mobj`'s `spawnPuff` reads it
  from *inside* that traversal's own call stack — nested, not stale — to pick the
  fist's `S_PUFF3`. It must **not** move into `AimResult`. It also carries a
  genuine cross-tic leak that must be preserved: `spawnPuff` has a third caller in
  no hitscan chain at all — the revenant's homing rocket spawns smoke every 4th
  tic (`Sim/Enemy`'s `tracer`) — which reads whatever the last hitscan left, so a
  recent punch can flip that smoke to `S_PUFF3`. That is vanilla behaviour sitting
  under the demo goldens; threading an explicit "no range" through the tracer is a
  behaviour change wearing a refactor's clothes.
- **`aimLineAttack`'s slope of 0 is ambiguous with a genuine horizontal shot.**
  Callers test the *target*, never the slope, to decide whether anything was
  found. `AimResult::target` preserves that exactly; "improving" a caller to test
  `slope != 0` would be wrong in a way no compiler would mention.
- **The app-link gate is invisible to the test configuration.** `examples/EACP`
  is not built with `-DPUREDOOM_BUILD_EACP_EXAMPLE=OFF`, and `EngineAccess.cpp`
  reaches into engine headers. Several names it reads (`m_x`, `scale_mtof`,
  `f_w`, `wipe_melt_offsets`) sit in files this work swept; they survived only
  because they are a *deliberate exported carve-out* declared in
  `AutomapTypes.h`, not aliases. Check the app builds; do not infer it.
- **Retiring an alias is a good way to find state nothing reads.** Eight members
  were deleted outright once the alias in front of them was gone:
  `CompositeCache`'s `firstpatch`/`lastpatch`/`numpatches`, `MenuState`'s
  `messx`/`messy`, `PlaneScratch::spanstop`, and `WeaponScratch`'s
  `swingx`/`swingy` — the last a genuine 1993 leftover, declared in vanilla's
  `p_pspr.c` and unused there too. **The bar is zero reads *and* zero writes.**
  A member that is written and never read is a *different* finding — possibly a
  lost read — and wants investigating, not deleting: see `AutomapView`'s
  `min_w`/`min_h` under "What is left".
- **A reader-count heuristic lies after a hoist.** Agents legitimately name the hoisted
  local after the alias it replaces, so a bare-name count reports those locals as readers
  (`Net.cpp`'s local `debugfile` copy alone showed 70). Delete the definitions and let the
  **compiler** name the survivors; it does not confuse a local with a global.
- **`extern` declarations are not only in headers.** Several sat in `.cpp` files
  (`Host/System.cpp`, `Host/Api.cpp`, `Render/Main.cpp`, `UI/Menu.cpp`) and three inside
  function bodies. A header-only sweep leaves dangling declarations.
- **Names collide across clusters.** `UI/Menu.cpp`'s `mousex`/`mousey` are references into
  `Doom::MenuState`, *not* the identically-named `ticcmdInput` aliases; `forwardmove`/
  `sidemove`/`angleturn` in the playsim are `cmd->` `Ticcmd` members; `linedef` is a
  parameter in `Sim/MapUtil.cpp` and a local `int` in `Sim/Setup.cpp`. Substituting by name
  would have broken all of them.
- **A test can assert the very thing being removed.** `Random/vanillaNamesAliasTheObject`
  existed to check `&prndindex == &randomness().playIndex`. Converting it mechanically
  yields `&x == &x` — green forever, testing nothing. It was deleted, with a comment
  recording where its surviving coverage lives.
### What is left

Everything above the line is done. What follows is, in the order worth doing it:

1. **The RAII tail — now down to two blocked items.** ~~Six more~~ *(landed this
   session, each verified against build + 82/82 + byte-identical goldens + the app
   link)*: `UI/Wipe`'s melt-offset table and `UI/Intermission`'s `lnames`,
   `Game/Game`'s save and demo buffers, `Game/Config`'s per-string-default
   `newstring`, and the **seven never-freed WAD path strings in
   `DoomMain::identifyVersion`** plus `addWadFile`'s per-file copy. Notes worth
   keeping:
   - **Two of them are dual-ownership and must stay raw views.** `DemoState`'s
     `demobuffer` is `doom_malloc`'d when *recording* but points straight at a
     `cacheLumpName` lump when *playing back* — making it an owner would free WAD
     memory on every demo playback. `SaveGameState`'s `buffer` is the same shape:
     owned bytes on load, `screens[1] + 0x4000` on save. Each grew a *separate*
     owning member (`demoRecordBuffer`, `loadStorage`) with the vanilla name left a
     raw view, per the RAII rule that a pointer which merely refers stays raw.
   - **`readFile`'s `byte**` out-parameter is gone**: it took a `doom_malloc`'d
     block the caller had to free by hand, and it has exactly one caller and no
     users outside `src/DOOM`, so it now fills an `EA::Vector<byte>&` directly. That
     also retired a manual `doom_free` that an early `return` on a bad savegame
     version had been skipping. **It had no coverage** — `doomSimSaveLoadPreservesWorld`
     drives `save.cursor` against its own scratch and never reaches the file layer —
     so `Sim/readFileFillsItsOwner` was added with it (`Tests/Sim/SaveGameTests.cpp`,
     via `doomSimReadFileIntoOwner`): it reads the IWAD and requires the owner back
     sized to exactly the length returned, holding the real bytes. Shown sharp —
     sizing the owner one byte short fails it on that assertion alone.
   - **The owner-of-owners cases reserve up front.** `Config`'s string defaults and
     `DoomMain`'s `wadfiles[]` both hand out `.data()` pointers that outlive the
     call, so the outer vector is sized/reserved once before any inner one is
     filled — growth would otherwise invalidate a pointer already published.

   **The level pool is now done too, and it was hiding a real leak.** `Sim/LevelPool`
   was a bare `{ LevelChunk* head; }` with no destructor: `loadLevel` calls
   `freeLevelAllocations` on *reload*, but nothing did on *teardown*, so destroying an
   `Engine` with a level loaded leaked every mobj and thinker special on the list.
   Harmless while the Engine outlived the process — and **made reachable by strand (a)**,
   which is the point worth carrying: making the `Engine` constructible turned a
   latent ownership gap into a live leak, and nothing failed. `resetEngine()` returned
   **none** of the 120 blocks one E1M1 load takes.

   It is fixed by a destructor, not a container, and the distinction is the whole
   lesson of this item. The blocks are variable-sized, hold polymorphic `Thinker`s
   whose addresses `Sim/SaveGame` serialises and the thinker list threads, and so can
   never be moved or relocated — an `EA::Vector` is not available at any price.
   **RAII here means owning the release, not owning the layout**, which is a shape
   worth recognising rather than treating as a failure to convert. `LevelPool` now has
   `releaseAll()`, a `~LevelPool()` that calls it, and deleted copy operations (raw
   ownership, so a copy would double-free); `freeLevelAllocations` delegates to the
   same routine, so reload and teardown cannot drift apart.

   **It needed a new kind of test, because no golden can see a leak.** The goldens hash
   the world and the picture, and leaked memory changes neither until the process runs
   out. `Tests/Sim/OwnershipTests.cpp` installs a counting `doom_malloc`/`doom_free`
   through the public `doom_set_malloc` (probe: `doomSimCountAllocations` /
   `doomSimLiveAllocations`) and asserts that live blocks after `resetEngine()` fall
   back to the post-boot figure. It is sharp *because* the RAII sweep already
   succeeded elsewhere: nearly everything else is an `EA::Vector` allocating through
   `operator new`, so `doom_malloc` is now almost exclusively the pool's own allocator.
   Shown sharp by gutting the destructor — it fails on exactly that assertion.

   One trap it cost, recorded because it produces a *green* mistake: the test first
   went into `Sim/EngineTests.cpp`, which builds into **`PrimitiveTests`**, and that
   binary takes NanoTest's default `main`. Only `SimTests` links `Tests/TestMain.cpp`,
   which sets `DOOMWADDIR` from `PUREDOOM_ROOT_DIR`. A booting test in `PrimitiveTests`
   therefore **passes when the binary is run from the repository root by hand and fails
   under ctest**, which runs it from elsewhere — the opposite of the usual failure, and
   easy to misread as flakiness. Booting tests go in `SimTests`.

   **What is left, one genuinely blocked item**: `Host/Sound`'s `paddedsfx` (a
   `+8`-offset per-sfx cache, audio-blocked and therefore golden-blind). Plus one
   *documented* non-item: `DoomMain::findResponseFile`'s buffer and its `myargv`
   reallocation.
   The response file's bytes are tokenised **in place** and each token's address is
   stored into `myargv[]`, which the whole engine reads for the life of the
   process — so a scope-owned buffer would dangle every argument parsed from it.
   That one needs `myargv`'s own ownership rethought, not a container swap, and no
   test drives `@responsefile` today.
2. ~~**Dead code from Step 4 that outlived the zone.**~~ **Done — on the second
   attempt, and the first one is the lesson.** This entry read **Done** for a full
   session while `allocLow`, `beginRead` and `endRead` were still sitting in
   `Host/System.cpp`. What had actually happened is the trap already recorded two
   sections down: the *declarations* were deleted from `System.h` and the
   *definitions* were not, so nothing referred to them, nothing failed to link, and
   every gate stayed green over three orphaned functions — one of which,
   `allocLow`, would still `doom_malloc` a block and leak it if anything called it.
   `zoneBase`, `heapSize` and `mb_used` were genuinely gone. All three definitions
   are now deleted, and the stale comments crediting `allocLow` for the `screens[]`
   backing buffer (`Render/VideoState.h`, `Render/Video.cpp`) say `EA::Vector` instead.

   **This is the third time a completion claim in this file has been wrong in the
   same way**, after strand (a)'s two false finishes, and it is the same shape every
   time: the claim was checked against the pattern that produced it (here, "the
   header no longer declares them") rather than against the category ("no definition
   of them survives anywhere"). The document's own rule — *count the definitions left
   in the files you expected to empty* — was written for exactly this and was not
   applied to this entry. **A deletion is verified by grepping for the definition,
   not by grepping for the declaration.**
3. ~~**`doom_boolean` is still an `int`.**~~ **Done.** All ~288 uses are a real
   `bool` and **the typedef is deleted from `doomtype.h`**, so there is no longer a
   type in this engine that says "boolean" and means "int". It went in the planned
   four batches — **Render** (19), **Host** (4), **UI** (104), **Sim + Game + `SimProbe`**
   (161) — each verified against the full four gates: build clean, 82/82, all six
   goldens byte-identical, and `PureDoomEACP` linking.

   **The version bump this entry used to suggest was not made, and should not be.**
   There is no savegame-format version to bump. `Game/Game.cpp` stamps saves with
   `VERSION` (110, `Game/GameDefs.h`) — and that same constant is the **demo** header
   version, written at `Game.cpp:1549` and gated at `:1588`. It is the *game* version,
   wired to two formats; bumping it to invalidate old `.dsg` files reaches for a knob
   that also moves the demo stream. The exposure it was meant to close is small
   anyway: no `.dsg` is committed, `Tests/Goldens/` holds none, and the format is only
   ever round-tripped inside one build, where `sizeof` is consistent on both sides.
   If old-save rejection is ever genuinely wanted, add a *separate* save-format
   version field rather than overloading `VERSION`.

   **One narrow behavioural difference the goldens cannot see, recorded rather than
   fixed.** `DoomMain.cpp:850-853` assigns `opts.nomonsters = checkParm("-nomonsters")`
   and friends — and `checkParm` returns an **argv index**, not 0/1. `Game.cpp:1561`
   then writes that value into a recorded demo's header (`*demo.demo_p++ =
   opts.respawnparm`), so a flag sitting at argv[3] used to put a `3` in that byte and
   now puts a `1`. Gameplay is identical in both directions — every consumer
   truth-tests, and read-back treats any non-zero as set — and no golden covers it,
   because the goldens only ever *play demos back*, never record one. It is noted
   because "the flip was byte-neutral everywhere" would be a slightly stronger claim
   than the truth.

   Verified **not** hazards, and each traced to the bottom rather than pattern-matched:
   `TiccmdInput`'s `mousebuttons`/`joybuttons` are same-type interior views, and though
   `gameResponder` assigns raw masks into them (`mousebuttons[1] = ev->data1 & 2`), the
   `dclickstate` they are compared against is a plain `int` assigned *only* from those
   same arrays, so both sides normalise identically; the config `defaults[]` table binds
   `int*` onto members that are already `int`; there is no `union` and no `#pragma pack`
   anywhere in `src/DOOM`; the demo and save streams move these values through a `byte*`
   cursor, so each transfer was one byte either way; `Game.cpp`'s `paused ^= 1` compiles
   warning-free on a `bool` under `-Wall -Wextra -Wpedantic`; and `Hud.cpp`'s
   `numplayers += playeringame[i]` is safe, `bool` promoting to 0/1.

   One site was a genuine judgement call and was flipped deliberately:
   `Game.cpp:1126`'s `doom_memcpy(statcopy, &wminfo_, sizeof(wminfo_))`. `statcopy` is a
   raw address parsed from the DOS-era `-statcopy` flag for another process to read, so
   `wminfo_`'s layout is nominally an external ABI and the flip moved it. Nothing in this
   repository consumes it and an absolute pointer shared between two processes cannot work
   on a modern OS, so it moved with the rest of the struct, with the reasoning left at the
   call site.
4. ~~**Write-only state: `AutomapView::min_w` / `min_h`.**~~ **Resolved, and they
   are deleted.** The suspicion recorded here was that vanilla computed the
   zoom-out limit from them and that the rewrite had **lost a read** — a real bug.
   It had not. Checked against the 1993-lineage source preserved in this
   repository's own history (`git show 110ddbe:src/DOOM/am_map.c` — PureDOOM's C;
   see the `noblit` note below for why that distinction can matter, though not
   here: the automap is platform-independent and the transcription is faithful,
   id's own `// const? never changed?` comment included). They are assigned
   `2 * PLAYERRADIUS` at lines 404-405 and read **nowhere**, `max_scale_mtof` being
   computed from the literal `2 * PLAYERRADIUS` instead. So this was a genuine
   1993 leftover, the same class as
   `WeaponScratch`'s `swingx`/`swingy`, and carrying it forward preserved nothing.

   **The method is the reusable part**, and it is now the rule for this category:
   a written-never-read member has two possible causes with opposite responses —
   an id leftover (delete) or a read this rewrite dropped (fix) — and *only the
   older source distinguishes them*. Never delete one on the strength of the
   current tree alone.

   **The sweep across the other clusters has now been done on that basis**: all 84
   cluster headers, 604 data members, every zero-read candidate run down against
   `110ddbe`. **The headline is that there are no lost reads — none.** Every
   write-only member in the C++ is write-only in the older source too. That is the
   reassuring answer to the question `min_w` raised, and it is worth knowing before
   anyone goes hunting again.

   The leftovers it found are being deleted (`CompositeCache`'s three memory
   counters, `RenderScratch::sscount`, `RenderMainState::framecount`,
   `SightScratch::sightcounts`, `ActionScratch::secondslidefrac`/`secondslideline`,
   `EnemyAI::vileobj`, `AutomapView::leveljuststarted`, six of `StatusBarState`'s
   flags, `IntermissionState::firstrefresh`, `LevelStats::levelstarttic`,
   `SoundState::nextcleanup`, `GameVersion::gamemission`,
   `NetState::lastnettic`/`frametics`). Two of them are famous vanilla oversights
   worth knowing about rather than merely deleting: `secondslide*` is the
   wall-slide runner-up that `P_HitSlideLine` never consults, and `vileobj` is set
   before the archvile's corpse search by a `PIT_VileCheck` that never reads it.

   **Dead state cascades, and one sweep does not find it.** Deleting
   `StatusBarState::st_chat` removed the *only read* of two other members at once:
   the status bar's chat popup was a three-link chain,
   `st_msgcounter` → `st_chat` → `st_oldchat`, joined by a single statement
   (`if (!--st_msgcounter) st_chat = st_oldchat;`). Only `st_chat` looked write-only
   to the audit; the other two were alive *solely because a dead member read them*.
   Both were then deleted, and a third pass over all 84 clusters came back with
   nothing new — so the sweep is at a fixpoint, which is the thing to check rather
   than to assume. The chain was equally dead in vanilla: `st_msgcounter` is
   declared `= 0` and only ever decremented, nothing sets it positive, so
   `!--st_msgcounter` came true once every 2³² tics in 1993 too. **That popup has
   never run, in any DOOM built from this lineage.**

   Deleting `SightScratch::sightcounts` cascaded differently — it emptied the
   cluster outright. The empty `struct`, its accessor and its `Engine` member were
   removed too rather than kept "in case cross-call sight bookkeeping is wanted
   again"; speculative retention is what this step exists to undo.

   **Three are deliberately left**, and the reasons are the point:
   - **`InputConfig::usemouse` / `usejoystick`** are written only through
     `Config.cpp`'s `defaults[]` bind, so deleting them would change the *on-disk
     config surface* (`use_mouse`, `use_joystick` keys) and leave those entries
     with nothing to bind to. That is a config-format change wearing a dead-code
     deletion's clothes.
   - **`IntermissionState::bp`** caches the deathmatch player-face patches whose
     only draw call is *commented out* — in both eras. Deleting the member means
     deleting that commented-out draw, which is the only surviving evidence of why
     the load exists. Four pointers is a cheap price for legible history.
   - **`RefreshFlags::noblit`** is where the cross-check shows its limit, and this
     is the caveat to carry: **`110ddbe` is *PureDOOM's* C, not id's original.**
     `noblit` is set from `-noblit` and read by nothing there — but PureDOOM has no
     blit at all, so a port that far from the DOS renderer is exactly where a read
     could have been dropped *before* this fork began. **Not checked against id's
     own source, which this repository does not contain.** So the general lesson:
     "the older source never read it" can mean "an earlier port dropped it" rather
     than "it was always dead". The sweep's result is sound for what it claims —
     *this* rewrite lost no reads — but it is not evidence a field was never
     meaningful. `noblit` costs one `int`; left alone pending someone with the DOS
     source to hand.
5. The ~1,600 enum constants (`MT_*`, `S_*`, `SPR_*`, `MF_*`) keep their
   prefixes **on purpose** — they are DOOM's data vocabulary, `Sim/Info.cpp`'s
   generated tables are built from them, and renaming them is high-churn and
   zero-value.
6. **The macro layer.** The numbered list above was written when the open work was
   ownership, and it is now spent — items 1–4 are done or blocked, and 5 is a
   deliberate non-item. What is actually left of the goal's "no 1993 C idiom" half is
   the preprocessor, and it splits into three piles of very different value:
   - ~~**Function-like macros**~~ — **done**, and it paid for itself: retiring
     `SHORT`/`LONG` is what surfaced `thintriangle_guy`, the PCX width mismatch and
     the meaningless swap on `drawPatchRectDirect`'s `src_w`. `Host/Net`'s
     `ntohl`/`ntohs`/`htonl`/`htons` are the deliberate remainder: they sit inside
     `#if defined(I_NET_ENABLED) && !defined(DOOM_APPLE)`, so **no gate in this
     repository compiles them**, and a change there could not be verified by
     anything. Leave them until someone builds that configuration.
   - **Constant object-like macros** — **essentially done: 629 → 309 across
     `src/DOOM`**, of which 199 of the remainder is `Game/StringsFrench.h`, which no
     build here compiles, and ~55 more are the dead-in-both-eras pile this item
     deliberately leaves in place. Done: `Sim/SimDefs.h` (21), `Sim/SpecialTypes.h` (16),
     `Game/GameDefs.h` (39 of 40), `UI/AutomapTypes.h` (32), `Wad/MapFormat.h` (10),
     `Math/TrigTables.h` (11, by deletion — see below), `Game/NetTypes.h` (3),
     `UI/StatusWidgetTypes.h` (2), `Sim/WeaponTypes.h` (2), and the `.cpp`-local
     piles in `UI/StatusBar` (72 of 114), `Host/Sound` (34), `UI/Intermission` (20),
     `UI/Automap` (all), `Game/Game` (8), `Game/Net` (7), `Game/Sound` (7),
     `UI/Menu` (3), `Sim/Weapon` (5), `Render/Draw` (3), `UI/Hud` (4),
     `Sim/Player` (3), `Sim/Enemy` (2), `UI/Finale` (2), `Game/DoomMain` (1),
     `Sim/Specials` (1). Mostly **no call-site churn**: the name is unchanged, so
     every use inside `namespace Doom` still resolves.

     **The one- and two-macro tail is now done too** — the files the first survey
     never fanned out to. Seventeen names left the preprocessor in one batch:
     `Sim/Mobj.cpp`'s `STOPSPEED`/`FRICTION`, `Render/Things.cpp`'s
     `MINZ`/`BASEYCENTER`, `Render/Segs.cpp`'s `HEIGHTBITS`/`HEIGHTUNIT`,
     `Sim/Interaction.cpp`'s `BONUSADD`, `Render/Main.cpp`'s `FIELDOFVIEW`,
     `Render/Sky.h`'s `SKYFLATNAME`/`ANGLETOSKYSHIFT`, `Render/Main.h`'s `DISTMAP`,
     `Game/MapSpawns.h`'s `MAX_DM_STARTS`, `Game/Event.h`'s `MAXEVENTS`,
     `Game/DoomMain.h`'s `MAXWADFILES`, `Game/ConfigTypes.h`'s `STRING_VALUE`, and
     `Math/FixedPoint.h`'s `FRACUNIT`/`FRACBITS` (the latter **retired**, not
     converted — see the duplicate-constant entry below). `examples/EACP/
     EngineAccess.cpp` needed `Doom::` on `DISTMAP` and `STRING_VALUE`, and
     `Game/DoomMain.cpp:106`'s `static char* wadfiles[MAXWADFILES]` sits *before*
     the file's `namespace Doom` and needed qualifying too — a global-scope reader
     that is not one of the four this document keeps listing, found only by grepping
     every includer rather than the known-readers list.

     **That first survey is still the lesson**: it ranked files by macro count and
     fanned agents out to everything with two or more, which silently scoped out a
     third of the work. A per-file count is a work *estimate*, not a work *list*.

     **And the count in the previous draft of this entry was itself wrong.** It said
     312; the tree held **326**. Nobody had re-run the count after the last batch, so
     a number that exists precisely to track progress had drifted by fourteen while
     reading as measured. The command is one line and settles it:

         grep -rl --include='*.h' --include='*.cpp' -E '^[[:space:]]*#[[:space:]]*define' src/DOOM \
           | while read f; do grep -cE '^[[:space:]]*#[[:space:]]*define' "$f"; done \
           | awk '{s+=$1} END {print s}'

     Better still, verify a macro sweep by **diffing the set of names** before and
     after rather than the count — `grep -rhoE '#[[:space:]]*define[[:space:]]+\w+'`
     piped to `sort`, then `comm`. That is what confirmed this batch removed exactly
     seventeen names and *added none*, which a count alone cannot tell you: a sweep
     that retires one macro and introduces another nets to zero and looks untouched.

     Six things learned, all of which apply to the next header:
     - **Three bodies were unparenthesized** — `PLAYERRADIUS 16 * FRACUNIT`,
       `MAXRADIUS 32 * FRACUNIT`, `VDOORSPEED FRACUNIT * 2` — so a `constexpr` adds
       parentheses the expansion did not have, and is *not* automatically equivalent.
       Audited site by site rather than assumed: every use is either pure
       multiplication (associative, so grouping cannot matter), `+`/`-` (binds looser
       than `*`, so unaffected), or already hand-parenthesized — `MapAction.cpp:734`
       writes `(MAXRADIUS).raw`, so whoever wrote it already knew. The pattern that
       *would* differ is dividing by, or taking `.`/`->` off, a bare macro, and it
       occurs nowhere. **Establish that; do not assume it.**
     - **The name moves into `namespace Doom`, which breaks *global-scope* readers.**
       Three files needed qualifying: `Tests/SimProbe.cpp` and
       `examples/EACP/EngineAccess.cpp` (both ordinary non-namespaced translation
       units), and the two `#define R ((8 * Doom::PLAYERRADIUS) / 7)` in
       `UI/Automap.cpp` that sit *after* the file's closing brace, at deliberate
       `::` scope so the eacp compositor links against them by bare name. The app is
       the gate that catches the second kind, and `-DPUREDOOM_BUILD_EACP_EXAMPLE=OFF`
       never compiles it.
     - **Nothing had to stay a macro**, checked rather than presumed: no name in
       either header is read by `#if`/`#ifdef`/`#elif` or by `#`/`##`.
     - **Three were dead and are deleted, not converted** — `MAPBMASK`,
       `MO_TELEPORTMAN` and `CEILWAIT` have zero uses, and each was checked against
       `110ddbe` per the standing rule for this category: all three are dead *in
       vanilla too*, so they are id leftovers rather than lost reads.
       `MAPBMASK` is the instructive one — as `(MAPBLOCKSIZE - 1)` it could not
       compile as a real expression once `MAPBLOCKSIZE` became a `Fixed`, and needed
       contorting to `MAPBLOCKSIZE.raw - 1` to convert at all. **A constant that has
       to be rewritten to compile has not been evaluated in years**; that is the
       signal to check whether anything reads it before finding it a new spelling.
     - **Ask whether the constant already exists before converting it — eleven did.**
       This is the largest thing the sweep found and it is not a macro problem at all.
       Each of these was defined *twice*, once as the vanilla macro and once as a
       `constexpr` on the `Engine` cluster that owns the array it sizes:

       | macro | the constant that sizes the array |
       |---|---|
       | `AM_NUMMARKPOINTS` | `AutomapView::numMarkPoints` |
       | `MAXWIDTH` / `MAXHEIGHT` | `DrawTables::maxWidth` / `maxHeight` |
       | `MAXSEGS` | `SolidSegs::maxSegs` |
       | `MAXVISSPRITES` | `SpriteState::maxVisSprites` |
       | `MAXVISPLANES` | `PlaneScratch::maxVisplanes` |
       | `MAXOPENINGS` | `PlaneScratch::maxOpenings` |
       | `MAXANIMS` / `MAXLINEANIMS` | `AnimatedSurfaces::maxAnims` / `maxLineAnims` |
       | `MAXSWITCHES` | `SwitchList::maxSwitches` |
       | `MAXSPECIALCROSS` | `Clip::maxSpecialCross` |

       Six were dead. **Five were live and were a latent overrun**: the array is
       sized by the cluster constant and the overflow guard tested the macro, with
       no compile-time relationship between them. Raise `PlaneScratch::maxVisplanes`
       and `Render/Planes`' guard keeps the old bound, silently. All five now test
       the constant that sizes the array.

       **A sixth has since turned up, and it hid from the grep because it is not a
       constant at all.** `Sim/Mobj.cpp`'s deathmatch-start guard read

           if (spawns.deathmatch_p < &spawns.deathmatchstarts[10])

       — a **bare literal**, against an array sized `MAX_DM_STARTS`. Same defect,
       same consequence (raise the bound and the guard stays at ten), but no second
       *spelling* exists for a grep to find, and the instrument this document was so
       pleased with — `grep -rn 'constexpr.*// *[A-Z_]* in '` — could never have
       matched it. It now reads `[MAX_DM_STARTS]`, identical in value, so no golden
       moved. **The category is "the guard and the array bound are not the same
       token", and a literal is the commonest way to be in it.** Worth a sweep of its
       own: every fixed-size array member in a state cluster, checked against every
       comparison that bounds an index or a cursor into it.

       **The instrument is the reusable part.** The migration that introduced each
       twin left a comment naming the macro it replaced, so

           grep -rn 'constexpr.*// *[A-Z_][A-Z0-9_]* in ' src/DOOM

       enumerated the whole category in one command. The first three were found one
       at a time, by accident, while doing something else — which is how a category
       stays open for months.

       **That grep now returns nothing, and not because it is still watching.** The
       comments it matched named the retired macro, so retiring them meant rewriting
       every one to say what the constant sizes instead. The instrument was
       single-use: it worked because a *previous* migration happened to leave a
       uniform trace, and nothing guarantees the next one will. What generalises is
       the question, not the command — *before converting a constant, ask whether the
       thing it names already exists under another spelling*.

       And one instance was *created* by this very sweep:
       `AM_NUMMARKPOINTS` was converted to a `constexpr` beside
       `AutomapView::numMarkPoints` rather than retired, giving both spellings equal
       standing. **A macro-to-`constexpr` sweep that does not first ask "does this
       already exist?" manufactures the defect it is cleaning up.**

       **`FRACBITS` was the thirteenth, and it is the one that should have been
       obvious.** `Math/FixedPoint.h` defined `FRACBITS 16` two lines below a
       `using fixed_t = Doom::Fixed`, while `Math/Fixed.h` — the header it
       includes — had held `Fixed::fracBits` and `Doom::fracBits` all along, same
       value, same meaning. It survived twelve other findings of this exact category
       because a shift count cannot overflow an array, so it never produced the
       *defect* that made the other eleven visible. What it produced instead was
       drift: two sites had already reached the state where **both spellings appear
       inside one expression** —

           draw.dc_yl = (topscreen.raw + fracUnit - 1) >> FRACBITS;   // Render/Things.cpp
           t = (proj.centerxfrac.raw - t + fracUnit - 1) >> FRACBITS; // Render/Main.cpp

       — which is precisely the `ANG*`/`ang*` problem `Math/TrigTables.h` had
       (69 sites one way, 17 the other, nothing telling a reader they were the same
       constant). So `FRACBITS` is **retired**, not converted, and its twelve real
       sites read `fracBits`. `FRACUNIT` stays, because it is genuinely a different
       thing — the `Fixed` value 1.0, not the integer 65536 — but it is now
       `constexpr Doom::Fixed FRACUNIT {Doom::fracUnit}`, defined *from* the existing
       constant so there is one number in the header and not two. It stays at `::`
       scope deliberately, beside `fixed_t` and `FixedMul`: `Tests/Sim/
       PrimitiveTests.cpp` and `examples/EACP/EngineAccess.cpp` read it unqualified
       and neither has a `using namespace Doom`.

       **The generalisation, since this is now thirteen instances:** the category is
       not "a macro and a `constexpr` with the same value". It is *any* constant
       reachable under two spellings, and the eleven were found only because five of
       them had produced a guard/array mismatch. `FRACBITS` shows the silent
       majority — a duplicate that causes no bug and simply lets a file drift into
       using both names for one number. Look for it **before** converting, because
       after conversion the two spellings are equally idiomatic and nothing marks
       one as the newcomer.

       **A fourteenth, recorded rather than acted on:** `Host/Sound.cpp`'s
       `SAMPLERATE 11025` duplicates the public `DOOM_SAMPLERATE` in `DOOM.h`. It is
       left alone only because it is *also* dead in both eras and therefore belongs
       to the deliberately-untouched pile below; whoever deletes that pile should
       know it was a duplicate as well as dead.

       **And a fifteenth, which is the worst of the set and was found by item 8's
       array conversion rather than by any macro work.** The savegame description
       length existed as **three** constants: `menuSaveStringSize` (`UI/MenuState.h`)
       sizing the ten slot buffers, and a file-local `SAVESTRINGSIZE = 24` in *each*
       of `UI/Menu.cpp` and `Game/Game.cpp`. `Menu.cpp` then read a whole field into
       one of those buffers —

           doom_read(handle, &state.savegamestrings[i], SAVESTRINGSIZE);

       — with the length and the buffer's size coming from **different constants that
       nothing related**. Lower `menuSaveStringSize` and that is a buffer overrun on
       every load-menu open; raise it and the read silently truncates. The typed-name
       bound two functions down (`saveCharIndex < SAVESTRINGSIZE - 1`) had the same
       split.

       **What makes it the sharpest instance is how the protection was lost.**
       `MenuState.h` carried a comment explaining that the two were kept in step *by
       the compiler*: Menu.cpp's reference-to-array bindings were spelled
       `char(&)[24]`, so a drift would fail to compile. That was true when written.
       **Step 9 strand (a) then retired every one of those bindings**, which removed
       the mechanism — and left the comment behind, still asserting a guarantee that
       no longer existed. Nothing points from a deleted mechanism to the comment that
       depended on it. So the general form is worse than "two spellings of a
       constant": **a refactor can delete the thing an older comment relies on, and
       the comment then reads as reassurance.** When retiring a mechanism, grep for
       prose that names it.

       `UI/Menu.cpp`'s copy is retired and the file uses `menuSaveStringSize`.
       `Game/Game.cpp` keeps its `SAVESTRINGSIZE` — it is the *on-disk* field width
       and deserves its own name — but now carries the compile-time link that was
       missing all along:

           static_assert(SAVESTRINGSIZE == menuSaveStringSize, ...);

       Shown sharp by moving `menuSaveStringSize` to 25: the build fails on that
       assertion with its own message. **That is the shape to reach for whenever two
       constants must agree across a subsystem boundary** — not a third spelling, and
       not a comment.

       It is also the argument in item 8 arriving early and unprompted: this was
       found *because* the buffer became an `EA::Array` whose size is part of its
       type, which put the array's real bound next to a `doom_read` using a different
       number. A raw `char[24]` says nothing.

       `Math/TrigTables.h` was the same finding at eleven-in-one-file scale, and is
       the reason to check before converting rather than after: every numeric name in
       it — `FINEANGLES`, `FINEMASK`, `SLOPERANGE`, `SLOPEBITS`, `DBITS`,
       `ANGLETOFINESHIFT`, `ANG45`/`ANG90`/`ANG180`/`ANG270` — already had a twin in
       `Math/Trig.h` or `Math/Angle.h`. The angle constants were the sharp ones: **69
       sites said `ANG*` and 17 said `ang*`, for the same four values**, with nothing
       telling a reader they were the same constant. So that header was a deletion and
       a redirect, not the eleven-macro conversion it was queued as. `PI` went with
       them — no reader here, none in `110ddbe`, and its only mention in either era is
       the typo'd comment on `finecosine`.
     - **Some object-like macros are not constants at all.** Six have bodies that call
       a runtime accessor: `UI/Hud`'s `HU_TITLE`/`HU_TITLEY`/`HU_INPUTY`
       (`gameSession()`, `hudFont()`), `UI/StatusBar`'s `ST_MAPWIDTH`/`ST_MAPTITLEX`
       (`doom_strlen` over live state), and `Game/Game`'s `MAXPLMOVE`
       (`movementSpeeds().forwardmove[1]`, which `-turbo` rescales at startup). No
       `constexpr` is available to any of them. They are function-like macros wearing
       an object-like spelling and they want to be inline functions, which is
       call-site churn and a separate change. **Unclaimed.**
   - **~55 macros are dead *and* dead in 1993, and are deliberately left in place.**
     Zero uses in the tree, and zero in `110ddbe` beyond their own `#define` — so id
     leftovers, not reads this rewrite dropped. `UI/StatusBar` holds 42 of them (the
     twelve `ST_WEAPON0X`…`ST_WEAPON5Y`, the message/outtext/map-title coordinates,
     the key widths), `UI/Intermission` seven (`SP_KILLS`…`SP_PAUSE`, one of which,
     `SP_PAR`, expands to an identifier that was never defined in either era),
     `Game/Sound` five, `UI/Hud` five, `Host/Sound` two, `Game/DoomMain` four
     (`X_OK`/`W_OK`/`R_OK`/`RW_OK`, never paired with an `access()` call).
     Converting them would dress dead code as live; deleting them is the separate
     judgement call this document reserves for a human, and is **unclaimed**.

     **A sweep converted ten of them anyway, and the way it went wrong is worth the
     entry.** The work list handed to the agent was built by grepping for files with
     one or two `#define`s left — a mechanical, complete-looking query — and that list
     silently *overlapped this pile*: `Game/Sound`'s five, `UI/Hud`'s three,
     `Host/Sound`'s two and `Render/Video.h`'s `CENTERY`. The agent converted them as
     instructed, and flagged the contradiction with this document rather than
     resolving it quietly, which is the only reason it was caught before landing. All
     ten are reverted to macros.

     **The tell was in the code the sweep produced.** Every one came out as

         [[maybe_unused]] constexpr int S_MAX_VOLUME = 127;   // "kept as a dead
                                                             //  constant rather than
                                                             //  dressed up as live"

     and an attribute whose entire job is to silence the diagnostic that says *this is
     unused* is a fairly loud signal that the thing should not have been converted.
     **When a mechanical conversion needs an annotation to stay quiet, ask what the
     compiler was trying to tell you before you silence it** — this repository already
     has one bug (`thintriangle_guy`) that lived for months behind exactly that
     reflex.

     **A second, unrelated `[[maybe_unused]]` was pure cargo cult, and measurement
     killed it.** The same sweep put the attribute on the six *live* header constants
     too (`DISTMAP`, `SKYFLATNAME`, `MAXEVENTS`, …), reasoning that a namespace-scope
     `constexpr` has internal linkage and so would warn in every including TU that
     does not read it. That reasoning is plausible and wrong: Clang deliberately does
     not warn about unused const variables **declared in a header**, which is why
     `Sim/SimDefs.h`'s 21 and `UI/Hud.h`'s have never needed one. The attribute was
     also a new idiom — `git grep` found **zero** at HEAD — which is itself the signal
     worth acting on. Removing all six left the build at its one deliberate warning,
     on a full clean rebuild. **A codebase that has done a thing a hundred times
     already has an answer; introducing a new spelling for it deserves a measurement
     first.**

     One of them is worth a second look rather than a deletion: `ST_NUMFACES`'s
     comment says the `faces[]` array is "sized off the same macro", and
     `StatusBarGraphics.h` in fact computes that bound independently as
     `(3 + 2 + 3) * 5 + 2`. The cross-reference is a comment, not a compile-time
     dependency — the same shape as the eleven duplicate constants above, and the
     one instance of it still standing.
   - ~~**The string tables**~~ — **done for the half this repository compiles.**
     290 of `Game/StringsEnglish.h`'s 293 are `constexpr` in `namespace Doom`.
     `Game/StringsFrench.h` (199) is deliberately untouched: `FRENCH` is defined
     nowhere, so no build here compiles it and no gate could verify a change — the
     same reasoning that leaves `Host/Net.cpp`'s `ntohl`/`ntohs` alone.

     **Three names stay macros, and between them they are the whole lesson of this
     item.** `PRESSKEY` and `PRESSYN` are string-literal building blocks that ten
     entries end in (`"...\n\n" PRESSKEY`); `DOSY` is concatenated the same way at
     a *call site* in `UI/Menu.cpp`, twice. Adjacent-literal concatenation happens at
     translation phase 6, and a `constexpr const char*` cannot do it.

     **Those three were found by three separate searches, each of which missed what
     the next one caught**, and that is the part to carry rather than the outcome:

     | Pass | Searched | Missed |
     |---|---|---|
     | 1 | `MACRO "literal"` in `#define` bodies | the operand order — every real one is `"literal" MACRO` |
     | 2 | `"literal" MACRO` in `#define` bodies | the file region — `DOSY`'s two uses are in ordinary code |
     | 3 | both orders, across all `.cpp`/`.h` | — |

     **A grep proves something about what it looked at and nothing about what it did
     not.** Both misses were in the *scope* of the search rather than its pattern,
     which is why each looked exhaustive and returned honest results.

     One improvement the conversion forced rather than worked around: `gammamsg` was
     `EA::Array<EA::Array<char, 26>, 5>`, a fixed 26-byte buffer per message that
     existed only because `GAMMALVL0`-`4` were literal macros. Its single reader
     wants a `const char*`, so it holds pointers now and the 26 cannot silently
     truncate. Converting those five back to macros would have been the easy way out.

     Verification worth copying for any bulk string change, since 290 strings is too
     many to eyeball: extract every string and char literal from the file before and
     after and `diff` them. Identical here, bar two that appear inside the added
     comments. The goldens agree independently — `menu.frames`, `finale.frames` and
     the demo frames cover most of these strings.

     **`Strings.h` went with it, and it holds a fourth concatenation family.**
     `SAVEGAMENAME` and `NUM_QUITMESSAGES` converted (the latter is an array bound at
     `UI/Menu.cpp:99`, which `constexpr int` serves); **`DEVMAPS` and `DEVDATA` stay
     macros**, because `Game/DoomMain.cpp` builds fifteen dev-mode paths out of them by
     concatenation — `addWadFile(DEVDATA "doom1.wad")`, `doom_strcpy(file.data(), "~"
     DEVMAPS "E")`. So the count of names that cannot leave the preprocessor for this
     reason is **five**, not three, and the fifth family was in a file the three
     searches above never opened. The pattern really is the recurring one: each pass
     widened the *scope* and found more.
7. ~~**Zero the warning count, and treat it as a gate.**~~ **Done — the engine
   builds with exactly one warning**, down from **81** at the start of this session,
   and that one is deliberate. `thintriangle_guy` is why this was worth doing at all
   rather than filed as tidiness: the compiler had been naming that bug in plain
   language in every single build, and it was invisible inside the noise. What the
   three classes were:
   - ~~**59 × `-Wwritable-strings`**~~ — all in `UI/Hud.cpp`: `chat_macros[]`,
     `player_names[]` and `mapnames[]` were `char*[]` initialised from string
     literals, while the neighbouring `mapnames2`/`mapnamesp`/`mapnamest` were
     **already** `EA::Array<const char*, 32>` — three of four converted and the
     fourth not, the same partially-applied-fix shape as `thintriangle_guy`, which
     is now twice in one session. All three are `EA::Array<const char*, N>` now.

     **`chat_macros` was the one that needed thought, and it is `const char*`, not
     `char* const`.** Its elements are genuinely reassigned at runtime:
     `Config.cpp`'s `defaults[]` stores `&chat_macros[i]` and `loadDefaults` writes
     a pointer to heap-owned storage back through it when the config supplies a
     macro. So the *pointers* are mutable and only the *pointees* are const. The
     ripple that followed is worth knowing because it turned out to be **one field**:
     `ConfigDefault::text_location` became `const char**`, and nothing else in the
     table had to move. It also retired a `*(char**)` cast in `saveDefaults` that
     existed only to launder the type.

     The trap here is the one this document already records in another form: `extern`
     declarations must move in **lockstep** with the definition, and these had six
     across four files (`Hud.cpp` ×3, `Config.cpp`, `Game.cpp`, `StatusBar.cpp`).
     With `EA::Array<const char*, N>` the size is part of the type, so a stale
     declaration is an ODR violation rather than a link error — check every one, and
     check the count in the declaration against the initializer (`mapnames` is 45:
     four episodes of nine, plus nine `"NEWLEVEL"`).
   - ~~**13 × `-Wunused-variable`**~~ — rewrite leftovers, where the vanilla `an`
     fed `finesine[an]` and the C++ replaced it with an `Angle::fineIndex()` local
     under another name. Deleted against the documented bar (zero reads *and* zero
     writes) with each site checked for a lost read rather than pattern-matched.
   - **1 × `-Wcast-function-type-mismatch`** in `Sim/Weapon.cpp` is *deliberate* and
     is **the one that remains** — the type-erased `states[].action` pointer cast
     back to its exact signature, a round-trip and therefore well-defined. It wants
     a narrowly scoped suppression it can be pointed at, not a change to the code.

   **`-Werror` is *not* one warning away, and the first draft of this entry said it
   was.** The correction is recorded rather than quietly made, because it is the same
   unchecked-claim failure this document exists to catch, committed in the same
   session that wrote the lesson down.

   The count of **1** is measured on **Apple Clang, macOS, arm64** — both `Debug` and
   `Release`, which is worth having checked, since optimisation level changes which
   warnings fire. But `.github/workflows/tests.yml` builds **five** configurations:
   gcc and clang on Ubuntu, gcc and clang on macOS, and **MSVC on Windows**, all at
   `Release`. Two of those are a compiler this repository has never measured, and
   MSVC is not even on the same flags — `src/DOOM/CMakeLists.txt` gives it `/W4`,
   not `-Wall -Wextra -Wpedantic`, and `/W4` warns about a different set of things
   (unreferenced formal parameters, signed/unsigned mismatch, conditional-expression-
   is-constant) that this code has never been held to.

   So the actual prerequisite is: **measure the count on all five, then decide.**
   Turning `-Werror` on from a one-compiler measurement would break `master` on push
   for four configurations out of five, which is a worse outcome than the warnings
   it prevents. A cheap first step is a CI job that *reports* the per-configuration
   count without failing on it.

   Until then, *read the build output*. That is the whole lesson of
   `thintriangle_guy`, and it is the kind of discipline that decays the moment a
   second warning is tolerated — which is an argument for watching the number, not
   for enforcing it before it is known.

   **The eight `-Wliteral-conversion` warnings are already gone** — they *were*
   `thintriangle_guy`, and fixing the bug removed them. Worth stating plainly,
   because it is the cleanest possible demonstration of why the pile mattered: the
   only warnings in the build that described a real defect looked exactly like the
   seventy that did not.
8. **Strand (c)'s "C arrays → `EA::Array`" — surveyed, and the UI is done.**
   Enumerated member by member rather than counted: **130 raw fixed-size array
   members across 43 headers**, of which **19 must stay raw** (table below) and
   **111 are safe**. This began as the fourth overstated completeness claim this
   document has had to walk back, and it was the same failure as the others — the
   sweep converted the arrays it *touched* while doing something else, and the
   category was never counted.

   **The UI subsystem is now converted: 35 members across 10 headers**
   (`IntermissionState`, `StatusBarGraphics`, `StatusBarWidgets`, `HudChat`,
   `MenuState`, `HudWidgetTypes`, `AutomapView`, `StatusBarState`, `StatusBarFace`,
   `HudFont`), with all four gates green and every frame golden byte-identical —
   which matters here more than usual, because `menu.frames`, `automap.frames`,
   `finale.frames` and the three demo `.frames` between them cover the status bar,
   HUD, menu, automap and intermission. It was chosen as the first batch for exactly
   that reason.

   **The Render subsystem followed: 29 members across 10 headers** (`PlaneScratch`
   11, `Lighting` 3, `SpriteState` 3, `RenderTypes`' `SpriteFrame`/`VisPlane` 4,
   `DrawTables` 2, `ViewProjection` 2, `BSPScratch`, `SolidSegs`, `SpriteScratch`,
   `Data`'s `Texture::name`), again with every frame golden byte-identical — which is
   the gate that matters most for this batch, since it is the software renderer and
   six `.frames` goldens point at it. **`Patch::columnofs` was excluded**, per the
   corrected hazard table above. `Render/Draw.cpp` needed no edit at all: its
   `tables.ylookup[i] + tables.columnofs[x]` are element reads, not decay.

   **`Game/` and `Sim/` finished it: 47 more members across 19 headers** — `NetState`
   11, `MapTypes`' `Sector`/`Line`/`Node` 5, `Clip` 3, `ActiveSpecials` 3,
   `TiccmdInput` 3, and the rest in twos and ones down to `Wad/WadFile.h`'s
   `Lump::name`. This is the batch that edits the **playsim**, so the `*.hashes`
   goldens are the gate that matters, and they are byte-identical.

   **So item 8 is done: 111 of 111 safe members converted, 19 correctly left raw.**

   **If you re-run the count, know that the obvious grep undercounts.** A pattern
   matching one `[...]` misses a **2D** array (`short bbox[2][4]`) and any member
   whose `= {}` initializer wraps to the next line. Checking this document's "19"
   with such a pattern returns 18 and looks like a retraction — the same bad-grep
   trap the `(void)` row records, walked into again while verifying this very
   section. The 19 are: `Wad/MapFormat.h` 8, `Game/PlayerTypes.h` 9,
   `Render/RenderTypes.h`'s `Patch::columnofs`, `Game/NetTypes.h`'s
   `NetPacket::cmds`. Likewise the warning count is **1 on a clean build**; an
   incremental one reports 0 because `Sim/Weapon.cpp` is not recompiled, which is a
   measurement artefact and not an improvement.

   Four things it taught that the earlier batches had not:

   - **`EA::Array<char, N>` is not an aggregate, so a bare string literal in a table
     stops binding.** `Sim/Switches.cpp`'s 41-entry `alphSwitchList[]` needed
     `EA::Array<char, 9> {{"SW1BRCOM"}}` rather than `"SW1BRCOM"` — the only place in
     the whole conversion needing a change of code *shape* instead of a `.data()`.
     Verified the way this document prescribes for any bulk string change: extract
     every literal before and after and `diff` them. Identical, 103 either side.
   - **`array-of-struct` decays in a pointer-*difference* idiom, which is easy to
     undercount.** `player - players_.players` computes a player index from a
     `Player*` in four places (`SaveGame`, `Interaction` ×2, `Intermission`), plus
     `spawns.deathmatch_p - spawns.deathmatchstarts`. None was in the briefing; all
     five were found by the compiler. **Grep for `- <arrayname>` when converting an
     array of structs**, because each site is individually a build error but the set
     is invisible to a reader.
   - **The enum-index warning in the briefing was wrong, and harmlessly so.**
     `AmmoLimits::maxammo` is indexed by `AmmoType`, an *unscoped* enum, which
     converts to `int` on its own — no `static_cast` needed anywhere. The
     `static_cast<int>` friction this document records is real but specific to
     `EA::Vector`'s `SizeType` concept, not to `EA::Array`.
   - **Nine members gained zero-initialization**, per the semantics correction above:
     `Sector::blockbox`, `Line::sidenum`/`bbox`, `Node::bbox`/`children`,
     `Clip::intercepts`, `SwitchListEntry::name1`/`name2`, `Lump::name` had no
     initializer before. Each is fully overwritten by its loader before any read
     (`Sim/Setup.cpp`'s `groupLines`/`loadLineDefs`/`loadNodes`, `pathTraverse`,
     `WadFile::addDirectory`), which the WAD, demo and level goldens confirm
     empirically rather than by argument. Recorded because "no initializer, so
     unchanged" is the wrong instinct here.

   Two `Game/` members publish a decayed pointer that outlives the call and had to
   keep pointing at the same storage — `NetState::exitmsg` into `Player::message`,
   `ConfigPaths::basedefault` into `ConfigPaths::defaultfile`. Both are fine under
   `.data()` (a by-value member's storage is stable for the `Engine`'s lifetime) and
   both are the standing RAII rule in another dress: a pointer that merely refers
   stays raw.

   Two things that batch taught, both about the decay sites:
   - **A nested array reached through a non-array member is easy to miss.**
     `HudChat::w_inputbuffer[i].l.l` and `w_chat.l.l` are the `char` buffer inside a
     `HudTextLine`, reached through a member that is not itself being converted. A
     work list built from the converted members does not contain them; the compiler
     found them.
   - **The enumerated decay list was incomplete and the compiler closed the gap.**
     `UI/Menu.cpp` had five `.data()` sites nobody had listed (`writeText`,
     `stringWidth`, `saveGame`, and the `startMessage` arguments). That is the right
     division of labour for this particular sweep — unlike a *deletion*, where
     nothing fails to build, a container swap makes every missed site a compile
     error. **Lean on that: it is one of the few sweeps in this document the build
     verifies for you**, which is why it is safe to hand out with an admittedly
     partial list.

   **It also found the sharpest duplicate-constant instance in the document**
   (the savegame description length, three spellings, one of them bounding a
   `doom_read` into a buffer sized by another) — see item 6's fifteenth entry. That
   is the concrete return on this conversion, arriving in the first batch.

   **Do not simply finish it, because part of the remainder is load-bearing — and
   the list of which part was wrong in both directions until it was surveyed.**
   The category has now been enumerated member by member rather than counted:
   **130 members across 43 headers** (the "128" above was close but never checked),
   of which **19 must stay raw and 111 are safe**. The hazard list this entry
   carried named three headers; **two of them were not hazards at all, and two real
   hazards were missing.** Corrected:

   | Must stay raw | Why | In the old list? |
   |---|---|---|
   | `Wad/MapFormat.h` (8 members) | `reinterpret_cast` onto raw lump bytes in `Sim/Setup.cpp` — an on-disk format | yes |
   | **`Render/RenderTypes.h`'s `Patch::columnofs`** | **the same thing one file over** — every `Patch*` is `static_cast` straight off `cacheLumpName`. Worse than a plain overlay: `columnofs[8]` is a *flexible array*, declared 8 and indexed to `[width]`, with the pixel data starting at `&columnofs[width]`. Its own comment says so | **no** |
   | `Game/PlayerTypes.h`'s `Player` (7 members) | `Sim/SaveGame.cpp:73` memcpys the whole `Player` | yes |
   | **`Game/PlayerTypes.h`'s `IntermissionStart`/`IntermissionPlayer` (2)** | **a whole-struct memcpy outside the SaveGame path entirely** — `Game/Game.cpp:1138` writes `wminfo_` to the `-statcopy` address. Dead in practice, but it is still a serialised struct with array members | **no** |
   | **`Game/NetTypes.h`'s `NetPacket::cmds`** | **packed onto the wire** — `Game/Net.cpp` checksums it through a `reinterpret_cast<unsigned*>` and `Host/Net.cpp` byte-swaps it field by field | **no** |

   And the two that were listed and should not have been:
   - **`Sim/MapTypes.h` (5 members, not 4) is safe.** The entry justified it with
     `doom_memcpy(mobj, th, sizeof(*mobj))` — but **`Mobj` is not in `MapTypes.h`**,
     it is in `Sim/MobjTypes.h`, and **it has no array members at all**.
     `MapTypes.h`'s arrays belong to `Sector`, `Line` and `Node`; `archiveWorld`
     copies `Sector`/`Line` fields *one at a time* into a `short*` stream, and
     `Node` is BSP data rebuilt from the WAD on every load and never saved.
   - **`Sim/SpecialTypes.h` is safe**, and the shorthand "the eight thinker
     specials" is wrong twice over: `SaveGame.cpp` archives **seven**
     (`FireFlicker` has no case at all), and **none of the seven has an array
     member**. That header's only two arrays belong to `SwitchListEntry`, which is
     the switch-texture table and is not archived by anything.

   **The shape of the error is the reusable part.** Every wrong entry came from
   reasoning about a *header* when the property belongs to a *struct* — "PlayerTypes
   is serialised" is true of `Player` and irrelevant to the other structs in the
   file; "MapTypes holds `Mobj`" was simply not checked. And the two missing hazards
   were missed because the search was seeded from a known list (`SaveGame.cpp`'s
   memcpys, `MapFormat.h`'s casts) rather than from the question *who else reads
   these bytes* — which is the identical failure, and the identical fix, as the
   `doom_boolean` "types that lie" sweep two sections down. **Classify the owning
   struct, not the file, and ask the ownership question of every one.**

   Any container swap in the 19 has to leave `sizeof` and layout untouched, and no
   golden covers it: `doomSimSaveLoadPreservesWorld` round-trips within a single
   build where both sides agree by construction, and nothing here exercises the
   wire or `-statcopy` paths at all.

   `EA::Array<T, N>` is a wrapper over `std::array<T, N>` with exactly one member, so
   it *is* size- and layout-identical and trivially copyable — but that is an
   **implementation fact about eacp's container**, not a language guarantee, and it
   should be re-checked rather than remembered before being relied on. **It has now
   been both re-checked and pinned**, because the Render batch found the one place in
   the engine that genuinely depends on it:

   - **`VisPlane::top`/`bottom` are indexed out of bounds on purpose.**
     `Render/Planes.cpp` writes `pl->top[pl->maxx + 1] = 0xff` and
     `pl->top[pl->minx - 1] = 0xff` — a sentinel one byte past each end, so the
     span-filling loop can run off either edge and stop on it with no bounds test in
     the inner loop. `pad1`..`pad4` exist to absorb exactly those writes, and the
     trick holds only while `top` and `bottom` occupy exactly `SCREENWIDTH` bytes
     with the pads immediately either side. The conversion preserved it, and
     `RenderTypes.h` now carries a `static_assert` on
     `sizeof(EA::Array<byte, SCREENWIDTH>)` so a future eacp change cannot break it
     silently. **A remembered implementation fact became a compile-time gate**, which
     is the same move as the `SAVESTRINGSIZE` assert in item 6, for the same reason.
     (A companion `offsetof` assertion was written and then removed on purpose:
     `offsetof` on a non-standard-layout class is only conditionally supported, and
     item 7's point stands — the warning count here is measured on Apple Clang alone.)

   - **The conversion is *not* initialization-neutral, and a batch report claimed it
     was.** `EA::Array`'s sole member is declared `ContainerType container {};` — a
     default member initializer — so `EA::Array<char, N> x;` **value-initializes**
     where `char x[N];` left garbage. The UI batch left `= {}` off two members
     (`HudTextLine::l`, `HudScrollingText::l`) reasoning that this "matches original
     semantics exactly"; it does not. Measured rather than argued, with a placement-new
     over a `0xAB`-filled buffer: the raw array reads back `0xAB`, the `EA::Array`
     reads back `0x00`.

     It is harmless here — those members live in `Engine` clusters that are
     value-initialized anyway, and every golden is byte-identical — and zeroing is the
     safer direction. But **"I left the initializer off, so nothing changed" is false
     for this container**, and on an array deliberately left uninitialized for cost it
     would be a real (if benign) behaviour change. Worth knowing for the remaining
     batches: the conversion adds zero-initialization whether or not you ask for it.

   For the rest — the scratch and state clusters — the conversion is safe. **About
   40 of the 111 have a bare-decay site** that a mechanical swap breaks and that
   therefore needs `.data()`: `plane.lastopening = plane.openings`,
   `spritelights = lights.scalelight[n]` (a row of a 2D array), the
   `doom_memset(check->top, 0xff, sizeof(...))` family, and the widget
   initialisers that take `Patch**`. Four are worth singling out because the
   decayed pointer is **stored and outlives the call** — `NetState::exitmsg` and
   `HudChat::lastmessage` are published into `Player::message`,
   `ConfigPaths::basedefault` into `ConfigPaths::defaultfile`, and
   `MenuState::endstring`/`tempstring` into `MenuState::messageString`. Those are
   the owner-of-owners hazard this document already records, in a different dress.

   This entry used to call the value "real but modest" and recommend doing it
   per-cluster rather than as a sweep. The *scheduling* advice still holds, but the
   value is worth restating upward, because the bare-literal guard found in
   `Sim/Mobj.cpp` this session (item 6) is precisely the defect an `EA::Array`
   makes unwritable: with a real container the bound is `arr.size()`, reachable
   from the array itself, so a guard **cannot** drift away from what it guards.
   That is the same defect class as the thirteen duplicate constants, and it is the
   strongest argument for the conversion — stronger than tidiness.

Recently finished, for orientation: **the function-like macro layer, and the
`thintriangle_guy` bug it uncovered** (the newest work — `SHORT`/`LONG` across 154
sites, `UI/Automap`'s seven, `PADSAVEP`, `SCRAMBLE`; three new tests in
`Math/`, one in `Automap/`; the orphaned `Host/System` definitions this document
had already called deleted; 13 unused-variable leftovers; warnings 81 → 1); **the
`doom_boolean` flip and the deletion of the
typedef** (four batches, ~288 sites); the 247 file-local reference
aliases (strand (a), seven batches); `MovementSpeeds` and `VideoState::dirtybox`
retyped to `int`; the hitscan traversers returning `AimResult`; frame goldens for the
automap and the finale; and the constructible `Engine` with the test that proves it.

**How the `doom_boolean` flip was actually run, since the method is the transferable
part.** The mechanical substitution is not the work; finding the sites where it is
*not* mechanical is. What that took, in order:

1. **Audit before editing, by hazard class rather than by spelling.** Four classes
   were worth enumerating up front — a value that is not 0/1 whose *number* is read;
   an address handed to something that reads a fixed byte count; a struct that is
   `memcpy`'d, persisted or overlaid on foreign bytes; and a pun between a boolean
   array and another type. Two real findings came from the address class alone
   (`ioctl`'s `trueval`, and `-statcopy`'s external struct), and **neither is visible
   from the declaration** — only from asking who else reads those bytes.
2. **Pre-clear the false alarms, in writing.** Eight sites match a dangerous pattern
   and are safe (`solid = thing->flags & MF_SOLID` and the rest). Handing that list to
   whoever does the edit is what stops the flip stalling on each one, and it forces the
   trace to be done once, properly, rather than re-argued per site.
3. **Know what the goldens cannot see, before trusting them.** They are extraordinarily
   sharp on the simulation and blind in three specific places: `deathmatch != 0` (all
   three demos are single-player), the netgame paths, and audio. Every `doom_boolean`
   in those regions was read by hand — `gameResponder`'s raw `ev_mouse` masks are the
   clearest case, since a demo feeds `Ticcmd`s directly and never produces one.
4. **Build the app.** `wipe_melt_running` is a `bool` crossing into
   `examples/EACP/EngineAccess.cpp`, and `-DPUREDOOM_BUILD_EACP_EXAMPLE=OFF` never
   compiles that file. The already-documented fourth gate earned its place again.

### Types that lie, found under `doom_boolean` — five of them, all now corrected

These were the hard blockers for the `doom_boolean` flip, and every one is the
same shape: a declaration that says "boolean" over storage that holds something
else. Each is now declared as what it actually is. **The corrections are
behaviour-neutral by construction** — `doom_boolean` *was* `int`, so the
storage, the layout and every value read are unchanged; what the change buys is
that the flip could no longer reach them. Verified: build clean, 82/82, all six
goldens byte-identical, app links. What each one was, kept because the *reasons*
are the load-bearing part:

**Three were found before the flip** (the first three below). **Two more surfaced
during it**, and the way each was found is the reusable part:

- **`Host/Net.cpp`'s `trueval`** is handed to `ioctl(insocket, FIONBIO, &trueval)`,
  which reads a full word back through that address — a one-byte `bool` makes that
  three bytes of neighbouring stack. **Now `int`.** It was found not by reading the
  declaration but by asking, of every `doom_boolean`, whether its *address* goes
  anywhere that reads a fixed byte count. The confirmation was sitting four lines up:
  the Windows arm of the same `#if` already spells it `u_long trueval = 1`, because
  that ABI's own header forced someone to get it right on one side only.
- **`Sim/Specials.cpp`'s `AnimDef::istexture`** was *already* a plain `int`, with a
  comment saying why: the animdefs table terminates on a `{-1, "", "", 0}` sentinel,
  and under a real `bool` that `-1` reads as `true`, so the end-of-table test never
  fires and the scan runs off the array. Worth listing because it is the same class
  caught by a previous reader, and because the neighbouring `SurfaceAnim::istexture`
  — the *destination* it is copied into — is safely `bool`, the copy happening only
  for rows the loop guard has already excluded the sentinel from.

**The general shape, since five instances is enough to call it a rule:** a boolean
declaration is lying whenever its storage is *not* private to the program — an
on-disk field, an OS/ABI call reading through its address, a wire or table sentinel,
or a value with more than two meanings. Grepping the type finds none of these. What
finds them is asking of each site: who else reads these bytes, and how many do they
expect?

- **`Render/Data.cpp`'s `MapTexture::masked`** is `doom_boolean` in a struct
  *overlaid directly onto raw `TEXTURE1`/`TEXTURE2` lump bytes*. Its value is
  never read — but its **four bytes are load-bearing**, because `width`,
  `height`, `columndirectory` and `patchcount` are read through the same overlay
  and would all shift by three bytes under a one-byte `bool`. Every texture in
  every WAD would parse as garbage on the first level load. It is an on-disk
  field, not an application boolean; it should be `int`. (The neighbouring
  `columndirectory` already carries a comment from someone who hit this exact
  class of bug.) **Now `int`**, with that reasoning recorded on the field.
- **`Game/GameSession.h`'s `deathmatch` is tri-state**: 0 coop, 1 deathmatch,
  2 altdeath. `DoomMain` assigns 2, `Sim/Interaction` and `Sim/Mobj` gate
  altdeath-only item rules on `!= 2`, and `Game/Net` packs it as a two-bit
  field. A `bool` collapses 1 and 2, silently turning the altdeath rules on for
  plain deathmatch — **and no golden would catch it**, because all three demos
  are single-player with `deathmatch == 0`. This is the clearest case yet of a
  change the net is blind to. **Now `int`**, with the tri-state spelled out on the
  member so the next reader cannot mistake it for a flag.
- **The ARMS widget's `(int*) &plyr->weaponowned[i + 1]`** (`UI/StatusBar.cpp`,
  vanilla's own cast) is the pun `doomtype.h` describes, and it is real:
  `weaponowned` *is* `doom_boolean[NUMWEAPONS]`, and `updateMultIcon`
  dereferences that as a 4-byte `int` index. At one byte it reads three
  neighbouring elements — and for `i = 5`, spills into the `int ammo[]` that
  follows. **Untangled**: the six icons now index
  `StatusBarWidgets::w_armsindex[6]`, refreshed from `plyr->weaponowned[i + 1]` by
  `drawWidgets` immediately before it updates them. The copy is behaviour-identical
  because `updateMultIcon` only ever *reads* through `inum`, and `plyr` is assigned
  in exactly one place — `initStatusBarData`, immediately before `createWidgets` —
  so the widget and the player can never disagree about which player is shown.

**A false trap — and this document said it was corrected while it was still there.**
`Game/PlayerState.h` claimed vanilla makes `(int*)` casts into `playeringame` that
"rely on `doom_boolean` being int-sized." There are none, anywhere in the tree — not
in `src/DOOM`, not in `Tests/`, not in `examples/`; the note had generalized from the
real `weaponowned` precedent. A wrong warning about a load-bearing quirk is worse than
no warning: it sends the next reader hunting for a hazard that does not exist, or
preserves a constraint nothing needs.

**The correction is now actually in the file** — it was not before. This entry read
"now corrected" for a full session while `PlayerState.h:19` still carried the warning
verbatim: the fix had been written into *this document* and never into the source. It
was found only because the `doom_boolean` flip had to read every one of those
declarations anyway.

That is the same failure as strand (a)'s three false completions, in a new place, and
it sharpens the lesson rather than repeating it. **A claim in this file is not evidence
about the tree.** The earlier rule — prefer a claim a *test* can fail over one only a
grep can support — has a corollary for the claims that no test can hold: when the
correction is to a *comment*, the document recording it and the thing being corrected
are both prose, and nothing but re-reading the source will ever catch the gap. Cite
the file:line you actually changed, so the claim can be checked against something.

### Traps carried from earlier sessions

- **A two-pass rename can orphan what it meant to delete.** Where the vanilla and
  namespaced names are the same identifier (`I_Error`), pass 1 renamed the shim's
  own definition and pass 2 then found nothing to delete — leaving 48 dead global
  duplicates that linked fine, never ran, and passed build + tests + goldens.
  After a rename sweep, **count the definitions left in the files you expected to
  empty.**
- **`grep -c ": error"` does not match `: fatal error:`** — the form clang uses
  for a missing header. A fresh configure was failing while the check said zero.
  What caught it was **ctest reporting 2 tests instead of 78**: the binaries had
  never built, so the harness registered the executables without enumerating
  their cases. Use `grep -cE "error:"`, and treat the **test count** as the real
  gate — a passing count cannot come from binaries that do not exist.
- **`Fixed{n}` and `Fixed::fromInt(n)` differ by 65536**, and choosing wrong is
  silent. Three shapes compiled cleanly and were wrong: `(a >> FRACBITS) * (b >>
  FRACBITS)` is an *integer* product written in `fixed_t` variables (as `Fixed`
  it becomes a fixed-point multiply); `(y2 - y1) / momy` is *integer* division
  (as `Fixed / Fixed` it becomes the saturating `fixedDiv`); and
  `(distance >> FRACBITS) * finecosine[angle]` divided every hitscan's reach by
  65536. The rule that governs serialisation is **"same bytes as before"**, not
  any one spelling — `SaveGame.cpp` writes whole units into a `short*`, so
  `.toInt()` is correct there and `.raw` would corrupt every save.
- **Unsigned strong types change the meaning of a sign test.** `chase()`'s
  `delta` is `int` in vanilla so `delta > 0` is *signed*; as an `angle_t` it
  became an unsigned test, true for every non-zero value, and monsters turned
  the wrong way. demo1 named tic 89.

**The naming session landed, newest last:**

- **`(void)` parameter lists retired** engine-wide (326 sites).
- **303 vanilla function shims retired** and **134 namespaced targets renamed** — retiring the shim
  alone would only have moved the prefix, since the target often still carried it (`Doom::dDisplay`,
  `Doom::I_Error`).
- **The address-pinned families**: the 75 `A_*` state actions moved to `Sim/Actions.{h,cpp}` as
  `Doom::Actions::*` (the `states[]` table needs one pointer shape); the 8 `T_*` thinkers deleted
  outright; the `HUlib_`/`STlib_` adapters deleted by passing references at their 48 call sites.
- **158 internal functions unprefixed** — larger than surveyed, because an earlier pass had
  lowercased-and-concatenated prefixes rather than removing them (`ST_refreshBackground` →
  `strefreshBackground`), which a grep for `_` reads as clean.
- **All 107 `_t` types renamed and namespaced**, in six verified batches.
- **`w_wad.cpp` and `p_maputl.cpp` retired** — the last flat files with real logic. `divline_t` and
  `Doom::DivLine` **merged** (the `reinterpret_cast` bridge is gone), `P_MakeDivline` written rather
  than forwarded, and `W_CacheLumpNum`'s dead `tag` removed along with all 150 `PU_*` arguments.
- **The enemy-action pointer→reference follow-up is green** (58 functions) — this row previously
  recorded it as reverted.
- **`MenuState::messageRoutine` is a `std::function`** with a no-op default, removing
  `startMessage`'s `void*` parameter and six `reinterpret_cast<void*>`.

**Three traps this session added to the list:**

- **Namespacing an unscoped enum moves its enumerators.** `enum MobjFlag` into `namespace Doom` takes
  every `MF_*` with it. The three verbatim generated tables (`Sim/Info.cpp`, `Sim/Items.cpp`,
  `Game/SoundData.cpp`) therefore take a file-scope `using namespace Doom;` — qualifying inside them
  would break the promise to keep that data byte-for-byte.
- **A name collision can be invisible to grep.** `M_Options` → `options` collided with an unscoped
  enum constant of that name and only surfaced in the build. Grep the new name before renaming, and
  do not trust a clean grep as proof.
- **A two-pass rename can silently orphan the thing it meant to delete, and no test will say so.**
  Where the vanilla name and the namespaced name are the *same identifier* (`I_Error` was both
  `::I_Error` and `Doom::I_Error`), pass 1 renamed the shim's own definition, so pass 2 searched for
  the vanilla name, found nothing, and left the definition standing under its new name — 48 dead
  global duplicates (`::initSoundHost()` forwarding to `Doom::initSoundHost()`). They linked cleanly
  and were never executed, so build + 80/80 + goldens *all passed with them present*. Two of the 48
  turned out not to be dead: `Host/Api.cpp` is `extern "C"` at global scope, so its unqualified calls
  were binding to the orphan rather than to `Doom::`. **The lesson: after a rename sweep, count the
  function definitions left in the files you expected to empty — a green suite does not prove the
  deletion half ran.**

**The earlier session landed, newest last:**

- **`info.cpp`→`Sim/Info.cpp`** (the last real vanilla file) with the **`states[].action`
  function-pointer union retired** for a single type-erased pointer the two dispatch
  sites (`setMobjState`/`setPsprite`) cast back to the exact signature — a round-trip,
  hence well-defined. The generated tables stay verbatim under `// clang-format off` + a
  localized `#pragma`.
- **`thinkercap`→`Doom::ThinkerList`** and the **savegame state**
  (`save_p`/`savebuffer`/`savename`)→**`Doom::SaveGameState`** — the two save/thinker-coupled
  clusters the (now-landed) thinker/p_saveg rewrites had been waiting on.
- **The 13 host callbacks** (`doom_print`/`doom_malloc`/… from `doom_config.h`) folded into a
  **`Doom::Host` singleton** (`Host/Host.h`, `host()`), kept deliberately *separate from
  `engine()`* — embedder-set platform state, not world, so it must survive a fresh `Engine`.
  The vanilla names became references onto it, so the ~380 call sites and the `doom_set_*`
  C API are unchanged.
- **A function-local-statics pass**: the world-state function-locals folded into their
  existing clusters — `A_BrainSpit`'s toggle→`EnemyAI`, `HU_Responder`'s chat send state→
  `HudChat`, the automap's animation→`AutomapView`, the bunny-scroll `laststage`→`FinaleState`,
  `M_Responder`'s input debounce→`MenuState`, `TryRunTics`'s `oldentertics`→`NetState`.
- **The last three cross-read flags**: `is_wiping_screen`→`GameFlow`, `inhelpscreens`→
  `OverlayState`, `st_statusbaron`→`StatusBarState`. (The `is_wiping_screen` move first missed
  a bare extern in `Tests/SimProbe.cpp`, and the **menu frame golden caught it at step 0** — the
  net doing its job; the lesson, now recorded, is to grep `Tests/` for bare externs too.)
- **`Game/Sound`'s engine-side playback state**→**`Doom::SoundState`** — the last cohesive
  cluster the file-scope-statics sweep had left loose: the mixing channels (`channels_s_sound`),
  the music-paused flag, the currently-playing music, and the next-cleanup tic. These were
  file-local to `Game/Sound` and read by no other file, so the vanilla names became references
  onto the members (`channel_t` forward-declared in the header, only a pointer held). This is the
  *engine* side of sound (which sounds should be heard); the mixing/MIDI runtime in `Host/Sound`
  stays loose as host state, the same split every Host static keeps. Golden-neutral (sound is not
  hashed and the frame goldens see the picture, not the channels), so verified by 80/80 + goldens
  byte-identical + the app linking, with a `StateClusterTests` accessor-identity check added.
- **Step 9 (modern C++ / RAII) opened**, after the goal was reframed (see the intro + the Step 9
  row): the refactor is not "state is owned" but "code reads as C++ someone wrote," and the manual
  `doom_malloc` owners are the clearest gap. Landed, each golden-verified + app-linked: (1) the
  **one-function scratch buffers** → `EA::Vector` (`Render/Data`'s `patchcount`/`patchlookup`/three
  precache present-arrays — which *fixed a real leak* on `generateLookup`'s early "column without a
  patch" return; `UI/Wipe`'s transpose `dest`; `Game/Config`'s PCX buffer), and (2) the first
  **persistent cluster owners** via the **unified owner/view recipe** — `GraphicsData`'s
  `spritewidth`/`spriteoffset`/`spritetopoffset`, `textureheight`, `texturetranslation`,
  `flattranslation` moved from raw never-freed pointers to owned `EA::Vector`s, the vanilla names
  becoming plain-pointer *views* onto `data()` refreshed after the fill (exactly `Level`'s geometry
  split). That recipe does strands (a) and (b) at once: the array is RAII-owned *and* its
  reference-alias shim is retired — six aliases gone, and every cross-file `extern` moved
  `T*&`→`T*` in lockstep (the load-bearing trap). One friction found and recorded: `EA::Vector`'s
  `SizeType` only accepts `std::integral`, so an enum index (`spritepresent[mobj->sprite]`) needs a
  `static_cast<int>`; `short` indices convert on their own. The recipe then reached the **aligned**
  buffers — `GraphicsData`'s `colormaps` and `DrawState`'s `translationtables`, each a
  `doom_malloc(len + 255)` with a 256-byte-aligned working pointer: the cluster owns the backing
  `EA::Vector` and the vanilla name is the aligned view into `data()` (alignment byte-identical, so
  every `colormaps + row*256` the light tables precompute is unchanged) — and the first **host
  buffer** (`Host/Api`'s `screen_buffer`/`final_screen_buffer` → `EA::Vector`, returned as `.data()`).
  The **nested owners** then followed
  — `GraphicsData`'s `sprites` (`spritedef_t.spriteframes` → `EA::Vector<spriteframe_t>`, the table →
  `EA::Vector<spritedef_t>`) and `textures` (`texture_t`'s flexible-array `patches[1]` →
  `EA::Vector<texpatch_t>`, the table owned by value with a `texture_t*` pointer array the
  `textures` `texture_t**` view points at, so every `textures[i]->field` reader incl. the app is
  unchanged); the vanilla `spritedef_t`/`texture_t` structs now hold owned vectors.
- **The deep tail is now mostly cleared** (this session, each golden-verified): (1)
  **`CompositeCache`'s five per-texture composition tables** — `texturewidthmask`/
  `texturecompositesize` → flat `EA::Vector<int>`s, and the three `T**` tables
  (`texturecolumnlump`/`texturecolumnofs`/`texturecomposite`) → nested owners (an inner vector per
  texture + a pointer-array view), with `Data.cpp`'s reference-alias shims retiring into
  plain-pointer views. **The load-bearing tutti-frutti over-read is preserved**: the lazily-composed
  column bytes keep their 64-byte zero tail, value-initialised by `assign(size + 64, byte(0))` exactly
  as the old `doom_malloc` + `doom_memset(0)` block was, so the renderer's over-read past a composited
  column still draws a deterministic zero. (2) **The software framebuffers behind `screens[]`** —
  `frame` (screens[0], `I_InitGraphics`), `workspace` (the contiguous 4× base block `V_Init` slices
  into screens[0..3]) and `statusBar` (screens[4], `ST_Init`) → `EA::Vector`s on `VideoState`.
  `screens[]` itself (the `byte*[5]`) stays a raw **view** array, because the eacp port legitimately
  reseats `screens[0]` to its overlay-capture scratch and restores it — a pointer that merely refers,
  kept raw per the RAII rules. The `workspace` was already `I_AllocLow`-zeroed so `assign(n, 0)`
  matches it byte-for-byte; screens[0]/screens[4] move from non-zeroing `malloc` to zero-init, proven
  unobservable because the hashed frames fully cover them before any read (the goldens are
  cross-machine reproducible today). (3) **The net comms block `doomcom`** → a `doomcom_t` held by
  value on `NetState` (mirroring `reboundstore`, already by-value there); the vanilla name stays a
  `doomcom_t*` view `I_InitNetwork` points at the owned storage, and `netbuffer` (= `&doomcom->data`)
  is now a stable pointer into the member. Golden-covered: `tryRunTics` reads `doomcom->numnodes`
  every tic. (4) **The sound mixing channels `channels`** → an `EA::Vector<channel_t>` on `SoundState`;
  `channel_t` promoted from a `Game/Sound` file-local struct to `Game/SoundState.h` (owning by value
  needs the complete type), the vanilla name `channels_s_sound` a plain-pointer view refreshed by
  `S_Init`. Crash-covered (every demo fires `S_StartSound`'s channel bookkeeping) and otherwise
  behaviour-preserving by construction. **What is left of the deep tail:** the `Host/Sound` per-sfx
  padding cache (`paddedsfx`, a `+8`-offset per-sound cache, fully audio-blocked so golden-blind —
  left for an audio session), and the level pool (`Sim/Tick`'s intrusive-list, variable-sized,
  polymorphic-`Thinker` allocator, whose memcpy-serialised p_saveg coupling makes it its own reviewed
  step).

Every step held all four `*.hashes`/`*.frames` goldens byte-identical, 80/80 tests, and the
app building and booting.

**The `engine()` flip is now Step 9's strand (a), being taken on — not chased as an in-place reset.**
The tempting shortcut looks like "all world state is a member now, so just reconstruct the `engine()`
singleton for a fresh world." Resist that shortcut. It only *looks* achievable because the
reference-alias architecture forces the Engine to a **fixed address**: the ~489
`extern T& x = engine().cluster.x` aliases bind to member addresses at static-init and can never be
re-pointed, so any in-place reset must happen in the *same storage* — `engine() = Engine{}` (unsafe:
`WadFile` has a user destructor closing raw file handles and no matching assignment, a rule-of-three
violation that would leak the handles) or `~Engine()` + placement-new (works, but is gymnastics that
exists *only* because the address is pinned). And its payoff is thin on its own: the engine already
runs many scenarios per process via **level reload** (Step 4 / `ReplayTests`), which is what scenario
tests actually use. The *clean* "constructed Engine" — a heap-owned instance you drop and remake (an
`OwningPointer<Engine>`, the obvious design) — falls out **for free** once the references are gone,
because then every reader reaches state through an owner/accessor and nothing is bound to a fixed
address. So the constructible engine is not chased as a capstone in its own right; it is what strand
(a) *arrives at* as a by-product of retiring the alias shims. The modernization is the point; the
constructible engine is the dividend.

**A cross-cutting stylistic/modernization pass has landed** — not a numbered step,
but a sweep over the code that was *already* in `namespace Doom` (Math/Sim/Render/
UI/Game/Host/Wad/Engine), turning transcribed-1993-C statements into C++ someone
wrote. It did six things: `typedef struct {…} T;` → `struct T {…};`; `std::array`/
`std::vector` → `EA::Array`/`EA::Vector` (which is what wired `ea_data_structures`
into the build — see Step 3); the `std::size_t` casts those `int`-indexed containers
made unnecessary, gone; `(void)` empty parameter lists → `()`; pointer→reference
where a pointer is provably never null and never reseated (done for the two
self-contained widget libraries `UI/HudWidgets` and `UI/StatusWidgets` and their
shims — the playsim's pointers are mostly *legitimately* nullable and stayed
pointers); and the statement-level cleanup — C-casts → `static_cast`/
`reinterpret_cast`/`const_cast`, `NULL`/pointer-`0` → `nullptr`, C-style loop
counters localized (`int i; … for (i=…)` → `for (int i = …)`, only where the counter
does not escape the loop), and declare-then-assign combined. The Render/Sim/Game/UI
bulk was fanned out to four parallel subagents and verified centrally. **The whole
transform set is behavior-preserving by the C++ standard**, so a mistake could only
surface as a compile error or a golden failure, never a silent behaviour change —
which is why it was safe to parallelise across the golden-pinned engine, and every
gate held (build clean, 80/80, `Tests/Goldens/` byte-identical, the app links). What
it deliberately did **not** touch: the load-bearing `doom_boolean`-is-`int` casts
(`ST_createWidgets`' `(int*) &weaponowned`), `#define`s, code inside inactive `#if`
blocks, `// clang-format off` regions, `(void) x;` value-discards, and the
pointer↔integer alignment / `offsetof` / function-pointer idioms (which want
`reinterpret_cast` and sit at the edge of the rubric).

**The config-backed category is complete.** It was the one bucket blocked all along:
`Config.cpp`'s static `defaults[]` table captured `&member` at static-init, so turning a
config global into an `Engine`-member reference made that a dynamic initializer that raced
the binding across translation units and segfaulted every test. The `doom_config`→`Host`
rework's config half removes the capture at its root: `bindEngineDefaults()` points each
config-backed `defaults[]` entry at its `Engine` member **at runtime** (called at the top of
`mLoadDefaults`/`mSaveDefaults`, before any `location` is dereferenced), so nothing is captured
statically. The app reaches the same members the same way — `eacpDoomBindKeys` writes through
`defaults[].location`, which the runtime bind now points at the members (verified by booting the
app). On that mechanism the config globals moved in: `SoundSettings`
(`snd_SfxVolume`/`snd_MusicVolume`/`numChannels`), `MenuSettings`
(`mouseSensitivity`/`showMessages`/`detailLevel`/`screenblocks`/`usegamma`), `ConfigPaths`
(`basedefault`/`defaultfile` — which turned out never to be captured, so they needed no bind), and
`InputConfig` (all 22 key/mouse/joystick bindings plus the device enables and the crosshair/
always-run toggles).

**The renderer's cross-read view-globals are swept.** The earlier sweep took each `Render/` unit's
*file-local* scratch; what remained were the globals the flat `r_*.cpp` shims still owned and
exported through `r_bsp.h`/`r_draw.h`/`r_segs.h`/`r_things.h`/`r_plane.h`/`r_sky.h`, read across the
renderer (and some by the app). Those moved into new clusters: `BSPScratch` (the BSP-walk
`curline`/`sidedef`/`linedef`/`frontsector`/`backsector` and the `drawsegs`/`ds_p` pool), `SegState`
(`segtextured`/`markfloor`/`markceiling`/the chosen textures/`rw_x`/`rw_stopx`/`walllights`/
`maskedtexturecol`), `SpriteState` (the vissprite pool + the per-sprite clip window), `DrawState`
(the `dc_*`/`ds_*` column and span drawer inputs), `VideoState` (`dirtybox`); `PlaneScratch` gained
the clip/projection arrays (`lastopening`/`floorclip`/`ceilingclip`/`yslope`/`distscale`, kept with
the `openings` `lastopening` points into) and the dead `floorfunc`/`ceilingfunc_t` were deleted;
`SkyState` gained `skytexture`/`skytexturemid`; `HudFlags` took the cross-read
`chat_on`/`message_dontfuckwithme`. **The load-bearing trap the whole renderer sweep turns on:** a
file that declares its own bare `extern int X;` (rather than reaching `X` through the header) and
then *writes* `X` clobbers the low half of the reference's 8-byte pointer once `X` is a reference —
so every such extern must move to `extern T&` in lockstep. It bit once, as the `0x100640000`
`skytexturemid` fault (`Render/Sky.cpp` wrote `100*FRACUNIT` through a plain-`int` extern), and is
found by grepping *every* `extern.*NAME` — headers and `.cpp` bodies alike — before a migration.

The UI, the whole renderer and the config-backed set are done; `info.cpp` (the generated
actor/state LUT), the last real vanilla source, has migrated to `Sim/Info.cpp`, so the
**flat vanilla list is only the shims**. Step 5's save/thinker-coupled state, function-local
`static`s and cross-read flags have all since moved in (see the handoff above), so essentially all
world state is now an `Engine` member — but *owned* is only half of modern C++; the other half is
**reached through an owner, not a global alias**, which is **Step 9** (see the progress table and the
intro's reframed goal). Retiring the ~489 reference-alias shims is the active work; the *in-place
reconstruction* shortcut (placement-new over the singleton) stays the wrong path — it only looks
reachable because the aliases pin the Engine to a fixed address, and level reload already gives
scenario tests a fresh world (Step 4). The clean constructed Engine is not a capstone chased for its
own sake; it falls out **for free** once the aliases are gone (a heap-owned `OwningPointer<Engine>`),
so it is the *dividend* of the modernization, not the point of it. The rest is non-world (the
`mypos`-cheat scratch, the Host-layer runtime statics) or externally blocked (audio; the
`doom_config`→`Host` fold itself is done).

**The zone is gone.** `z_zone.cpp`/`z_zone.h` are deleted. Mobjs and the thinker
specials live in a level-scoped malloc pool (`Sim/Tick`: `levelAlloc`/`levelFree`
over an intrusive list, `freeLevelAllocations` releasing the level whole — every
such allocation is a thinker, and the *list*, not a `thinkercap` walk, is what
makes it leak-free, because `unArchiveThinkers` marks-and-orphans without freeing);
the renderer's boot-once `PU_STATIC` and the scratch buffers are `doom_malloc`; the
WAD and `Level` geometry already owned theirs. Two things carried the change:
`P_SpawnMobj`/`levelAlloc` `doom_memset(0)` matches the OS first-touch zero the
demos recorded (so the composite-cache tutti-frutti over-read stays a deterministic
zero, golden-neutral); and a use-after-free the zone had masked — `runThinkers`
reading a freed block's `next` — was fixed (capture `next` before the free in the
remove case; still advance after the think in the run case, so a mobj spawned
mid-tic runs the same tic). The vestigial `PU_*` tags `W_CacheLumpNum` ignores are
in `w_wad.h` now.

What remains is the deep, interlocking tail, and the p_saveg net is what makes it
safe:

- **The p_saveg net exists** (`doomSimSaveLoadPreservesWorld`): archive the live
  world, reload a fresh base level, unarchive over it — the exact `gDoLoadGame`
  sequence — and require the world back unchanged in every serialized field. This
  is the one simulation path no demo covers, and it is precisely the mobj/special
  byte layout the next step rewrites. It is shown to bite (a corrupted restored
  field fails it). Build any layout change against it.
- **`thinker_t`→`Thinker`** (a real base with a virtual `tick()`) **has landed** — the
  last and deepest playsim step. It made `mobj_t`/the specials polymorphic, which broke
  the memcpy serialisation and the `memset`-over-raw-alloc init, so it needed
  placement-new at every spawner and a vtable-preserving p_saveg, all held by the net
  (Step-8 row). Its *per-object* dispatch union (`thinker_t.function`) is gone.
- **`info.cpp` and the `states[].action` model have landed on top of it** — a separate
  union from the thinker's. `info.cpp` moved to `Sim/Info.cpp` (the generated tables
  verbatim under the strict flags), and `d_think.h`'s three-pointer `actionf_t` union
  (`acp1`/`acv`/`acp2`, the 1993 "ANSI C with classes" hack) became a single type-erased
  pointer the two dispatch sites (`setMobjState`, `setPsprite`) cast back to the exact
  signature — a round-trip conversion, hence well-defined. **With it the flat vanilla
  list is only the shims.** Goldens byte-identical throughout (every mobj/weapon action
  the demos fire runs through the two rewritten sites).

**What exists in modern C++** (`src/DOOM/`, `namespace Doom`, `-Wall` + clang-format;
everything else is still vanilla C compiled as C++ under `-w`):

- `Math/` — `Fixed`, `Angle`, `Trig`, `BBox`, `Vec2`.
- `Sim/` — `Random`, `Level` (level geometry, RAII), `Blockmap` (the grid
  descriptor + block-index arithmetic, owned by `Level`), `Clip` (the movement /
  collision scratch owned by `Engine`: `P_PathTraverse`'s intercept list, the line
  opening window + trace, the `tm*` clipping state, the aim's `linetarget` and
  shot's `attackrange`), `MapGeometry` (`pointOnLineSide` / `pointOnDivlineSide` /
  `interceptVector` / `boxOnLineSide` / `approxDistance` / `lineOpening`), `MapUtil`
  (the callable-taking blockmap iterators, thing linking, `pathTraverse`),
  `Movement` (`checkPosition` / `tryMove` / `teleportMove` / `thingHeightClip`),
  `MapAction` (`slideMove` / `aimLineAttack` / `lineAttack` / `useLines` /
  `radiusAttack` / `changeSector`), `Sight` (`checkSight`), `Interaction` (pickups /
  damage / death), `Player` (`playerThink`), `Mobj` (spawn / mobj thinker /
  missiles), `Weapon` (the `A_*` weapon actions), `Enemy` (the `A_*` monster AI), the
  specials `Lights` / `Plats` / `Ceilings` / `Floors` / `Doors` / `Switches` /
  `Teleport` / `Specials`, and `Tick` (the thinker list and per-tic ticker).
- `Render/` — the whole software renderer: `Sky`, `Data` (texture composition),
  `Main` (view setup, `R_PointToAngle`), `Planes`, `BSP`, `Segs` (wall columns),
  `Things` (sprites / psprites), `Draw` (the column/span blitters). Each is shimmed
  by its flat `r_*.cpp` name, which keeps the `r_state.h` view-state cluster and the
  drawer input state (`dc_*`/`ds_*`) the files share.
- `Wad/` — `WadFile` (owns lumps, RAII).
- `Engine/` — `Engine`, the composition root owning `Random`/`WadFile`/`Level`/`Clip`;
  `randomness()`/`wad()`/`level()`/`clip()` are accessors into the one `engine()`.

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

**The playsim's actor core is fully migrated** — eight flat files rewritten into
`namespace Doom` under `Sim/`, each shimmed by its vanilla-named flat file so the
still-vanilla callers, info.cpp's state table and p_saveg are untouched:

- `MapUtil` (blockmap iterators as callable-taking templates, thing linking,
  `pathTraverse`), `Movement` (`checkPosition`/`tryMove`/`teleportMove`/
  `thingHeightClip`, scenario-test-pinned), `MapAction` (slide/hitscan/use/radius/
  changesector), `Sight` (`checkSight`).
- `Interaction` (pickups/damage/death), `Player` (`playerThink`), `Mobj` (spawn/
  thinker/missiles), `Weapon` (the `A_*` weapon actions), `Enemy` (the `A_*` monster
  AI).

The movement/collision/aim/sight scratch consolidated into `Doom::Clip`; module-
private state that no other file reads became file-local statics (gone from the
global cloud). The pattern for the action files: the `A_*` functions info.cpp
references by address, `P_MobjThinker` and the `T_*` thinker functions p_saveg
identifies by pointer, and `soundtarget` p_saveg archives all stay **global shims**
in the flat file, forwarding to the `Doom::` logic. That preserves every
function-pointer identity the save code and the probe rest on.

**The thinker-container cluster went the shim route** — the same one the action
files used. Each special's `T_*` thinker function stays a global shim in the flat
file (its address is what the spawners store and what p_saveg compares against), and
the logic moved to a `Sim/` unit. That was the state through Steps 6–7; `p_tick`'s
run loop dispatched through the `thinker_t` union until the **`thinker_t`→`Thinker`
virtualisation landed in Step 8** (a real base class with a virtual `tick()`). It was
the deeper change it looks like — mobj_t and the specials became polymorphic, which
touched spawning (`placement-new`), init and p_saveg's byte-serialisation (kept as
memcpy, with the vtable pointer preserved across the copy), all under the demo
goldens — and the append-only probe hash moved from `function.acp1 == P_MobjThinker`
to the virtual `kind()` (a change to *how* it finds mobjs, not *what* it mixes).

**What remains overall:** the deep tail of Step 8 — the `thinker_t`→`Thinker`
virtualisation is **done** (see the Step-8 row), and so now is **`info.cpp` +
the `states[].action` action model** (moved to `Sim/Info.cpp`, the union retired for
a type-erased pointer), which means the flat vanilla list is *only* shims. The
`doom_config`→`Host` fold has since landed too (the 13 callbacks into a `Doom::Host`
singleton). Left is audio (externally blocked on eacp), plus the
*finish* of Step 5. Steps 6 and 7 are complete — every `p_*` and `r_*` file is now a
shim over a `namespace Doom` unit — and Step 5 has migrated ~25 cohesive clusters, so
`doomstat.h`/`r_state.h`/`p_local.h` are nearly empty of loose globals (and the `d_event`/
`d_main`/`p_spec` clusters have followed). The clean export-header tail is now cleared
(`validcount` moved in, DoomMain's boot strings went file-local, dead `drone` removed), and the
**file-scope-statics sweep — the last Step-5 phase — has begun**: g_game's own internal state is
moving into the `Engine` cluster by cluster (`TiccmdInput`, the demo buffer, `DeferredNewGame`,
`consistancy`). What is still left is the config-backed globals (waiting on the config rework —
proven necessary, a naive migration segfaulted every test), `thinkercap` (since migrated as
`Doom::ThinkerList` once the `Thinker` rewrite landed), and the remaining file-scope statics across the UI/renderer/specials files.
The full remaining list is in the Step-8 tail at the end of this document.
The per-file recipe is settled and mechanical: move the logic into a `namespace Doom`
`Sim/` or `Render/` unit, leave the vanilla names as shims, keep as globals only what
another file reads (or identifies by function-pointer address), and run the demos.

**How to verify, every step** (nothing here re-records goldens):

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure     # 80 tests, ~5s
git diff --stat Tests/Goldens/                 # MUST be empty
cmake --build build --target PureDoomEACP      # app still links (touches EngineAccess)
```

`-DPUREDOOM_BUILD_EACP_EXAMPLE=OFF` gives the fast engine-only loop. eacp now
fetches from GitHub `main` by default — the GPU-palette features this port needed
have merged, so no `-DCPM_eacp_SOURCE` override is required (pass one only to
co-develop against a local eacp checkout).

**Traps already paid for, do not rediscover:** ~~`doom_boolean` must stay `int` (not
`bool`)~~ *(superseded — Step 9 flipped it engine-wide and deleted the typedef; what
survives is four specific `int`s listed in `doomtype.h`)*;
`pointOnLineSide` and `pointOnDivlineSide` are different formulae on
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
last reader was disabled by default, and built the eacp app — which needs a GPU CI
does not have. It now builds the engine and the tests, which is
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

**`doom_boolean` is an `int`, and must stay one** — *true as written, for Step 2. Step
9 has since flipped it to `bool` engine-wide and deleted the typedef; what made that
safe was untangling the very cast described below, not discovering it was harmless.
Kept because the failure it records is the reason the flip took four batches and an
audit.* `doomtype.h` already had a
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

The engine uses eacp's containers now: `EA::Vector` and `EA::Array` come from
`ea_data_structures`, which is header-only, `INTERFACE`-only, C++20 and pulls in
nothing — no eacp, no graphics. `CPMAddPackage(eacp)` runs inside
`examples/EACP/CMakeLists.txt`, *after* `add_subdirectory(src/DOOM)`, so the
target would not exist when the engine is configured; instead
`CPMAddPackage(NAME EADataStructures ...)` is hoisted into the **root** list ahead
of the engine, and `doom-engine` links `ea_data_structures` PUBLIC. CPM dedupes by
name, so eacp's own `find_package` reuses it, and the tests keep linking
`doom-engine` (plus that one header-only target) alone. **Landed** in the
stylistic/modernization pass (see the handoff): the trig/random tables became
`EA::Array`, and `Level`'s geometry and `WadFile`'s directory/lump-cache vectors
became `EA::Vector` — `int`-indexed, so the `std::size_t` casts at their call sites
went away with them.

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
is an `EA::Vector` (a `std::vector` when this landed; migrated to eacp's container
in the stylistic pass); the loaders in `p_setup.cpp` `assign` into them and refresh
the vanilla global to `.data()`. **`assign`, not `resize`** — a shorter second level
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

`Doom::Engine` (`Engine/Engine.h`) holds `Random`, `WadFile`, `Level` and now `Clip`
(the movement/clipping scratch, `Sim/Clip.h`, reached through `Doom::clip()`). The three
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

**What moves, and when.** The ~684 scalar globals (`doomstat.h`'s 73,
`r_state.h`'s 44, `p_local.h`'s 27) move cluster by cluster, each *into the `Engine`
once the subsystem that owns it has been rewritten* — the renderer, playsim and UI
now have (Steps 6–8), so the clusters they own are ready to migrate. The first one
has (the view point, below). Aliasing a cluster in *before* its subsystem is rewritten
(`int& gametic = engine().gametic` while `g_game` was still vanilla) would be
golden-neutral but would scatter reference-globals across the transition for no gain;
the `Engine` grows with the rewrite, not speculatively ahead of it.

The end shape, once the clusters have moved in:

```cpp
auto doom = Engine {config, wad};
doom.runTic();
```

### Landed — the first scalar clusters: the renderer's view state

The renderer being fully rewritten (Step 7), its own state is the first of the ~684
scalar globals to migrate — six cohesive clusters so far, each a `Doom::` struct that
is an `Engine` member:

- **`ViewPoint`** (`Render/ViewPoint.h`) — the camera `R_SetupFrame` computes each
  frame: `viewx`/`viewy`/`viewz`/`viewangle`/`viewcos`/`viewsin`/`viewplayer`.
- **`ViewProjection`** (`Render/ViewProjection.h`) — how that view lands on the
  screen, computed by `R_ExecuteSetViewSize`/`R_InitTextureMapping` when the view size
  changes: the screen centre (`centerx`/`centery`/`centerxfrac`/`centeryfrac`), the
  `projection` scale, the field-of-view edge `clipangle`, and the angle↔column tables
  `viewangletox`/`xtoviewangle`.
- **`ViewWindow`** (`Render/ViewWindow.h`) — all the view-sizing state: the region of the
  screen the view fills (`viewwidth`/`viewheight`/`scaledviewwidth`/`viewwindowx`/
  `viewwindowy`), the pending resize request `R_SetViewSize` stashes (`setsizeneeded`/
  `setblocks`), and the applied `detailshift`. (The engine global `viewheight` is distinct
  from `player_t::viewheight`, the player's eye height — that member is untouched;
  `setdetail` stays file-local to `Render/Main`, its only toucher.) The request pair had no
  header extern, only file-scope externs in `Render/Main`/`Game`/`Host` — all confirmed at
  global/function scope, so no Step-8 namespace trap.
- **`Lighting`** (`Render/Lighting.h`) — the light selection: `fixedcolormap` (the row a
  powerup locks the view to) and `extralight` (the muzzle-flash bump) set per frame by
  `R_SetupFrame`, and the diminishing-light lookups `scalelight`/`scalelightfixed`/`zlight`
  built once by `R_InitLightTables`. The tables are references-to-array, so the
  `walllights = scalelight[light]` row assignment the drawers rely on is unchanged.
- **`GraphicsData`** (`Render/GraphicsData.h`) — the graphics `R_InitData` loads from the
  WAD once and only reads after: the composed wall textures and their heights, the flats,
  the sprite lumps and their pre-measured dimensions, the sprite frame table, and the
  `colormaps` base (16 pointer/count tables). The first cluster that is *loaded data*
  rather than per-frame state — init-once and read-only — and the tables are all malloc'd,
  so the vanilla names are plain references, no reference-to-array. Its 14 globals were
  defined in `r_data.cpp` and 2 in `r_things.cpp`, externed across `r_state.h` and
  `r_data.h`; the app's `EngineAccess`, world shader and texture uploader read `textures`/
  `colormaps`/`spritewidth`/`sprites`/`numtextures` and resolve unchanged.
- **`RenderScratch`** (`Render/RenderScratch.h`) — the transient state overwritten as the
  BSP is walked and never persisted: the current wall segment (`rw_distance`/
  `rw_normalangle`/`rw_angle1`), the current subsector's floor/ceiling visplanes
  (`floorplane`/`ceilingplane`), and the subsector counter `sscount`. The last of the
  renderer's own per-frame state; all six were externed only in `r_state.h` with no reader
  outside the renderer. `validcount`, `viewactive`, `linecount` and `viewangleoffset` were
  *left out* on purpose — shared with the playsim/game loop, or vestigial, not
  renderer-owned; they move with those subsystems.

The mechanism is the one `Random`/`Level`/`Clip` used: the storage moves off the
file-scope globals into the struct, and the vanilla names become **references onto the
member** (the header `extern`s become `extern fixed_t& viewx;`, and the two projection
arrays `extern int (&viewangletox)[FINEANGLES / 2];` — a reference-to-array, so the type
and every indexed read are unchanged). Every reader resolves unchanged and no call site
moved: all eight `Render/` units, plus `EngineAccess`/playsim for `viewz`, the app's
world shader for `projection`, and the status bar / HUD / `Game/DoomMain` / app for the
window geometry. `ViewWindow` was the most cross-cutting — its globals were externed
redundantly across `doomstat.h`, `r_main.h` and `r_state.h` (and a file-scope `extern`
in `Game/Config.cpp`), all converted together; a missed one is a safe conflicting-
declaration error, not a silent bug. All four `*.hashes`/`*.frames` goldens held
byte-identical (the frame goldens see the *picture*, not these numbers) and the app
links.

This is the template for the rest: a cohesive cluster → a `Doom::` struct → an `Engine`
member → an accessor → vanilla-name references in the shim, verified by
ctest + goldens-clean + app-link.

**With these six the renderer's own globals are fully migrated.** What is left in
`r_state.h` and `r_main.h` is either a view onto `Doom::Level` (the geometry — `vertexes`,
`segs`, `sectors`, …, migrated back in Step 4) or a symbol that is *not* the renderer's:
the shared pass counter `validcount` and the drawer function pointers
(`colfunc`/`spanfunc`), which the playsim and the game loop write too, and the vestigial
`viewactive`/`linecount`/`loopcount`/`viewangleoffset`. Those move with the subsystem that
owns them — the next bucket is `doomstat.h`'s game state (`gametic`/`gamestate`/`players[]`/
…), whose owner (`Game/`) is already rewritten.

### Landed — the game state begins (`doomstat.h`)

The renderer done, the ~73 `doomstat.h` externs are next, cluster by cohesive cluster —
mostly owned by `Game/Game.cpp` and `Game/DoomMain.cpp` (the state owners, which keep their
globals at file scope above their `namespace Doom`) and reached far more widely than the
renderer's were, across the whole playsim, HUD and game loop. The reference-binding stays
transparent to every reader, so the breadth costs nothing.

- **`LevelStats`** (`Game/LevelStats.h`) — the current level's progress: the intermission
  tallies `totalkills`/`totalitems`/`totalsecret`, the `levelstarttic` the level began at,
  and `leveltime`, the level clock the specials time against. All five were externed only
  in `doomstat.h`; their storage was in `Game/Game.cpp` (four) and `p_tick.cpp`
  (`leveltime`). Unlike the renderer clusters these values *are* mixed into the simulation
  probe's hash — but a reference reads the identical value, so the move is golden-neutral
  all the same, which the demos confirm.
- **`LaunchOptions`** (`Game/LaunchOptions.h`) — the command-line launch flags, doomstat's
  first section: `nomonsters` (-nomonsters), `respawnparm` (-respawn), `fastparm` (-fast)
  and `devparm` (-devparm). Defined in `Game/DoomMain.cpp` above its namespace, externed
  only in `doomstat.h`. The three gameplay modifiers feed the playsim, so they are
  indirectly on the hash's path — golden-neutral through the reference, as ever.
- **`GameVersion`** (`Game/GameVersion.h`) — the loaded game's identity, doomstat's "Game
  Mode" and "Language" sections: `gamemode` (shareware/registered/retail/commercial),
  `gamemission` (the mission pack), `modifiedgame` (a PWAD has been layered on) and
  `language` (the string-table selector). These were the whole content of `Game/State.cpp`
  (the flat `doomstat`→`Game/State` file); its four `::`-scoped definitions become
  references, read unchanged by the ~17 files that use them. None is hashed.
- **`GameSession`** (`Game/GameSession.h`) — the current game's rules, doomstat's "Selected
  by user" skill/map selection and the net flags beside it: `gameskill`/`gameepisode`/
  `gamemap`, `respawnmonsters` (the dead come back), `netgame` and `deathmatch`. Defined in
  `Game/Game.cpp` above its namespace. respawnmonsters/netgame/deathmatch steer the
  simulation, so they are indirectly on the hash's path.
- **`StartupDefaults`** (`Game/StartupDefaults.h`) — the new-game defaults, doomstat's
  "Defaults for menu" half of the skill/map section: `startskill`/`startepisode`/`startmap`
  (filled by -skill / -episode / -warp) and `autostart` (the command line asked for a
  specific start). Defined in `Game/DoomMain.cpp` above its namespace. None is hashed.

The five above were each externed only in `doomstat.h` (no cross-file `extern` to keep in
step), so each was the same three-touch move `LevelStats` was: the header struct, the
`Engine` member and accessor, and the definition line becoming a reference.

Then the clusters with cross-file readers, where the extra `extern`s had to move to
references *in lockstep* — an untouched `extern gamestate_t gamestate;` against a
`gamestate_t& gamestate` definition is not a link error but a silent one, the old declaration
reading the reference's hidden pointer as a value:

- **`PlayerState`** (`Game/PlayerState.h`) — the player roster and view selection:
  `players[]` (every player's state), `playeringame[]` (which slots are live), `consoleplayer`
  and `displayplayer`. The two arrays become references-to-array, so every indexed read —
  including vanilla's own `(int*)` casts into `playeringame`, which lean on `doom_boolean`
  being int-sized — is unchanged. `players`' fields are hashed; a reference reads the same
  bytes. Defined in `Game/Game.cpp`, no extra externs.
- **`GameFlow`** (`Game/GameFlow.h`) — `gamestate` (which screen we are on) and
  `wipegamestate` (the last drawn frame's state, which `D_Display` melts on when it differs).
  `gamestate` from `Game/Game.cpp`, `wipegamestate` from `Game/DoomMain.cpp`; the extra
  externs — `wipegamestate` in `Game/Game.cpp` and `UI/Finale.cpp`, `gamestate` a
  function-body extern in `Host/Api.cpp`'s crosshair draw — all went to references in step.
  `gamestate` is hashed.
- **`DemoState`** (`Game/DemoState.h`) — `usergame` (a real game is running, so save/end are
  allowed), `demoplayback`, `demorecording`, `singledemo`. Defined in `Game/Game.cpp`;
  `demorecording`'s extra externs (`Game/DoomMain.cpp`, `Host/System.cpp`'s `I_Error` demo
  flush) went to references. None is hashed.
- **`RefreshFlags`** (`Game/RefreshFlags.h`) — `paused`, `viewactive` (the 3D view is being
  drawn, false under the full-screen automap), and the `-nodraw`/`-noblit` timing switches
  `nodrawers`/`noblit`. Defined in `Game/Game.cpp`, no extra externs. `viewactive` was left
  out of the renderer's Step-5 clusters as game-loop-owned; this is where it lands.
- **`OverlayState`** (`Game/OverlayState.h`) — `automapactive` and `menuactive`, the two
  overlay flags the loop, HUD and crosshair read together. `automapactive` from `am_map.cpp`
  (a flat renderer shim), `menuactive` from `UI/Menu.cpp`; extra externs updated in step —
  `automapactive` in `UI/HudWidgets`, `UI/Hud` and `Host/Api`'s function-body crosshair
  draw, `menuactive` in `Host/Api` beside it. Neither is hashed; the app's overlay capture
  and crosshair read them through `doomstat.h` unchanged.

Then the rest of the section, into the flat renderer/playsim shims and other owners:

- **`NetState`** (`Game/NetState.h`) — the netcode buffers and tic bookkeeping: `doomcom`
  and `netbuffer`, the command rings `localcmds`/`netcmds`, `nettics`, `maketic` and
  `ticdup`. Defined in `Game/Net.cpp`; the arrays become references-to-array. PureDOOM is
  single-player, but `netcmds[gametic]` feeds each built command to the ticker, so these are
  on the demo path.
- **`MapSpawns`** (`Game/MapSpawns.h`) — the map's spawn spots: `deathmatchstarts`,
  `deathmatch_p`, `playerstarts`. Defined in the flat `p_setup.cpp`. `deathmatchstarts` is
  sized `MAX_DM_STARTS` to match its doomstat extern (p_setup's own `MAX_DEATHMATCH_STARTS`
  is the same 10, so the reference-to-array types agree).
- **`GameClock`** (`Game/GameClock.h`) — `gametic`, the monotonic session tic the whole
  engine times against. From `Game/Game.cpp`.
- **`AmmoLimits`** (`Game/AmmoLimits.h`) — `maxammo`, the per-type carry cap. From the flat
  `p_inter.cpp`, and declared in *two* headers (`doomstat.h` and `p_local.h`) — both externs
  moved to references-to-array, or the untouched one would reintroduce the old `::maxammo`.
- **`IntermissionInfo`** (`Game/IntermissionInfo.h`) — `wminfo`, the `wbstartstruct_t` the
  intermission scoreboard runs on. From `Game/Game.cpp`.
- **`SkyState`** (`Game/SkyState.h`) — `skyflatnum`, the F_SKY1 lump number that means "sky
  here" to the renderer and "vanish silently" to a projectile. From the flat `r_sky.cpp`,
  shared with the playsim and the app's sky detection.

Every cluster verified by ctest + goldens-clean + app-link. The app declares no bare externs
of any migrated global (it reads them through the engine headers, so the reference-externs
reach it), which was checked across `examples/` before the cross-file clusters landed.

**One category will not migrate this way, and it is now proven, not guessed: config-backed
globals.** `Game/Config.cpp`'s static `default_t defaults[]` table captures a global's address
(`{"sfx_volume", &snd_SfxVolume, 8}`) at static-init time. Turn that global into a reference
and `&snd_SfxVolume` stops being a constant — the array element becomes a *dynamic* initializer
whose order versus the reference's own binding is unspecified across translation units, and
when the table wins the race `M_LoadDefaults` writes through a garbage `location`. Attempting to
migrate the two sound volumes did exactly this: **every SimTest segfaulted**, and it was
reverted. So `snd_SfxVolume`/`snd_MusicVolume`/`mouseSensitivity` (and the Menu-owned
`screenblocks`/`detailLevel`/`showMessages`) stay loose globals until the `doom_config`→`Host`
rework, which is where the static address capture goes away. The dead externs the same section
carried — `statusbaractive` and the four `snd_*Device` selectors, none defined or read anywhere
— were simply dropped.

**Then `r_state.h` and `p_local.h`, the other two headers Step 5 empties.** `r_state.h` was
already done: its remaining externs are the geometry *views* onto `Doom::Level` (Step 4), not
loose globals. `p_local.h` was mostly `Doom::Clip` references and blockmap/geometry views too,
but three genuine loose clusters remained:

- **`clipammo` folded into `AmmoLimits`** — the per-pickup ammo amounts, the natural sibling of
  the already-migrated `maxammo` carry caps. `AmmoLimits` now holds both ammo tables. Read on
  every ammo pickup the demos make.
- **`ItemRespawnQueue`** (`Sim/ItemRespawnQueue.h`) — the deathmatch item-respawn ring:
  `itemrespawnque`/`itemrespawntime` (arrays) and `iquehead`/`iquetail`. The first `Sim/`
  cluster besides `Clip`. Single-player demos don't walk it, but its `P_SetupLevel` reset is on
  the level-load path.
- **`CorpseQueue`** (`Game/CorpseQueue.h`) — `bodyque[]` (the retained corpse mobjs) and
  `bodyqueslot` (the insert counter). The first cluster to pull a *file-local* global
  (`bodyque[]`, g_game's own array) in alongside its exported sibling (`bodyqueslot`, doomstat's),
  because the two are one mechanism. `mobj_t` is used only by pointer, so the header just
  forward-declares it. `bodyqueslot`'s `P_SetupLevel` reset is golden-covered.

**Then the `d_main` / `d_event` / `p_spec` clusters — Step 5 past the three export headers.**
With `doomstat.h`/`r_state.h`/`p_local.h` nearly empty, the same reference-alias pattern reaches
the loose globals the *other* headers still carry, each owned by a now-rewritten subsystem:

- **`EventQueue`** (`Game/EventQueue.h`) — the input event ring buffer `events[MAXEVENTS]`/
  `eventhead`/`eventtail`, the asynchronous boundary the host posts through (`D_PostEvent`) and
  the game and netcode drain (`D_ProcessEvents` / `NetUpdate`). Externed in `d_event.h`, defined
  in `Game/DoomMain.cpp`; `events[]` becomes a reference-to-array. Never hashed nor level-reset,
  so golden-neutral by construction.
- **`gameaction` folded into `GameFlow`** — the deferred action `G_Ticker` dispatches each tic
  (load a level, start a game, run a demo, complete the level), the third of g_game's game-flow
  controls beside the already-migrated `gamestate`/`wipegamestate`. Externed only in `d_event.h`,
  defined in `Game/Game.cpp`; transient within a tic, golden-neutral through the reference.
- **`ActiveSpecials`** (`Sim/ActiveSpecials.h`) — the level's active special-effect registries:
  `activeplats`/`activeceilings` (the running mover thinkers the stasis lines pause and resume)
  and `buttonlist` (switch textures counting down to revert). `P_SpawnSpecials` clears all three
  together at the top of every level, which is what makes them one cluster. Each was externed in
  `p_spec.h` and defined in its flat special shim (`p_plats`/`p_ceilng`/`p_switch`), which still
  owns the vanilla name as a reference-to-array. `activeceilings` is archived by the save code, so
  the p_saveg net (`SaveGameTests`) covers this alongside the demos.
- **`EndLevelTimer`** (`Sim/EndLevelTimer.h`) — the deathmatch `-timer`/`-avg` countdown:
  `levelTimer` (running) and `levelTimeCount` (tics left, decremented in `P_UpdateSpecials` until
  `G_ExitLevel`). Externed in `p_spec.h`, defined in the flat `p_spec.cpp` shim. Named
  `EndLevelTimer` so its accessor does not collide with the vanilla member name `levelTimer`.
- **`AttractMode`** (`Game/AttractMode.h`) — the title/demo-loop cycler `D_DoAdvanceDemo` runs
  when no one is playing: `advancedemo` (the advance request), `demosequence` (the cycle index)
  and `pagetic`/`pagename` (the current full-screen page). Defined in `Game/DoomMain.cpp`;
  `advancedemo`'s cross-file externs (`d_main.h`, `Game/Net.cpp`) and `pagename`'s
  (`Game/Game.cpp`) move to references in step. The attract-mode demo replays confirm it neutral.

Two more dead renderer globals went the `viewangleoffset` way rather than into the `Engine`:
**`linecount`/`loopcount`** (r_main's "just for profiling purposes" counters) had no readers or
writers at all — every `linecount` in the tree is the unrelated `sector_t.linecount` member — so
they were simply deleted, goldens byte-identical.

**What remains loose, and why it waits.** In `doomstat.h`: the config-backed scalars
(`snd_*Volume`/`mouseSensitivity`, and `basedefault` the config path) — blocked until the config
rework, because `Config.cpp`'s `defaults[]` captures their address at static-init (proven to segfault
a naive migration). The three scattered internals that section also held — `debugfile`, `precache`
and `singletics` — have since **migrated** into `Doom::EngineParams` (below), doomstat.h's own
"Internal parameters" grouping being organizing principle enough, so the config-backed set is all that
is left loose in doomstat.h. (`viewangleoffset`, the vestigial 3-screen-mode offset that was always
zero, was *deleted* rather than migrated — `viewangle = player->mo->angle` directly, and the psprites
draw unconditionally; goldens held byte-identical.) In `p_local.h`: `thinkercap`, the thinker-list
head, since migrated as `Doom::ThinkerList` once the `thinker_t`→`Thinker` rewrite landed.

Beyond the four export/near-export headers, what is still loose is now a short tail of
single scalars and boot strings, no longer cohesive clusters. **`validcount` has now moved
in** — the one scalar the doc singled out as "genuinely engine-global, owned by no one
subsystem" (r_main's shared renderer/playsim traversal generation counter, bumped by both the
BSP walk and the blockmap/sight/collision passes). It is a `Doom::ValidCount` (`Sim/ValidCount.h`)
`Engine` member reached through `Doom::validCount()`, wrapped in a struct only to keep the
accessor pattern uniform; `r_main.cpp`'s `int validcount = 1` became a reference onto the member
(default `1`), the `r_main.h` extern an `extern int&`, no reader touched, all four goldens
byte-identical. **And DoomMain's boot-path string globals are now off the global cloud.** They were
`::`-scoped only because that is where DoomMain kept its state, not because anyone outside read
them: `wadfiles[]` (the WAD list `D_AddFile` builds and `W_InitMultipleFiles` consumes) had a
`d_main.h` extern but no external `.cpp` reader, and `title[]` (the startup banner) had no
extern at all — both became `static` (file-local), the extern dropped. `wadfile[]` and
`mapdir[]` turned out **dead** — referenced only in comments — and were deleted the
`linecount`/`loopcount`/`viewangleoffset` way, as was the always-unread `drone` flag beside them
(the `doomcom_t.drone` in `d_net.h` is an unrelated struct member). `basedefault[]` stays a
loose global, config-blocked (`Config.cpp`'s `defaults[]` captures its address at static-init).
All four goldens byte-identical.

What is left of the export-header tail: `basedefault[]` (config-blocked, above). `save_p`
(p_saveg's serialization cursor) has since migrated with the savegame buffer/name as
`Doom::SaveGameState`, the p_saveg net (SaveGameTests) having unblocked it. The `am_map`
automap state stays in its shim deliberately, the GPU automap in the app reading it. Beyond the
tail is the config rework (which unblocks the config-backed set) and the long file-scope-statics
phase — which has now begun. (The `Thinker` rewrite that `thinkercap` waited on has since landed,
and `thinkercap` migrated with it as `Doom::ThinkerList`.)

### Landed — the file-scope-statics sweep begins

With the loose globals (header externs, cross-file) nearly all migrated, the next Step-5 work is
the ~1,534 *file-scope statics* and the `::`-scoped **Game-local** globals a state-owner file
keeps to itself — off the global cloud already (no header extern, read by no other file), but
still state reached by free function rather than owned by the `Engine`. Retiring them is what
finally lets the engine be *constructed* rather than booted. They take the same reference-alias
pattern, and being single-file they need no cross-file coordination — only a check that no static
table captures their address (the config trap) before they move.

- **`TiccmdInput`** (`Game/TiccmdInput.h`) — the first. The per-tic input accumulators
  `g_game` builds a `ticcmd` from: `gamekeydown` (held keys), `turnheld` (accelerative-turn
  counter), the mouse/joystick movement (`mousex`/`mousey`/`joyxmove`/`joyymove`) and button
  state (`mousearray`/`joyarray`), and the double-click detection (`dclick*`). All fifteen were
  `Game/Game.cpp`'s own file-scope globals, read by no other file (the `mousex`/`mousey` in
  `UI/Menu.cpp` are unrelated function-local statics), and **none is config-backed** — the
  key/mouse/joy *bindings* (`key_*`/`mouseb*`/`joyb*`) are, and stay loose until the config
  rework; this is the input *state* they drive. The vanilla names became references onto the
  member (arrays as references-to-array); the interior view pointers `mousebuttons`/`joybuttons`
  (`= &mousearray[1]`, to allow a `[-1]` index) stay in `Game.cpp`, now indexing the referenced
  arrays. `mousex`/`mousey` reach the sim through the ticcmd, but a demo overrides the built
  command and the headless suite feeds no mouse, so they are zero throughout — golden-neutral,
  as the four byte-identical goldens confirm.
- **The demo buffer folded into `DemoState`** — the second. `DemoState` already held the demo
  *flags* (`usergame`/`demoplayback`/`demorecording`/`singledemo`, a doomstat.h loose-global
  cluster); g_game's own file-scope demo *buffer* state — `demobuffer` (the byte block), `demo_p`
  (the read/write cursor `G_ReadDemoTiccmd`/`G_WriteDemoTiccmd` walk), `demoend` (the end guard),
  `demoname` (the `-playdemo`/`-record` name) and `netdemo` — folds in beside them, one
  demo-subsystem owner (the way `clipammo` folded into `AmmoLimits`). All five were `Game/Game.cpp`
  file-scope, read by no other file; the vanilla names became references onto the members
  (`demoname` a reference-to-array, the buffer pointers references-to-pointer). Unlike
  `TiccmdInput` this is squarely on the demo replays' path — they drive `demobuffer`/`demo_p` end
  to end — so its byte-identical goldens are a live confirmation, not just an absence.
- **`DeferredNewGame`** (`Game/DeferredNewGame.h`) — the third. Starting a game cannot happen
  inside the responder that asks for it, so `G_DeferedInitNew` (the menu's skill/episode pick, or
  `-warp`/`-skill`) stashes `d_skill`/`d_episode`/`d_map` and raises `gameaction = ga_newgame`;
  `G_Ticker` replays them into `G_InitNew` when the tic runs. All three were `Game/Game.cpp`
  file-scope, read by no other file and not config-backed; the vanilla names became references
  onto the member. `gameaction` (the action they are the payload of) is already in `GameFlow`;
  these are kept a small cluster of their own so `GameFlow` stays the screen/wipe/action state.
  Golden-neutral.
- **`consistancy` folded into `NetState`** — the fourth. The per-player, per-tic desync checksum
  `G_BuildTiccmd` stamps into the command it sends and `G_Ticker` compares on the way back
  (`I_Error` on a mismatch). It looked cross-file, but the `consistancy` in `d_ticcmd.h` and
  `Host/Net.cpp` is the unrelated `ticcmd_t` member — the global array is `Game/Game.cpp`'s own,
  read by no other file. It is netcode tic-bookkeeping, so it folds into `NetState` beside the
  command rings; the seven doomstat.h members still bind in `Game/Net.cpp`, `consistancy` binds
  in `Game/Game.cpp`, each at its definition site. Unlike the new-game params it is squarely on
  the demo path — the consistency check runs every replayed tic — so the byte-identical goldens
  are a live confirmation.
- **`ParTimes`** (`Game/ParTimes.h`) — the fifth. The par times: `pars[gameepisode][gamemap]` (the
  DOOM episode/map table) and the flat DOOM II `cpars[gamemap - 1]`, one of which `G_DoCompleted`
  reads into `wminfo.partime` for the intermission scoreboard. Both were `Game/Game.cpp`'s own
  file-scope tables, read by no other file and never written — fixed reference data, but off the
  loose globals all the same so the `Engine` owns every table rather than the process. The vanilla
  names become references-to-array onto the members. Read-only and reached only when a level
  completes, so golden-neutral.
- **`MovementSpeeds`** (`Game/MovementSpeeds.h`) — the sixth. The tables `G_BuildTiccmd` applies to
  the player's input: `forwardmove`/`sidemove` (walk/run forward and strafe) and `angleturn` (the
  per-tic turn, fast/faster/slow-turn). `MAXPLMOVE` is `forwardmove[1]`. Unlike the par times these
  are *not* purely fixed data — `-turbo` scales `forwardmove`/`sidemove` at startup, so they are
  genuine per-session state; that write lives in `Game/DoomMain.cpp`, which is why those two are the
  only ones of the three read outside g_game. Their DoomMain externs moved to references-to-array
  *in lockstep* (`int(&)[2]` == `fixed_t(&)[2]`, so `-turbo` writes through to the member — an
  untouched plain-array extern against a reference definition would silently read the reference's
  hidden pointer as the array). `G_BuildTiccmd` is the only consumer, and the headless suite
  overrides the built command in demo playback (and never passes `-turbo`), so golden-neutral.
- **`TimeDemo`** (`Game/TimeDemo.h`) — the seventh. The `-timedemo` benchmark state: `timingdemo`
  (the flag `G_TimeDemo` raises to play a demo flat out) and `starttime` (the wall clock
  `I_GetTime` captures at level start in `G_DoLoadLevel`), from which `G_CheckDemoStatus` reports
  the frame rate on demo end. `starttime` is written every level load but read only on that path.
  Both were `Game/Game.cpp`'s own file-scope state, read by no other file; the vanilla names become
  references onto the members. `-timedemo` is never passed in the headless suite, so `timingdemo`
  stays false and `starttime` is written-but-unread — golden-neutral.
- **`PendingCommands`** (`Game/PendingCommands.h`) — the eighth. The deferred special-command
  requests `sendpause`/`sendsave`: a pause (the pause key) or a save (menu save) cannot be applied
  inside the event that asks for it, so the flag is raised and next tic `G_BuildTiccmd` folds it
  into the ticcmd as a `BT_SPECIAL` command (`BTS_PAUSE`/`BTS_SAVEGAME`) and clears it. Both were
  `Game/Game.cpp`'s own file-scope state; `m_menu` (`UI/Menu.cpp`) carried a *dead*
  `extern doom_boolean sendpause;` it never used, dropped with the move, so nothing reads either
  flag outside g_game. The vanilla names become references onto the members. The headless suite
  never pauses or saves (demos feed the ticcmd directly), so both stay false — golden-neutral.
- **`HudMessage`** (`UI/HudMessage.h`) — the ninth, and the first out of g_game and into the UI.
  The HUD message line (item pickups, keys, chat lines): `w_message` (the scrolling-text widget),
  `message_on` (a message is showing — the widget binds to it), `message_counter` (tics left before
  it clears) and `message_nottobefuckedwith` (a message a lower-priority one may not overwrite).
  `HU_Ticker` pulls `plr->message` into the widget and times it out. **The first cluster that was
  already *file-local*** — a `static` inside `UI/Hud`'s namespace, not a `::`-scoped loose global —
  so it was off the global cloud but still process-static state a second `Engine` would share,
  which is exactly what the sweep's file-static half retires. (The "two message flags" the
  `hu_stuff` shim owns and other files read are the separately-externed
  `chat_on`/`message_dontfuckwithme`; these four are read by no other file.) Unlike the g_game
  shims, `UI/Hud.cpp` is in the rewritten set (`-Wall` + clang-format), so the aliases are
  warning-clean and formatted. The message widget is drawn into `screens[0]` every tic and the
  demos pick items up, so this is **live** frame-golden-covered — the byte-identical goldens are a
  live confirmation, not just an absence.
- **`HudChat`** (`UI/HudChat.h`) — the tenth, the chat half of `UI/Hud`. The multiplayer chat
  state: `w_chat` (the local input line), `chatchars`/`head`/`tail` (the outgoing keystroke ring
  the netcode drains), `w_inputbuffer` (each remote player's incoming text), `chat_dest` (who each
  is addressing), and `always_off` (the permanently-false flag the input buffers bind their cursor
  to — assembled from the wire, never shown as editable). All seven were file-local statics.
  PureDOOM ships single-player, so no demo drives the chat — golden-neutral. (`HU_Responder`'s send
  path keeps a few *function-local* statics — `lastmessage`, the destination-key table — for a
  later function-local pass; this is the file-scope half.)
- **`StatusBarFace`** (`UI/StatusBarFace.h`) — the eleventh, the first cluster out of
  `UI/StatusBar`. The animated face's selection state (`ST_updateFaceWidget`): `st_faceindex` (the
  patch shown — `w_faces` binds to it), `st_facecount` (how long an expression holds), `st_oldhealth`
  (last tic's health → a pained face on a big drop), `oldweaponsowned` (last tic's weapons → the evil
  grin), and `st_randomnumber` (a per-tic `M_Random` the straight face varies with). The demos take
  damage, grab weapons, rampage and die, so the face animates into `screens[0]` — **live**
  frame-golden-covered.
- **`StatusBarWidgets`** (`UI/StatusBarWidgets.h`) — the twelfth. The ten STlib widgets
  `ST_createWidgets` binds and `ST_drawWidgets` paints each tic: `w_ready`, `w_ammo`/`w_maxammo`,
  `w_health`/`w_armor`, `w_arms` over `w_armsbg`, `w_faces`, `w_keyboxes`, `w_frags`. The addresses
  the STlib calls take (`&w_ready`, …) resolve to the members; live frame-golden-covered.
- **`StatusBarGraphics`** (`UI/StatusBarGraphics.h`) — the thirteenth. The patches `ST_loadGraphics`
  reads from the WAD (`sbar`/`armsbg`, `tallnum`/`tallpercent`, `shortnum`, `keys`, `arms`,
  `faces`/`faceback`) — init-once, read-only, the status bar's `GraphicsData`. `faces` is sized by
  the StatusBar-local `ST_NUMFACES`, so the struct carries a `numFaces` constexpr equal to it and
  the reference-to-array binding self-checks the two — a drift won't compile.
- **`StatusBarState`** (`UI/StatusBarState.h`) — the fourteenth; it **empties `UI/StatusBar` of data
  file-scope statics**. The status bar's residual runtime state in four loosely-related threads:
  lifecycle/timing (`plyr`, `st_firsttime`, `veryfirsttime`, `lu_palette`, `st_clock`,
  `st_msgcounter`, `st_stopped`), which layout is drawn (`st_gamestate`, the deathmatch flags,
  `st_fragscount`, `keyboxes`), the "t"-to-talk chat line (`st_chatstate`/`st_chat`/`st_oldchat`/
  `st_cursoron`) and the palette flash (`st_palette`). `ST_Start`/`ST_initData` reset most before
  use, but the defaults reproduce vanilla's zero/one/true initializers exactly.
- **`HudState`** (`UI/HudState.h`) — the fifteenth; it **empties `UI/Hud`**. What was left after the
  message line and chat: the console player (`plr`), the level-title line (`w_title`, drawn on entry
  — frame-golden-covered), and `headsupactive` (the once-per-level setup gate).
- **`AutomapView`** (`UI/AutomapView.h`) — the sixteenth. `UI/Automap`'s own view state, distinct
  from the globals the `am_map.cpp` shim exports for the GPU automap: the pan/zoom increments and
  limits, the level's map bounds, the saved window for resize recovery, the follower's old location,
  the frame→map scale, the placed marks, and the open/closed flag — 27 statics. Named `AutomapView`
  because `st_stuff.h`'s `st_stateenum_t` already has a global enum value `AutomapState` that would
  be ambiguous under `using namespace Doom` (the `StateClusterTests` build caught it). **Not
  frame-golden-covered** (no demo opens the automap), but a reference alias is pure storage
  relocation — the compiler binds each name to its same-named member — so it is behaviour-preserving
  by construction; verified by build + app-link, with `StateClusterTests` pinning the non-trivial
  defaults. The "iddt" cheat stays a file-local `static` (its `cheatseq_t` points at its own byte
  array, which does not survive being a copyable member — an m_cheat concern).
- **`IntermissionState`** (`UI/IntermissionState.h`) — the seventeenth, the first out of
  `UI/Intermission`. `wi_stuff`'s residual runtime state and loaded graphics: the timing/count-up
  state machine (`state`/`acceleratestage`/`cnt`/`bcnt`, the per-stage `dm_state`/`ng_state`/
  `sp_state`, the count-up accumulators `cnt_*`), the passed-in scoreboard data (`wbs`/`plrs`/`me`),
  and the ~30 patches `wiLoadData` reads from the WAD plus the malloc'd `lnames` — 48 of the file's
  54 statics. The other six — the animation/layout data tables `lnodes`/`epsd0/1/2animinfo`/
  `NUMANIMS`/`anims_wi_stuff` — **stay file-local**, because `anims_wi_stuff` is a table of pointers
  *into* `epsd*animinfo`, the same self-referential-pointer trap the "iddt" cheat hit (a copyable
  struct member holding a pointer to its own storage does not survive being copied), so that whole
  group is left for a later pass. **Not frame-golden-covered** (no attract demo or menu replay
  reaches the intermission), but a reference alias is pure storage relocation — behaviour-preserving
  by construction; verified by build + app-link, with `StateClusterTests` pinning the one
  non-zero-looking default (`stateenum_t` zero-init lands on `StatCount`, since `NoState` is −1) and
  the accessor wiring. Shown to bite: defaulting `state` to `NoState` fails `otherClusterDefaults`.
- **`FinaleState`** (`UI/FinaleState.h`) — the eighteenth, out of `UI/Finale`. The finale's mutable
  runtime state: the animation stage/count (`finalestage`/`finalecount`), the chosen text and
  background flat (`finaletext`/`finaleflat`), and the DOOM II cast call's per-monster bookkeeping
  (`castnum`/`casttics`/`caststate`/`castdeath`/`castframes`/`castonmelee`/`castattacking`) — 11
  `namespace Doom`-scope globals, read by no other file. The finale's **immutable reference data
  stays file-local**: the per-ending text pointers (`e1text`..`t6text`, each aliasing a
  string-literal macro) and the `castorder[]` cast list are fixed constants, not per-run state, and
  carry no self-reference to unwind (`caststate` points into the global `states[]`, not into any of
  them) — so migrating 22 literal-pointer aliases and the cast table would be bulk for no gain. **Not
  golden-covered** (no demo or menu replay reaches a finale); verified by build + app-link, with the
  accessor wiring pinned.
- **`WipeState`** (`UI/WipeState.h`) — the nineteenth, out of `UI/Wipe`. The screen melt's two
  file-local scratch framebuffers: `wipe_scr` (the working frame the composite is built into) and
  `wipe_scr_end` (the incoming frame melted in from). The melt's *exported* globals —
  `wipe_scr_start`/`wipe_melt_offsets`/`wipe_melt_running` — stay in the `f_wipe.cpp` shim, being
  what the GPU compositor reads through `f_wipe.h`; the same split `AutomapView` used (app-read
  globals exported in the shim, private scratch into the `Engine`). Unlike the finale and
  intermission this **is** frame-golden-covered — `G_DoLoadLevel` wipes at every level load, so the
  demo frame goldens see the melt, and they held byte-identical (a live confirmation, not just
  build + app-link; the melt walks `M_Random`, not `P_Random`, so it never touches the simulation
  hash regardless).
- **`MenuState`** (`UI/MenuState.h`) — the twentieth, out of `UI/Menu` (the biggest UI file). The
  menu's transient interaction state: the skull cursor's menu/item/blink (`currentMenu`/`itemOn`/
  `skullAnimCounter`/`whichSkull`), the screen-size preview (`screenSize`), the pop-up message box
  (`messageString`/`messx`/`messy`/`messageNeedsInput`/`messageRoutine`/`messageLastMenuActive`) and
  the savegame string editor (`saveSlot`/`saveCharIndex`/`saveStringEnter`/`savegamestrings`/
  `saveOldString`), plus `quickSaveSlot`/`endstring`/`tempstring`/`epi` — 20 fields, all UI/Menu's
  own namespace-scope private globals, read by no other file. **Three groups stay put**: the
  config-backed / cross-read globals (`screenblocks`/`detailLevel`/`showMessages`/`mouseSensitivity`/
  `inhelpscreens`/`messageToPrint`, and `menuactive` — already an `OverlayState` reference) keep
  their `::` file scope above the namespace (the config-backed ones blocked until the config rework);
  the immutable reference-data tables (the gamma/skull/detail/message lump-name tables, the quit
  sounds, the custom-text segments) stay file-local; and the **self-referential menu-definition
  apparatus** — every `*Menu[]`/`*Def` table (`prevMenu` cross-links them, `lastOn` is written as the
  user navigates) and the `OptionsMenu`/`SoundMenu` variant selectors (whose `menuitem_t` element
  type is an anonymous-struct typedef that cannot be forward-declared) — stays file-local, the same
  trap the intermission's animation tables and the automap cheat hit. `currentMenu` migrates because
  `menu_t` is a *named* struct (`menu_s`) that forward-declares cleanly. Unlike the finale and
  intermission the menu **is** golden-covered — `Tests/Goldens/menu.frames` drives a scripted walk
  through nearly every branch and hashes every tic, and it held byte-identical (a live confirmation).

**Then the sweep reached the renderer's and playsim's own file-local state** — the private globals
Steps 6–7 moved *file-local* into the `Render/`/`Sim/` units (as `namespace Doom` globals with no
header extern, read by no other file — the flat shims re-export only the cross-read names). These are
all **live golden-covered**: the demos draw every wall and fight every monster, so the frame/`.hashes`
goldens exercise them directly.

- **`CompositeCache`** (`Render/CompositeCache.h`) — `Render/Data`'s texture-composition working
  data: the PNAMES patch bounds, the per-texture composite-column cache (`texturewidthmask`/
  `texturecompositesize`/`texturecolumnlump`/`texturecolumnofs`/`texturecomposite`) and the flat/
  texture/sprite memory counters. Distinct from `GraphicsData` (loaded-once, read-only): this is the
  composition machinery over it, generated lazily. 12 fields.
- **`WallScratch`** (`Render/WallScratch.h`) — `Render/Segs`'s per-wall-segment rendering
  intermediates: the masked-texture flag, the seg centre angle / texture offset, the scale and step,
  the three texture mid heights, the world top/bottom/high/low edges, and the running top/bottom
  column fractions and steps. Distinct from `RenderScratch` (the `r_state.h`-externed BSP-walk
  scratch). 20 fields.
- **`ActionScratch`** (`Sim/ActionScratch.h`) — `Sim/MapAction`'s p_map "action" scratch: the
  slide/hitscan/use/radius/change-sector working state vanilla kept global so the `PIT_*` blockmap
  callbacks could see it (17 fields). Its `mobj_s`/`line_s` forward declarations sit at **global
  scope** — inside `namespace Doom` they would be distinct `Doom::` types that would not bind to the
  vanilla `mobj_t`/`line_t` (caught at build).
- **`WeaponScratch`** (`Sim/WeaponScratch.h`) — `Sim/Weapon`'s per-tic bob offset (`swingx`/`swingy`)
  and the hitscan auto-aim slope (`bulletslope`). 3 fields.
- **`SightScratch`** (`Sim/SightScratch.h`) — `Sim/Sight`'s line-of-sight scratch: the looker eye z,
  the looker→target trace divline, the target point, and the reject/real test counters. 5 fields.

A second pass finished the renderer and took most of the rest of the playsim:

- **`SpriteScratch`** (`Render/Things`), **`DrawTables`** (`Render/Draw` — `ylookup`/`columnofs`/
  `fuzzpos`), **`RenderMainState`** (`Render/Main` — `framecount`/`setdetail`), **`SolidSegs`**
  (`Render/BSP` — the occlusion clip ranges; its `cliprange_t` element type *moved to the header* so
  `solidsegs` could be a member, an anonymous-struct typedef otherwise), and **`PlaneScratch`**
  (`Render/Planes` — the visplane pool, the `openings` silhouette scratch, the span cache). Two
  details `PlaneScratch` settled: `lastvisplane`/`newend`/`lastopening`-style pointers point *into* a
  sibling array but are reset each frame at runtime (not by a self-referential initializer), so they
  are safe as members; and `openings` is a member while the header-externed `lastopening`
  (`Render/Segs` reads it) stays in the `r_plane` shim, `R_ClearPlanes` just pointing it at the member
  array. Its big arrays grow the `Engine` from 118 KB to 259 KB — fine, far under the main-thread
  stack the `EngineTests` second `Engine` sits on. The dead vestigial `ceilingfunc` was *deleted*.
  **With these, every `Render/` file's private scratch is in the `Engine`.**
- **`EnemyAI`** (`Sim/Enemy` — the archvile-resurrection and DOOM II boss-brain scratch; not
  demo-exercised, the shareware episode has neither, so verified by build not goldens),
  **`SwitchList`** (`Sim/Switches` — the wall-switch texture table), **`PlayerScratch`** (`Sim/Player`
  — the one `onground` flag; distinct from `PlayerState`, the roster).

A third pass took the two `Sim/` files the second had deferred, and doomstat's last loose scalars:

- **`AnimatedSurfaces`** (`Sim/Specials`) — the level's animating flats/textures (`anims`/`lastanim`)
  and the scrolling-texture linedef list (`numlinespecials`/`linespeciallist`). The `anim_t` untangle
  it was deferred for: the type was defined *twice* — a dead file-scope typedef (a leftover of the
  namespace wrap, unused at global scope) beside the real namespace one; the dead copy was **deleted**
  and `anim_t` moved into the cluster header. `lastanim` points into `anims` but is reset at runtime by
  `P_InitPicAnims`, so it is safe as a member.
- **`LevelPool`** (`Sim/Tick`) — the level malloc-pool list head (`levelChunks`, the zone
  replacement's intrusive allocation list; the `LevelChunk` node type moved to the header with it).
  Deferred earlier as "moves with the `Thinker` rework", but it is a clean single-pointer migration on
  its own, and making it a member is a real step toward the pool being owned *per-Engine* rather than
  by the process.
- **`EngineParams`** (`Game/EngineParams.h`) — doomstat.h's "Internal parameters, used for engine"
  section: `debugfile` (the `-debugfile` handle), `precache` (load all graphics at level load) and
  `singletics` (one built-and-run tic per loop iteration). REFACTOR.md had deferred these three as too
  scattered to bucket; they migrate because doomstat.h itself groups them, and it clears doomstat.h's
  last *migratable* loose globals. `singletics` is golden-covered (a wrong `true` default would desync
  the demos — it is the load-bearing five-tic-lag fix); `precache` is golden-neutral (preloading
  changes only *when* a lump is read), so its `true` default is pinned by `StateClusterTests`.

Each file's *immutable* reference-data tables (the enemy direction tables, the BSP `checkcoord`, the
`Sim/Specials` `animdefs`/`switchlist` WAD-name tables, …) stay file-local as before. At the time this
was written doomstat.h's only remaining loose globals were the config-backed set
(`snd_*Volume`/`mouseSensitivity`/`basedefault`), blocked on `Config.cpp`'s static address capture —
**that block is now removed and the whole config-backed set has migrated** (see the runtime-bind note
in the handoff and the Step-8 tail); doomstat.h is off the loose-global cloud.

Alongside these, a **test net for the migrated clusters** landed (`Tests/Sim/StateClusterTests.cpp`,
in `PrimitiveTests`, which boots nothing). Most clusters are pinned by the demo/frame goldens — the
HUD and status bar draw every tic — but the par-time and movement-speed *tables* are golden-neutral
(demo playback overrides the built ticcmd and rarely reaches a par readout) yet load-bearing, so a
mistyped default would pass every golden. The tests pin those tables against the 1993 values, the
other clusters' non-trivial defaults (`TimeDemo`/`PendingCommands` clear, `HudChat` ring empty,
`StatusBarFace`'s −1 sentinel, `StatusBarGraphics`' 42 faces, `StatusBarState`'s one-time gate,
`IntermissionState`'s `StatCount` zero-init), and that the free accessors view the one `Engine`'s
members (the guarantee `EngineTests` makes for random/wad/level). Shown to bite: corrupting E1M1's
par (30 → 31) fails `parTimesMatchVanilla`; defaulting `state` to `NoState` fails
`otherClusterDefaults`.

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

### Landed — `p_maputl` and `p_map`, fully migrated

The stateful half of `p_maputl` — the blockmap iterators, thing linking and
`P_PathTraverse` — is now `Sim/MapUtil`, and `p_maputl.cpp` a shim. Following the
established shim pattern (a genuinely-rewritten module's logic moves to a subdir
under `-Wall` + `clang-format`; the flat vanilla-named file stays a `-w` shim), the
rewrite lives in `Sim/` rather than `p_maputl.cpp` itself flipping out of `-w`. The
iterators are templates (`forEachLineInBlock`/`forEachThingInBlock`) taking any
callable — the payoff a rewritten caller uses to pass a lambda capturing its own clip
state; today the shims and `MapAction` pass function pointers, so behaviour is
identical.

`p_map` split in two: `Sim/Movement` (the collision core — `checkPosition`,
`tryMove`, `teleportMove`, `thingHeightClip`, with `PIT_CheckLine`/`CheckThing`/
`StompThing` as file-local statics fed to the callable iterators) and `Sim/MapAction`
(slide, hitscan aim/shoot, use, radius, sector-change). `p_map.cpp` went from 1296
lines to a 184-line shim. Its `tm*` clipping state joined `Clip`; the ~17
slide/hitscan/radius globals no other file reads became file-local statics in
`MapAction` and left the global cloud; only `linetarget` and `attackrange` (read by
`p_mobj`/`p_pspr`) stayed, as `Clip` members with vanilla-name references. The
scenario tests pin `checkPosition`/`tryMove` directly, and every demo fires, slides
and crushes through the rest — all four `*.hashes`/`*.frames` goldens held
byte-identical across the split.

Next in the module: `p_mobj`, and with it the `thinker_t` → `Thinker` rewrite.

## Step 7 — The renderer

Pinned by the frame goldens from Step 0. `r_data` → `r_bsp` → `r_segs` →
`r_plane` → `r_things` → `r_draw` → `r_main`.

The software renderer stays. The GPU path in `examples/EACP` is the real renderer
now, but the software frame is still the fallback outside a level, the source of
the composited status bar, and what the overlay capture draws into.
`EngineAccess` reaches directly into `r_state.h`, `r_bsp.h` and `r_data.h`, so it
moves with each renderer change — it is ours, and the intent is that it gradually
stops being a reach-around and becomes the engine's actual interface.

### The recipe (proven on `r_data`)

The renderer files carry the same shim shape as the playsim, with two wrinkles the
playsim did not have:

- **Split the module globals by whether another file reads them.** The renderer's
  state is a mix: `viewx`, `textures`, `colormaps`, `spritewidth`, the core
  `r_state.h` cluster — read by every renderer file and by `EngineAccess` — stay
  defined in the flat shim (they have `extern`s in a header, and the `Doom::` code
  reaches them through it). But a file's *own* private globals (`r_data`'s composite
  cache — `texturecomposite`/`texturecolumnofs`/…, its patch/flat counts and memory
  counters, 12 in all) have no header `extern` and no other reader, so they move
  *file-local* into the `Render/` unit. The test is mechanical: `grep` the name in
  `*.h` — a hit means keep it in the shim, a miss means move it in.
- **Build the app after every renderer commit.** `EngineAccess.cpp` calls the `R_*`
  functions and indexes the `r_*` globals; because the shims keep both, it keeps
  linking, but that is the thing to verify (the sim tests do not link it).

Two frame-golden traps carried from Step 4: `getColumn`'s tutti-frutti over-read is
load-bearing — do not "fix" it — and it held byte-identical here; and the many
literal lump names the loaders pass (`W_CacheLumpName("PNAMES")`, …) want their
callee taking `const char*` (done for `W_CacheLumpName`/`W_GetNumForName`/
`W_CheckNumForName`/`M_CheckParm`) so the rewritten code is `-Wall`-clean.

**Landed:** `Sky`, `Data` (the foundational one), `Main` (view setup, the load-bearing
`R_PointToAngle`), `Planes`, `BSP`, `Segs` — all frame-golden-clean and app-linking.
The globals split was automated: a name is kept in the shim if a header `extern`s it
*or* any other `.cpp` reads it (checked against every source file, playsim and app
included — the drawer pointers `colfunc`/`spanfunc`, the pending-view flags, the
view state all stay), and moved file-local otherwise.

**Landed the last two, `r_things` and `r_draw`, with the whole-file depth-0 scan.**
The preamble-scan shortcut (extract the block of globals before the first function,
split it) works for a file whose globals sit in one place. Both of these scatter
theirs: `r_things` defines `mfloorclip`/`mceilingclip` mid-render, and `r_draw`
defines the span-drawer globals (`ds_x1`, `ds_source`, …) *after* the column drawers,
past several functions. A mid-file global lands in the `namespace Doom` body and
becomes `Doom::x`, which the shim's header `extern` and the other files that switch it
can no longer link to. The finish is a **whole-file depth-0 global scan**: collect
every top-level `type name…;` (not just the preamble), classify each with the same
header-or-cross-reference test, and lift the kept ones out of the namespace into the
flat shim.

Two traps the classification had to see through, both false positives on a bare
name-grep: `r_draw`'s `columnofs`/`ylookup` (the frame-address tables) looked read
by other files, but every hit was a `patch->columnofs` struct-member access or a
comment — the real globals live only in the drawers, so they moved file-local. And
`r_draw`'s `translations[3][256]` looked read by `Sim/Mobj`, but that too was a
comment; it was dead (as were `viewimage`, `dccount`, `dscount`, and the fuzz
drawer's texture step, which never samples a pixel). `r_draw`'s drawer functions
themselves (`R_DrawColumn`/`R_DrawSpan`/…) are ordinary `R_*` shims — `Main`'s
`colfunc = …` assignments already store the shim address, so the function-pointer
dispatch keeps working. `r_things` kept `sprites`/`numsprites` (app reads them) and
the vissprite pool + `mfloorclip`/`spryscale` window (`r_segs` reads them) in the
shim; `spritelights`/`sprtemp`/`maxframe`/`spritename`/`overflowsprite` moved
file-local.

**Step 7 is complete:** all eight `r_*` files (`Sky`, `Data`, `Main`, `Planes`,
`BSP`, `Segs`, `Things`, `Draw`) are `namespace Doom` `Render/` units shimmed by
their flat vanilla names — all frame-golden-clean and app-linking. The globals split
was automated: a name is kept in the shim if a header `extern`s it *or* any other
`.cpp` genuinely reads it (checked against every source file, playsim and app
included — the drawer pointers `colfunc`/`spanfunc`, the pending-view flags, the view
state all stay), and moved file-local otherwise.

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

### Landed so far — the UI and the game loop

Ten flat files are now `namespace Doom` units shimmed by their vanilla names, in
two new subdirs `UI/` and `Game/`. The recipe is the same shim shape as the
renderer, with the whole-file depth-0 global scan where globals scatter:

- **`f_wipe`→`UI/Wipe`**, **`hu_lib`→`UI/HudWidgets`**, **`st_lib`→
  `UI/StatusWidgets`**, **`hu_stuff`→`UI/Hud`**, **`st_stuff`→`UI/StatusBar`**,
  **`am_map`→`UI/Automap`**, **`wi_stuff`→`UI/Intermission`**, **`f_finale`→
  `UI/Finale`** — the screen/HUD/effect files. The status bar, HUD and melt are
  pinned by the frame goldens; the automap, intermission and finale are not (no
  demo opens them), so those are faithful cp-and-relocate transcriptions verified
  by build + app-link. `st_statusbaron` became a real shared global (the app had
  been externing a file static across a linkage mismatch); `am_map`'s shapes /
  window state stay in the shim because the GPU automap reads them.
- **`g_game`→`Game/Game`** and **`d_main`→`Game/DoomMain`** — the game loop.
  Both are golden-covered (the demos drive `G_*` end to end and `D_Display` runs
  every tic), and both are the *state owners* of the core game globals: neither
  has a single file-static. Rather than scatter ~90 shared globals into the shim
  with a matching extern wall, those globals stay defined at file scope in the
  rewritten unit, above its `namespace Doom` — still `::`-scoped, so `doomstat.h`
  and every reader resolve unchanged. The trap this exposed: an `extern` of a
  *global* symbol that lands inside `namespace Doom` (a function-local
  `extern int always_run;`, or a scattered mid-file definition) becomes
  `Doom::always_run` and fails to link — those move to global scope, before the
  namespace.

Two `-Wall`/`-Wextra` patterns recurred across these and were fixed
behaviour-neutrally: **`-Wwritable-strings`**, from the pervasive 1993 idiom of
`char*` pointing at string literals — the fix is `const char*` on the read-only
lump/file-name paths (`R_TextureNumForName`/`R_FlatNumForName`, `D_AddFile`,
`G_DeferedPlayDemo`, `pagename`, `doomwaddir`), the same const cleanup
`W_CacheLumpName` had; and **`-Wmissing-field-initializers`** on legitimate
partial-init data tables (`wi_stuff`'s anim tables), silenced with a localized
`#pragma GCC diagnostic ignored`. `player_t.message` was already `const char*`
in the tree, so message assignments were clean.

Then the game loop, netcode and the utility layer followed the same shape — the
KEEP-heavy ones (`g_game`, `d_main`, `d_net`, `v_video`, `m_misc`) keep their
state at file scope in the rewritten unit above the namespace (they are the state
*owners*), the small ones (`m_cheat`, `m_argv`, `s_sound`) move it file-local:

- **`v_video`→`Render/Video`** (the framebuffer blitters; frame-golden pinned),
- **`m_cheat`→`UI/Cheat`**, **`m_argv`→`Game/Args`**, **`m_misc`→`Game/Config`**
  (config/file I/O; `M_LoadDefaults` reads the test config, frame-golden pinned),
- **`g_game`→`Game/Game`**, **`d_main`→`Game/DoomMain`**, **`d_net`→`Game/Net`**
  (ticcmd building, the loop, the netcode — all golden-covered end to end;
  `NetUpdate`'s singletics quirk preserved verbatim),
- **`s_sound`→`Game/Sound`** (audio state, exercised each tic though unheard).

### Landed — `m_menu`, behind a golden built for it first

`m_menu` was the one file with **no test coverage at all** — nothing in a demo
opens a menu — so it got the Step-0 treatment: widen the net *before* touching
the code. `Tests/SimProbe` grew a menu harness (`doomSimBootToTitle`,
`doomSimPostKeyDown`/`Up`, `doomSimStepTic`, and `doomSimIsWiping`/`GameState`/
`MenuActive` accessors); `Tests/MenuReplay.h` scripts a player's walk through the
menus over the attract-mode title screen — options and their toggles, the
thermometer sliders, the mouse and sound submenus, episode/skill select, the help
pages, load/save, the quit prompt and the title-screen F-keys — and hashes the
finished software frame **every tic** into `Tests/Goldens/menu.frames` (86
frames, 45 distinct). The title screen is a static, deterministic picture, so the
golden pins the menu drawn over it and nothing else; the entry wipe is run out
before hashing starts. The script **never commits** — it answers the quit and
nightmare prompts "no" and never starts, loads or saves a game — so `gamestate`
stays `GS_DEMOSCREEN` and the process is never taken down, while nearly every
branch of `M_Responder`/`M_Drawer`/`M_Ticker` still runs. It was shown to bite by
mutation (a one-pixel `SKULLXOFF` shift fails `menu.frames` while every demo
golden sails through), and shown to bite the *rewritten* code the same way. The
darken-background path (`DOOM_FLAG_MENU_DARKEN_BG`, off by default) and the
no-mouse/no-sound menu variants are covered by app-run, which sets that flag.

Then the rewrite: **`m_menu`→`UI/Menu`**, the same shim shape. The seven globals
another subsystem reads — `menuactive`, `inhelpscreens`, the config-backed
`screenblocks`/`detailLevel`/`showMessages`/`mouseSensitivity`, and
`messageToPrint` (the eacp overlay capture reads it) — stay defined at file scope
above the namespace, `::`-scoped so `doomstat.h`/`Game/Config.cpp` and the
renderer resolve unchanged; the ~40 private globals (the menu-definition tables,
the skull cursor, the message and save-string state) moved into `namespace Doom`.
The vanilla `M_` names are kept *inside* the namespace so the 2,000-line
transcription stays diffable against the 1993 source; `m_menu.cpp` is a five-line
shim forwarding `M_Responder`/`M_Ticker`/`M_Drawer`/`M_Init`/`M_StartControlPanel`
to their `Doom::` counterparts. The Step-8 extern trap bit again — the
function-local `extern int crosshair;`/`always_run;` and the drawer's
`extern ... screens`/`colormaps` would have become `Doom::` members — so those
moved to file scope above the namespace. `-Wwritable-strings` (the string-drawing
helpers took `const char*`) and `-Wmissing-field-initializers` (the localized
`#pragma`, as `wi_stuff`) were the recurring fixes. All four `*.frames`/`*.hashes`
goldens held byte-identical and the app links.

### Landed — the host layer begins (`Host/`)

The host boundary is where the engine calls out to the platform. Its files start
moving into a new `Host/` subdir on the settled shim shape, cheapest and most
golden-covered first:

- **`i_video`→`Host/Video`** — the video seam. A thin host stub in PureDOOM (the
  eacp app does the drawing, reading `screens[0]` and the palette back out), but
  golden-covered where it counts: `screen_palette` and `screens[0]` are on the
  frame-hash path and `I_SetPalette` runs on every flash the demos replay.
  `screen_palette` stays `::`-scoped for its many readers; the vestigial X11
  mouse-warp globals were dead and dropped.
- **`i_system`→`Host/System`** — timing, the zone's backing allocation,
  startup/teardown and `I_Error` (the abort the probe catches through
  `doom_set_exit`). `mb_used`/`emptycmd` moved file-local.

Both keep the vanilla `I_` names inside the namespace and a flat `i_*.cpp` shim,
same as the menu. This is orthogonal to the `doom_config`→`Host` redesign below:
moving the engine's `I_` seams into namespace units is the routine step every
file took; reshaping the *host-provided* hooks is the separate structural pass.

### Landed — the host boundary finished, the remainders cleared, the net built

The rest of the host boundary is now in `Host/`, and the two flat files that were
never really C — the tiny data/decl remainders — are gone:

- **`i_sound`→`Host/Sound`** and **`i_net`→`Host/Net`** — the audio and network
  seams, on the settled shim shape. Both are thin (PureDOOM has no audio backend
  and ships single-player, so the socket code stays behind `I_NET_ENABLED` and the
  mixer runs unheard), so they are *faithful relocations* verified by build +
  app-link rather than by goldens; `mixbuffer` stays `::`-scoped in `Sound.cpp`
  for its one reader (`DOOM.cpp`). The unwired-stub warnings the vanilla `-w`
  suppressed (unused params, dead statics) are marked `[[maybe_unused]]`, not
  removed.
- **`DOOM.cpp`→`Host/Api`** — the host-boundary centrepiece and the one file with
  **no forwarding shim**: the public API is `extern "C"`, so its linker symbols
  are global whichever TU defines them, and the flat `DOOM.cpp` is simply deleted.
  `DOOM.h`/`doom_config.h` stay flat (the public interface, never reflowed). A
  byte-for-byte relocation, proven by diffing before clang-format; the `extern "C"`
  function-pointer defaulting compiles `-Wall -Wextra -Wpedantic` clean as-is.
- **The small remainders** — `m_swap`→`Math/Swap.h` (constexpr `swap16`/`swap32`;
  `SHORT`/`LONG` forward there only under `__BIG_ENDIAN__`), `doomstat`→`Game/State`
  (the four identity globals), `dstrings`' `endmsg` folded into `UI/Menu` (its one
  reader, made `const char*`), and the empty `doomdef.cpp` deleted.
- **The ready data tables** — `d_items`→`Sim/Items` (`weaponinfo`) and
  `sounds`→`Game/SoundData` (`S_sfx`/`S_music`), pure externed data with no action
  pointers, so migrate-able now (a localized `#pragma` + `// clang-format off`
  keeps the 1993 rows verbatim under the strict flags). `info.cpp` stayed deferred at
  the time — its `states[]` holds the action pointers — and **has since migrated** to
  `Sim/Info.cpp` (see *Landed — the action model and `info.cpp`* below).
- **The p_saveg save/load net** (`Tests/Sim/SaveGameTests.cpp` +
  `doomSimSaveLoadPreservesWorld`) — the enabler for everything below. p_saveg's
  archive/unarchive is the one simulation path no demo covers, and it is exactly
  the mobj/special byte layout the `Thinker` step and the mobj/special
  zone-ownership change rewrite. The net archives the live world, reloads a fresh
  base level and unarchives over it (the `gDoLoadGame` sequence), then requires a
  comprehensive world hash unchanged. Shown to bite.

### Landed — the action model and `info.cpp`

`info.cpp` (the 4,669-line multigen-generated state/mobjinfo/sprite LUT) was **the last
real vanilla source** — the only flat file that was not a shim over a `namespace Doom`
unit. It is now `Sim/Info.cpp`, migrated the way `d_items`→`Sim/Items` and
`sounds`→`Game/SoundData` were: the tables stay at `::` scope (their `info.h` externs are
read across the engine and the app), verbatim under a `// clang-format off` guard and a
localized `#pragma` that quiets the three warnings legitimate data of this shape raises
under the strict flags — `-Wwritable-strings` (`sprnames` binds literals to `char*`),
`-Wcast-function-type` (each action is stored via an `(actionf_p1)` cast), and
`-Wmissing-field-initializers`. **With it the flat vanilla list is only the shims.**

The **action model** it bundled with — `state_t.action`, the function the state machine
runs when a mobj or weapon enters a state — was `d_think.h`'s three-pointer *union*
`actionf_t` (`acp1`/`acv`/`acp2`), the 1993 "ANSI C with classes" hack: the generated
table stores every action in the `acp1` slot via a cast, and the two dispatch sites read
whichever member matches their context (`Sim/Mobj`'s `setMobjState` reads `acp1` and calls
with `(mobj_t*)`; `Sim/Weapon`'s `setPsprite` reads `acp2` and calls with
`(player_t*, pspdef_t*)`) — the same bits, a union pun, and undefined behaviour when the
call goes through a type the pointer was not created with. This is a **different union
from the thinker's** (that one was per-object *tick* dispatch, retired in Step 8; this is
per-state *action*), and it was the only remaining user of the `actionf_*` types.

It became a single type-erased pointer: `struct actionf_t { actionf_p1 fn; }`, with the
two dispatch sites casting `fn` back to the exact signature at the point of call —
`reinterpret_cast<void (*)(mobj_t*)>(st->action.fn)(mobj)` and the weapon equivalent.
That is a *round-trip* conversion (the table stored the function as its own type erased to
`actionf_p1`; the call restores exactly that type), which is well-defined, unlike the
union pun. Three things made this small and safe rather than a 967-row table rewrite:

- **The storage type is kept `actionf_p1`** (`void (*)(void*)`), so the generated table's
  `{(actionf_p1)Func}` / `{0}` initializers aggregate-init the new struct's `fn` member
  **verbatim** — not one data row changed. Only `d_think.h`, the two call sites, and the
  file's home moved.
- **The typed call lives at the two sites, not on the struct.** `pspdef_t` is an
  anonymous-struct typedef (no tag to forward-declare) that itself holds a `state_t*`, so
  a `pspdef_t`-typed call method could not sit on `actionf_t` in the early-included
  `d_think.h` without an include cycle. Casting at the call sites also puts the
  load-bearing well-definedness where a reviewer sees it. The dead `actionf_v`/`actionf_p2`
  typedefs, the union, and the unused `think_t` alias were dropped.
- **`states[]` stays a contiguous array.** `SaveGame` serializes a state as its *index*
  (`mobj->state - states`, `&states[i]`), never via the action pointer, and the struct is
  the same size as the union (three same-size pointers → one), so `state_t`'s layout is
  byte-identical and the save path is untouched.

Verified the usual way: 80/80 tests, **all four `*.hashes`/`*.frames` goldens
byte-identical**, the app links. This is *live* golden coverage, not an absence — every
mobj and weapon action the demos fire (combat, pickups, doors, the player's weapon) runs
through the two rewritten dispatch sites, and the `states[]` checksum test (which
deliberately skips the address-valued `.action` field) still matches.

**Steps 6 and 7 are complete**, Step 8's mechanical migrations are done, **the zone is
deleted**, and — with the `Thinker` virtualisation and the `info.cpp`/`states[].action`
action model now both landed — **the flat vanilla list is only the shims**. What remains
is the deep, interlocking tail:

- **The zone is gone** (Step 4), in three moves that kept `mobj_t` byte-identical
  (no vtable). The two easy buckets — the renderer's boot-once `PU_STATIC` tables
  (`Render/{Data,Things,Draw,Planes}`) and the scratch buffers
  (`UI/{Wipe,Intermission,StatusBar}`, `Game/{Game,Config,Sound}`, `Host/Sound`) —
  became `doom_malloc` (behaviour-identical: `PU_STATIC` was never freed). The
  composite-cache tutti-frutti came out golden-neutral: `PU_STATIC` was OS
  first-touch-zeroed, so the demos recorded a zero over-read, which
  `doom_malloc` + a 64-byte zero tail reproduces.

  The hard bucket — mobjs and thinker specials — is **atomic** (a `doom_free` on a
  still-`Z_Malloc`'d block corrupts the heap) and was **not** the trivial "free
  through the thinker list" swap it first looked like. `removeMobj` frees *lazily*,
  and `unArchiveThinkers` clears the world by marking every fresh mobj then
  emptying `thinkercap` **without** freeing them; the zone reclaimed those orphans
  at the next `Z_FreeTags(PU_LEVEL)`, and only a **list of every allocation**
  (`Sim/Tick`'s `levelChunks`), not a `thinkercap` walk, catches them the same way.
  `levelAlloc` `doom_memset(0)`s to match the first-load zero. And one latent
  use-after-free the zone had masked surfaced: `runThinkers` read a freed block's
  `next` — fixed by capturing `next` before the free in the remove case while still
  advancing after the think in the run case (so a mid-tic spawn runs the same tic).
  The vestigial `PU_*` tags moved to `w_wad.h`.
- **`thinker_t`→`Thinker`** virtualisation and **`info.cpp`** — both **now landed**
  (the `Thinker` in Step 8; the action model / `info.cpp` on top of it, see *Landed —
  the action model and `info.cpp`* below). The plan they were built to, kept as record: a
  real base with a virtual `tick()` makes `mobj_t`/the specials polymorphic, which
  breaks the memcpy serialisation and the `memset`-over-raw-alloc init (now
  `doom_memset(0)` in `levelAlloc`/`P_SpawnMobj`), so it needs placement-new at
  every spawner and a field-wise p_saveg, all held by the net. `info.cpp`'s
  `states[]` action union is exactly what the *action-model* half replaced (a
  *different* union from the thinker's per-object dispatch — one is per-state action,
  the other per-object tick). Neither was required for "no zone" (done) or for the flat
  list to empty; each dispatch was already a clean golden-pinned choke point, so the
  payoff was modest and the risk real — which is why they were left until last.

  Concretely, so the next pass can size it: it is **atomic** (nothing compiles or
  passes until it is all done) and touches, by grep, ~52 `function.acp1`/`acv`/`acp2`
  sites, ~51 `(mobj_t*)`/`(thinker_t*)` casts, ~32 `->thinker.` accesses, ~28
  `&…->thinker` sites and ~33 `P_AddThinker`/`P_RemoveThinker` calls — because embedding
  `thinker_t` as `mobj_t`'s first member has to become inheritance (`mobj_t : Thinker`),
  so `mobj->thinker.next` becomes `mobj->next`, `&mobj->thinker` becomes `mobj`, and the
  union dispatch becomes `th->tick()`. Two sentinels currently *encoded in* `function`
  need real members: removal (`function.acv == (actionf_v)(-1)`, read in `runThinkers`
  and mid-tick in `P_MobjThinker`) → a `removed` flag; and in-stasis plats/ceilings
  (`function.acv == 0`, the parked state `EV_StopPlat`/`EV_CeilingCrushStop` set and
  `archiveSpecials` keys on) → a `parked` flag or a no-op `tick()`. The type-identity
  comparisons `function.acp1 == P_MobjThinker` / `== T_MoveCeiling` / … (in `SaveGame`,
  `Enemy`, `Teleport`, `Render/Data`, the app's `EngineAccess`, and — the append-only
  hash — `SimProbe`) become a `dynamic_cast`/virtual-`kind()` check; the probe may change
  *how* it finds mobjs but not *what* it mixes. p_saveg's whole-struct `doom_memcpy` is
  the crux: the clean way is placement-new the object on load (to establish the vtable),
  then copy only the POD data region after the `Thinker` base — a plain memcpy of the
  vtable across two objects of the same dynamic type happens to work in-process but is UB
  and against the refactor's clean-C++ ethos. It was a focused, review-warranting effort,
  not a mechanical sweep — done last, on exactly that foundation (the p_saveg net, the
  level pool, the golden-pinned dispatch).
- **The `doom_config`→`Host` redesign + audio.** The **config half is done** — the thing
  it was really needed for. `Config.cpp` no longer captures option-global addresses at
  static-init; `bindEngineDefaults()` points each config-backed `defaults[]` entry at its
  `Engine` member at runtime (top of `mLoadDefaults`/`mSaveDefaults`, before any `location`
  is read), and the app reaches them the same way, through `defaults[].location`. That is
  what unblocked the config-backed category (now migrated — see below). **The host-pointer
  fold is now done too**, leaving only audio of this bullet: (1) the 13 host function pointers
  (`doom_print`/`doom_malloc`/… in `doom_config.h`) are folded into a `Doom::Host` struct
  (`Host/Host.h`) reached through `host()` — **a separate singleton from `engine()` on
  purpose**, since these are host/platform state set once by the embedder before `doom_init`
  and must *not* be reset when a fresh `Engine` is constructed. The feared churn did not
  happen: the vanilla names became references onto the `Host` members (the same reference-alias
  the Engine clusters use), so the ~380 call sites, the `doom_set_*` C API and `doom_init`'s
  null-defaulting are all unchanged — a pure loose-global cleanup, 80/80 goldens byte-identical
  and the app boots on its custom callbacks. (`error_buf` and `doom_flags` stay loose: the
  former is I_Error scratch, the latter a single `doom_init` argument, neither a callback.)
  What is *left* of this bullet is (2) **wiring audio (gap-log item 1), which is externally blocked** — the
  engine side is fully built (`Host/Sound` mixes SFX and translates MUS→MIDI; `Api.cpp`
  exposes `doom_get_sound_buffer`/`doom_tick_midi`), but *nothing drains it*: eacp has no
  audio subsystem, and this repository must not modify eacp (porting rule). So audio playback
  cannot be finished from here — it waits on an eacp audio stream, at which point the pull/
  push wiring under *What the engine expects of its host* connects the two.
- **The globals-into-`Engine` work** (Step 5) — **the loose globals are all in, and
  the file-scope-statics sweep is well advanced** (the UI and the whole renderer are swept, and most
  of the playsim). `doomstat.h` is down to the config-backed set, `r_state.h`/`p_local.h` nearly
  empty: 67 clusters have moved into `Engine` (see "the game state
  begins" and "the file-scope-statics sweep begins" under Step 5), each a `Doom::` struct reached
  by an accessor with the vanilla names as references onto the members. The clean export-header
  tail is cleared: `validcount` moved in (the lone engine-global scalar), DoomMain's boot-path
  strings went file-local `static` or were deleted, and the dead `drone` was removed. What is
  left:
  - **Config-backed globals — DONE.** The runtime bind (above) removed the static address
    capture, and on it `SoundSettings`/`MenuSettings`/`ConfigPaths`/`InputConfig` migrated in:
    the sfx/music volumes and channel count, the mouse-sensitivity/message/detail/screenblocks/
    gamma settings, the config file paths, and all 22 input bindings + device enables + toggles.
    doomstat.h and the config option globals are off the loose-global cloud.
  - **The renderer's cross-read view-globals — DONE.** `BSPScratch`/`SegState`/`SpriteState`/
    `DrawState`/`VideoState`, `PlaneScratch`'s clip/projection arrays, `SkyState`'s
    `skytexture`/`skytexturemid`, and `HudFlags`'s `chat_on`/`message_dontfuckwithme` — the state
    the flat `r_*.cpp` shims exported through their headers.
  - **`thinkercap`** — **now migrated** as `Doom::ThinkerList` (an `Engine` member reached
    through `thinkerList()`), the `thinker_t`→`Thinker` rewrite it waited on having landed:
    the `p_tick.cpp` shim's `thinker_t thinkercap` became `thinker_t& thinkercap =
    thinkerList().cap`, the `p_local.h` extern an `extern thinker_t&`, and the engine-wide
    readers (`Sim/Tick`, `SaveGame`, `Enemy`, `Render/Data`, the app's `EngineAccess`) resolve
    unchanged. Live simulation-golden-covered. **`save_p` and the savegame orchestration state
    (`savebuffer`/`savename`) have since migrated too**, as `Doom::SaveGameState` (an Engine
    member) — the p_saveg net (SaveGameTests), which sets `save_p` and walks it through
    P_ArchiveThinkers, making the serialization cursor's move safe; both its externs (p_saveg.h
    and the bare one in Sim/SaveGame) moved to `extern byte*&` in lockstep.
  - **The rest of the file-scope statics** (~1,534 in all) and the remaining Game-local globals,
    which the reference-alias pattern also fits — the sweep has cleared g_game's cohesive
    clusters (`TiccmdInput`, the demo buffer into `DemoState`, `DeferredNewGame`, `consistancy`
    into `NetState`, `ParTimes`, `MovementSpeeds`, `TimeDemo`, `PendingCommands`) and has reached
    the UI's file-local `static`s — emptying `UI/Hud` (`HudMessage`/`HudChat`/`HudState`) and
    `UI/StatusBar` (`StatusBarFace`/`StatusBarWidgets`/`StatusBarGraphics`/`StatusBarState`) of data
    statics, then the automap's own view state (`AutomapView`), the intermission's residual state
    (`IntermissionState`, its self-referential animation-data tables left file-local), the finale's
    runtime state (`FinaleState`, its immutable text/cast reference data left file-local), the
    melt's scratch framebuffers (`WipeState`) and the menu's transient interaction state
    (`MenuState`, its config-backed globals and self-referential menu-definition apparatus left
    file-local), and then **every `Render/` file's** file-local scratch (`CompositeCache` /
    `WallScratch` / `SpriteScratch` / `DrawTables` / `SolidSegs` / `PlaneScratch` / `RenderMainState`)
    and the playsim's (`ActionScratch` / `WeaponScratch` / `SightScratch` / `EnemyAI` / `SwitchList` /
    `PlayerScratch` / `AnimatedSurfaces` / `LevelPool`), plus doomstat's internal-parameter scalars
    (`EngineParams`: `debugfile` / `precache` / `singletics`), with a `StateClusterTests` net for the
    golden-neutral tables. Since then `soundtarget` (`Sim/SoundTarget`), the whole of `Game/Net`'s
    private bookkeeping (folded into `NetState`), and `Game/Sound`'s engine-side playback state
    (`Doom::SoundState`: the mixing channels, the music-paused flag, the currently-playing music, the
    next-cleanup tic — the *engine* side of sound, the mixing/MIDI runtime in `Host/Sound` staying
    loose as host state) have moved in too, so **every cohesive cluster is now migrated**. The loaded-once **asset pointers are now in too**: `hu_font` (the heads-up font, a
    `Doom::HudFont` cluster — a loose global the `hu_stuff.cpp` shim owned and `UI/Hud`/`UI/Menu`/
    `UI/Finale`/`Game/Config` read by bare extern, all moved to references-to-array in lockstep) and
    `sttminus` (the status-bar number widget's minus sign, a `Doom::StatusWidgetGraphics` cluster kept
    the widget library's own rather than folded into `StatusBarGraphics`, so `UI/StatusWidgets` stays
    self-contained). **The function-local pass has now begun** — the golden-covered ones first: the
    status-bar **face-drawer's counters** (`lastcalc`/`oldhealth`/`lastattackdown`/`priority`, folded
    into the existing `StatusBarFace` cluster — the same face subsystem, just declared inside its
    functions), the `w_ready` **n/a-ammo sentinel** (`largeammo`, folded into `StatusBarWidgets`, which
    the widget's `num` pointer points at), and **`D_Display`'s frame-diff state** (the six-static
    border/overlay-redraw machine `viewactivestate`/`menuactivestate`/`inhelpscreensstate`/`fullscreen`/
    `oldgamestate`/`borderdrawcount`, a new cohesive `Doom::DisplayState` cluster). Each is a member with
    a matching default reached by a local reference in its function — vanilla never resets them, so the
    persistence is identical in a single-Engine process; all three are live frame-golden-covered and held
    the goldens byte-identical, with `StateClusterTests` pinning the non-trivial sentinels. **The pass
    has since taken the world-state function-locals that no shareware demo exercises**, each a member
    reached by a local reference in its function, folded into the existing cluster of its subsystem:
    `A_BrainSpit`'s boss-brain skill toggle (`easy` → `EnemyAI`), `HU_Responder`'s send-path state
    (`lastmessage`/`shiftdown`/`altdown`/`num_nobrainers` → `HudChat`), the automap's animation and
    level-change detection (`lastlevel`/`lastepisode`/`bigstate`/`nexttic`/`litelevelscnt` →
    `AutomapView`), the bunny-scroll ending's `laststage` (→ `FinaleState`), `M_Responder`'s mouse/
    joystick input debounce (`joywait`/`mousewait`/`mousex`/`mousey`/`lastx`/`lasty` → `MenuState`), and
    `TryRunTics`'s `oldentertics` (→ `NetState`). Off the golden net, so verified by build + 80/80 +
    app-boot (a reference alias is behaviour-preserving by construction); the immutable tables and pure
    drawing scratch beside them (`destination_keys`, `litelevels`, the automap's reused `fline_t`/
    `mline_t` buffers) stay file-local. **The scattered *cross-read* single flags have since all moved
    in too**, each into the cross-read cluster of its subsystem: **`is_wiping_screen`** (the screen-melt-
    in-progress flag) into **`GameFlow`** beside `wipegamestate` — its four sites (the `Game/DoomMain`
    definition, the `Host/Api` and app `EngineAccess` externs, and the `SimProbe` extern) all moved to
    `doom_boolean&` in lockstep, the fourth (in `Tests/`) missed on the first pass and the **menu frame
    golden caught it at step 0** (a bare extern read the reference's pointer bits as a bool, so the boot
    wipe "never finished") — the goldens doing exactly their job, and the lesson to grep `Tests/` for bare
    externs too, not just `src/DOOM` and `examples`; **`inhelpscreens`** (a full-screen help page is up)
    into **`OverlayState`** beside `menuactive`/`automapactive`, golden-covered by the menu replay's help
    pages; and **`st_statusbaron`** (the main bar is drawn) into **`StatusBarState`**, its four sites the
    shim definition, the `UI/StatusBar` and app `EngineAccess` externs, and the STlib widgets that cache
    `&st_statusbaron` (which now yields the member's stable address) — heavily golden-covered, the bar
    drawing every tic. **What genuinely remains** is a short tail of non-world statics: the `mypos`-cheat
    message buffer and similar pure drawing-scratch statics, and — deliberately staying out of the Engine
    — the **Host layer's own runtime statics** (`I_GetTime`'s `basetime`, the sound handle counter, …),
    which are host state, not world. With the cross-read flags in, **essentially all mutable world state
    is now an `Engine` member.**

    Beyond the tail is **Step 9** — the modernization half of the goal (see the progress table) — and
    its strand (a), retiring the reference-alias shims, is what *finally* lets the engine be
    **constructed** rather than booted. The shape of it is worth being precise about, because there is a
    tempting wrong version. The **clean** end state is that every reader reaches state through an
    owner/accessor (or an `Engine&` it was handed) rather than a static-init-bound vanilla-name reference
    (`extern fixed_t& viewx = engine().viewPoint.viewx`) — those references bind to member *addresses*
    before `main()` and can never be re-pointed, which is the sole reason `engine()` is pinned to a fixed
    address today. The **tempting shortcut** — "all world state is a member now, so just **reconstruct the
    singleton** for a fresh world" — is the wrong path, not the next step. Because the address is pinned, a
    reset cannot make a new instance (a heap `OwningPointer<Engine>` remade elsewhere would strand every
    reference on the freed object); it would have to happen in the *same storage* — either
    `engine() = Engine{}` (unsafe: `WadFile` has a user destructor closing raw file handles and no matching
    assignment — a rule-of-three violation that leaks them) or `~Engine()` + placement-new (works, but is
    gymnastics that exists only because the address is pinned). And on its own it buys little: the engine
    already runs many scenarios per process via **level reload** (Step 4 / `ReplayTests`), which is what
    scenario tests actually use. So the constructible engine is not pursued as a capstone in its own right:
    the *clean* one — a heap-owned instance dropped and remade, no placement-new — falls out **for free**
    once strand (a) has retired the aliases, because then nothing is bound to a fixed address. The
    modernization is the point; the constructible engine is the dividend.

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
