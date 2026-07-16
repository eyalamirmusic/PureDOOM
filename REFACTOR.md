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
| 4 | Ownership: kill the zone allocator | **done** — `z_zone.cpp`/`z_zone.h` deleted. Mobjs and thinker specials moved to a level-scoped malloc pool (`Sim/Tick`'s `levelAlloc`/`freeLevelAllocations`); the renderer's boot-once `PU_STATIC` and the scratch buffers to `doom_malloc`; the WAD and `Level` geometry already owned theirs. The vestigial `PU_*` tags `W_CacheLumpNum` ignores moved to `w_wad.h`. All goldens byte-identical throughout |
| 5 | The `Engine` object: globals become members | **in progress** — composition root owns `Random`/`WadFile`/`Level`/`Clip`/`ViewPoint`; `Clip` holds all of p_maputl's + p_map's movement/collision scratch (blockmap descriptor on `Level`, intercept list, opening window + trace, the `tm*` clipping state, the aim's `linetarget` and shot's `attackrange`); `ViewPoint` holds the renderer's camera (`viewx`/…/`viewplayer`), `ViewProjection` its screen projection (`centerx`/…/`projection`, the `viewangletox`/`xtoviewangle` tables), `ViewWindow` the view's on-screen geometry (`viewwidth`/…/`viewwindowy`), `Lighting` its light selection (`fixedcolormap`/`extralight`, the `scalelight`/`zlight` tables) and `GraphicsData` its loaded WAD graphics (`textures`/`colormaps`/`sprites`/…, the R_InitData tables) and `RenderScratch` its per-frame BSP scratch (`rw_*`/`sscount`/`floorplane`/`ceilingplane`) — the renderer's own state, now fully off the loose globals; the game state has followed — **all three headers (`doomstat.h`, `r_state.h`, `p_local.h`) are now nearly empty of loose globals**, and the same pattern has reached the other headers too — ~22 cohesive clusters migrated: from `doomstat.h`, `LevelStats`/`LaunchOptions`/`GameVersion`/`GameSession`/`StartupDefaults`/`PlayerState`/`GameFlow`/`DemoState`/`RefreshFlags`/`OverlayState`/`NetState`/`MapSpawns`/`GameClock`/`AmmoLimits`/`IntermissionInfo`/`SkyState`/`CorpseQueue`; from `p_local.h`, `ItemRespawnQueue` and `clipammo` (folded into `AmmoLimits`); from `d_event`/`d_main`, `EventQueue`/`AttractMode` and `gameaction` (folded into `GameFlow`); from `p_spec`, `ActiveSpecials`/`EndLevelTimer` — `r_state.h` was already done (its externs are geometry views onto `Level`). Three vestigial globals were *deleted* rather than migrated (`viewangleoffset`, `linecount`, `loopcount` — all always-zero or dead). What is left loose is the config-backed set (`snd_*Volume`/`mouseSensitivity`, blocked by `Config.cpp`'s static address capture until the config rework — proven, a naive migration segfaulted every test), the deferred `thinkercap` (moves with the `Thinker` rewrite), and a short tail of single scalars (`save_p`, `basedefault`) that move with their subsystems/the config rework. The lone engine-global scalar `validcount` — owned by no subsystem — has moved in as a `Doom::ValidCount` `Engine` member, and DoomMain's boot-path string globals are off the cloud (`wadfiles[]`/`title[]` made file-local `static`, the dead `wadfile[]`/`mapdir[]` deleted). The **file-scope-statics sweep — the last Step-5 phase — is well advanced** — g_game's per-tic input (`TiccmdInput`), its demo buffer (folded into `DemoState`), its deferred new-game params (`DeferredNewGame`), its `consistancy` array (folded into `NetState`), its par-time tables (`ParTimes`), its movement-speed tables (`MovementSpeeds`), the `-timedemo` benchmark (`TimeDemo`) and the pending-command flags (`PendingCommands`) are the first Game-local internal state into the `Engine`, and the sweep has since reached the UI's *file-local* `static`s, **emptying `UI/Hud` and `UI/StatusBar` of data statics**: the HUD (`HudMessage`/`HudChat`/`HudState`) and the status bar (`StatusBarFace`/`StatusBarWidgets`/`StatusBarGraphics`/`StatusBarState`), then the automap's own view state (`AutomapView`), the intermission's residual state (`IntermissionState`), the finale's runtime state (`FinaleState`), the melt's scratch framebuffers (`WipeState`) and the menu's transient interaction state (`MenuState`), and then the renderer's file-local scratch (all of Render: `CompositeCache`/`WallScratch`/`SpriteScratch`/`DrawTables`/`SolidSegs`/`PlaneScratch`/`RenderMainState`) and most of the playsim's (`ActionScratch`/`WeaponScratch`/`SightScratch`/`EnemyAI`/`SwitchList`/`PlayerScratch`/`AnimatedSurfaces`/`LevelPool`), plus doomstat's internal-parameter scalars (`EngineParams`), with a `StateClusterTests` net pinning the golden-neutral tables. **The config-backed category is now complete** — the `doom_config`→`Host` rework's config half landed as a runtime bind: `Config.cpp`'s `defaults[]` no longer captures `&member` at static-init (the race that segfaulted every test) but binds each config-backed entry to its `Engine` member at runtime (`bindEngineDefaults`, called before `mLoadDefaults`/`mSaveDefaults` touch a `location`), and the app reaches them the same way, through `defaults[].location`. On that, the config-backed globals migrated in: `SoundSettings` (`snd_SfxVolume`/`snd_MusicVolume`/`numChannels`), `MenuSettings` (`mouseSensitivity`/`showMessages`/`detailLevel`/`screenblocks`/`usegamma`), `ConfigPaths` (`basedefault`/`defaultfile` — never actually captured), and `InputConfig` (all 22 key/mouse/joy bindings + the device enables + the crosshair/always-run toggles). **And the renderer's cross-read view-globals are now swept** — the state the flat `r_*.cpp` shims still owned and exported through `r_bsp.h`/`r_draw.h`/`r_segs.h`/`r_things.h`/`r_plane.h`/`r_sky.h`: `SkyState` gained `skytexture`/`skytexturemid`, and new clusters landed for `BSPScratch` (the BSP-walk pointers + drawseg pool), `SegState` (the wall-segment texture/mark/light state), `SpriteState` (the vissprite pool + sprite clip window), `DrawState` (the dc_*/ds_* drawer inputs), `VideoState` (`dirtybox`), plus `PlaneScratch` extended with the clip/projection arrays and the dead `floorfunc`/`ceilingfunc_t` deleted; `HudFlags` took the cross-read `chat_on`/`message_dontfuckwithme`. The load-bearing trap throughout: a bare `extern int X;` (not via the header) that *writes* `X` clobbers the low half of the reference's pointer — every cross-file extern must move to `extern T&` in lockstep (caught once as the `0x100640000` `skytexturemid` fault) |
| 6 | The playsim | **done** (modulo the deferred `Thinker` virtualisation) — **every** `p_*.cpp` is now a shim over a `namespace Doom` `Sim/` unit: the actor core (`MapUtil`/`Movement`/`MapAction`/`Sight`/`Interaction`/`Player`/`Mobj`/`Weapon`/`Enemy`), the specials (`Lights`/`Plats`/`Ceilings`/`Floors`/`Doors`/`Switches`/`Teleport`/`Specials`), `Tick`, `Setup` and `SaveGame`. The `thinker_t` function-pointer union is kept — the `T_*`/`P_MobjThinker` addresses stay global shims so p_saveg's pointer-identity serialisation is untouched — and virtualising it into a real `Thinker` with a virtual `tick()` is deferred to Step 8 |
| 7 | The renderer | **done** — all 8: `r_sky`→`Sky`, `r_data`→`Data`, `r_main`→`Main`, `r_plane`→`Planes`, `r_bsp`→`BSP`, `r_segs`→`Segs`, `r_things`→`Things`, `r_draw`→`Draw`, all holding the frame goldens byte-identical and the app linking |
| 8 | UI, game loop, host boundary; `thinker_t`→`Thinker`; delete the zone | **in progress** — UI (menu included), game loop and utils done: `f_wipe`→`UI/Wipe`, `hu_lib`→`UI/HudWidgets`, `st_lib`→`UI/StatusWidgets`, `hu_stuff`→`UI/Hud`, `st_stuff`→`UI/StatusBar`, `f_finale`→`UI/Finale`, `am_map`→`UI/Automap`, `wi_stuff`→`UI/Intermission`, `m_cheat`→`UI/Cheat`, `m_menu`→`UI/Menu` (behind a new frame golden built for it first); `g_game`→`Game/Game`, `d_main`→`Game/DoomMain`, `d_net`→`Game/Net`, `m_argv`→`Game/Args`, `m_misc`→`Game/Config`, `s_sound`→`Game/Sound`; `v_video`→`Render/Video`. **The host boundary is now complete**: `i_video`→`Host/Video`, `i_system`→`Host/System`, `i_sound`→`Host/Sound`, `i_net`→`Host/Net`, and `DOOM.cpp`→`Host/Api` (the public `doom_*` C API — no shim, its `extern "C"` symbols stay global). The small remainders are done (`m_swap`→`Math/Swap.h`, `doomstat`→`Game/State`, `dstrings`' `endmsg` folded into `UI/Menu`, empty `doomdef.cpp` deleted), as are the two ready data tables (`d_items`→`Sim/Items`, `sounds`→`Game/SoundData`). **The p_saveg save/load net is built** (`Tests/Sim/SaveGameTests.cpp` + `doomSimSaveLoadPreservesWorld`), and on it **the zone was deleted** (Step 4 above): mobjs/specials to a level pool, renderer `PU_STATIC`/scratch to `doom_malloc`, `z_zone` gone. The flat vanilla list is down to the shims plus `info.cpp` alone. Left: `thinker_t`→`Thinker` and `info.cpp` (deferred *together* with the action-model rewrite, since `states[]` is the action table that step replaces), plus the `doom_config`→`Host` interface redesign + audio and the ongoing globals-into-`Engine` work |

## Where this is — session handoff

Everything below is committed on branch **`C++Refactor`**; the working tree is
clean and the suite is green (**80 tests**, ~6s: `ctest --test-dir build`). Steps
0–4 are complete; 6 and 7 are done; 8 is nearly done — **the whole UI, game loop,
netcode, utility layer and host boundary are migrated, and the zone allocator is
deleted**; and **Step 5 has had two whole categories finished this session** on top
of everything the earlier sweep did (**~90 `Engine` members now**).

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

The UI, the whole renderer and the config-backed set are done. The flat vanilla list is still the
shims plus **one file**, `info.cpp` (the generated actor/state LUT). What is left of Step 5, and why,
is spelled out in the Step-8 tail — it is now a genuine tail (a handful of scattered single flags, the
inert netcode bookkeeping, the function-local `static`s, and the save/thinker-coupled state), plus the
two deep items below.

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
- **`thinker_t`→`Thinker`** (a real base with a virtual `tick()`) is the last and
  deepest step — it makes `mobj_t`/the specials polymorphic, which breaks the
  memcpy serialisation and the `memset`-over-raw-alloc init (now `doom_memset(0)` in
  `levelAlloc` and `P_SpawnMobj`), so it needs placement-new at every spawner and a
  field-wise p_saveg, all held by the net. It is **not** required for "no zone"
  (done) or for the flat list to empty; the union dispatch is already a clean,
  golden-pinned choke point, so the payoff is modest and the risk real — deferred on
  purpose. `info.cpp` migrates with this step (its
  `states[].action` union is exactly what the virtualisation replaces — converting
  it before means converting it twice).

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
the logic moved to a `Sim/` unit; `p_tick`'s run loop still dispatches through the
`thinker_t` union. The `thinker_t`→`Thinker` virtualisation (a real base class with a
virtual `tick()`) is therefore **deferred**, not done: it is a deeper change (mobj_t
and the special structs become polymorphic, touching spawning, `Z_Malloc`/`memset`
init, and p_saveg's byte-serialisation, all under the demo goldens), and the union
being kept means the append-only probe hash — which finds mobjs by
`thinker->function.acp1 == P_MobjThinker` — is untouched.

**What remains overall:** the deep tail of Step 8 (the `thinker_t`→`Thinker`
virtualisation + `info.cpp`, and the `doom_config`→`Host` redesign + audio), plus the
*finish* of Step 5. Steps 6 and 7 are complete — every `p_*` and `r_*` file is now a
shim over a `namespace Doom` unit — and Step 5 has migrated ~25 cohesive clusters, so
`doomstat.h`/`r_state.h`/`p_local.h` are nearly empty of loose globals (and the `d_event`/
`d_main`/`p_spec` clusters have followed). The clean export-header tail is now cleared
(`validcount` moved in, DoomMain's boot strings went file-local, dead `drone` removed), and the
**file-scope-statics sweep — the last Step-5 phase — has begun**: g_game's own internal state is
moving into the `Engine` cluster by cluster (`TiccmdInput`, the demo buffer, `DeferredNewGame`,
`consistancy`). What is still left is the config-backed globals (waiting on the config rework —
proven necessary, a naive migration segfaulted every test), `thinkercap` (waiting on the
`Thinker` rewrite), and the remaining file-scope statics across the UI/renderer/specials files.
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
head, which moves with the deferred `thinker_t`→`Thinker` rewrite.

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

What is left of the export-header tail: `save_p` (p_saveg's serialization cursor, which moves
with the `Thinker`/p_saveg rewrite) and `basedefault[]` (config-blocked, above). The `am_map`
automap state stays in its shim deliberately, the GPU automap in the app reading it. Beyond the
tail is the config rework (which unblocks the config-backed set), the `Thinker` rewrite (which
brings `thinkercap`), and the long file-scope-statics phase — which has now begun.

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
  keeps the 1993 rows verbatim under the strict flags). `info.cpp` stays deferred —
  its `states[]` holds the action pointers the `Thinker` step rewrites.
- **The p_saveg save/load net** (`Tests/Sim/SaveGameTests.cpp` +
  `doomSimSaveLoadPreservesWorld`) — the enabler for everything below. p_saveg's
  archive/unarchive is the one simulation path no demo covers, and it is exactly
  the mobj/special byte layout the `Thinker` step and the mobj/special
  zone-ownership change rewrite. The net archives the live world, reloads a fresh
  base level and unarchives over it (the `gDoLoadGame` sequence), then requires a
  comprehensive world hash unchanged. Shown to bite.

**Steps 6 and 7 are complete**, Step 8's mechanical migrations are done, and
**the zone is deleted** — the flat vanilla list is down to the shims plus
**`info.cpp`** alone. What remains is the deep, interlocking tail:

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
- **`thinker_t`→`Thinker`** virtualisation and **`info.cpp`** last, together: a
  real base with a virtual `tick()` makes `mobj_t`/the specials polymorphic, which
  breaks the memcpy serialisation and the `memset`-over-raw-alloc init (now
  `doom_memset(0)` in `levelAlloc`/`P_SpawnMobj`), so it needs placement-new at
  every spawner and a field-wise p_saveg, all held by the net. `info.cpp`'s
  `states[]` action union is exactly what it replaces. Neither is required for "no
  zone" (done) or for the flat list to empty; the union dispatch is already a clean
  golden-pinned choke point, so the payoff is modest and the risk real — deferred.

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
  and against the refactor's clean-C++ ethos. It is a focused, review-warranting effort,
  not a mechanical sweep — which is why it stays deferred even though everything it rests
  on (the p_saveg net, the level pool, the golden-pinned dispatch) is now in place.
- **The `doom_config`→`Host` redesign + audio.** The **config half is done** — the thing
  it was really needed for. `Config.cpp` no longer captures option-global addresses at
  static-init; `bindEngineDefaults()` points each config-backed `defaults[]` entry at its
  `Engine` member at runtime (top of `mLoadDefaults`/`mSaveDefaults`, before any `location`
  is read), and the app reaches them the same way, through `defaults[].location`. That is
  what unblocked the config-backed category (now migrated — see below). What is *left* of
  this bullet is two things the config unblock did not need: (1) folding the 13 host function
  pointers (`doom_print`/`doom_malloc`/… in `doom_config.h`) into a `Host` struct — a
  loose-global cleanup whose value is modest (the callbacks are host/platform state a
  *fresh Engine* arguably should not reset — they are set once by the embedder before
  `doom_init`), and which churns the public `doom_set_*` C API the app and `SimProbe`
  depend on; and (2) **wiring audio (gap-log item 1), which is externally blocked** — the
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
  - **`thinkercap`** — moves with the `thinker_t`→`Thinker` rewrite (it is the head of
    the intrusive thinker list that step reworks); `save_p` and the savegame orchestration state
    (`savebuffer`/`savename`/…) move with the p_saveg part of that rewrite.
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
    golden-neutral tables. Since then `soundtarget` (`Sim/SoundTarget`) and the whole of `Game/Net`'s
    private bookkeeping (folded into `NetState`) have moved in too, so **every cohesive cluster is now
    migrated**. **What genuinely remains of the sweep** is a tail: a few scattered single flags with no
    cohesive cluster home (`st_statusbaron`, `is_wiping_screen`, `inhelpscreens`) — each belongs to a
    different subsystem and does not group without a grab-bag, so they wait for their subsystem to want
    them; the loaded-once **asset pointers** (`hu_font`, `sttminus`); and the **function-local
    `static`s** (the "later function-local pass" — `HU_Responder`'s send state, the status-bar
    face-drawer's counters, `D_Display`'s frame-diff state, `A_BrainSpit`'s alternating toggle, …),
    which are a different shape from the file-scope sweep. Beyond the tail, the last thing
    that *finally* lets the engine be **constructed** rather than booted is flipping `engine()` from a
    function-local-static singleton to an instance threaded through the `doom_*` entry points — a large
    API change, not a mechanical migration, and the real end of Step 5.

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
