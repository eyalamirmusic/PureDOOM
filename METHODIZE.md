# METHODIZE.md — free functions → methods

Progress log for the effort to find the "free function that's really a method on a
single owned type" pattern in the engine and rewrite it as classic OOP. Continues in
spirit from the thinker-OOP work (`git log`, "Moved thinkers to look more like proper
OOP classes") and sits alongside `REFACTOR.md`.

**Status: done. The whole playsim surface (Player + Mobj + the state-action table)
and the Specials line/sector subsystem are converted and verified. Nothing that fits
the rule cleanly remains a free function.**

## The rule applied

A free function `RET f(T& obj, args...)` whose primary argument is a single
engine-owned type is a hidden method. It becomes `RET T::f(args...)` and every call
site `f(x, args)` becomes `x.f(args)`. Applied to `Mobj`, `Player`, `Sector` and
`Line`.

`Mobj` now declares **79** methods, `Player` **42**, `Sector` **14** and `Line` **21**.

## What was converted

| Cluster | File(s) | Methods |
|---|---|---|
| **State-action table** | `Sim/ActionFunc.h`, `Sim/Info.cpp` | slot is `void (Mobj::*)()` / `void (Player::*)(PspDef&)`; all 448 table entries are `&Mobj::name` / `&Player::name`; call sites `(mobj.*action.mobj)()` |
| **Enemy actions** (52 Mobj) | `Sim/Enemy.cpp` | `look`, `chase`, `faceTarget`, the attacks, the deaths, the brain/boss set, `bfgSpray` (in Weapon.cpp) |
| **Weapon actions** (22 Player) | `Sim/Weapon.cpp`, `Sim/Enemy.cpp` | `weaponReady`, the fire routines, `light0/1/2`, `bfgSound`, the super-shotgun trio |
| **Player think** | `Sim/Player.cpp` | `think` (was `playerThink`), `thrust`, `calcHeight`, `movePlayer`, `deathThink` |
| **Enemy AI helpers** | `Sim/Enemy.cpp` | `move`, `tryWalk`, `newChaseDir`, `lookForPlayers`, `checkMeleeRange`, `checkMissileRange`, `painShootSkull` |
| **Weapon helpers** | `Sim/Weapon.cpp` | Player: `setupPsprites`, `movePsprites`, `bringUpWeapon`, `checkAmmo`, `fireWeapon`, `dropWeapon`, `setPsprite`; Mobj: `computeBulletSlope`, `gunShot` |
| **Mobj core** | `Sim/Mobj.cpp` | `setState` (was `setMobjState`), `xyMovement`, `zMovement`, `nightmareRespawn`, `remove` (was `removeMobj`), `spawnMissile`, `spawnPlayerMissile`, `explodeMissile`, `checkMissileSpawn` |
| **Movement clipping** | `Sim/Movement.cpp` | `checkPosition`, `tryMove`, `teleportMove`, `thingHeightClip` |
| **MapAction** | `Sim/MapAction.cpp` | Mobj: `slideMove`, `lineAttack`, `radiusAttack`; Player: `useLines`; **Sector: `changeSector`** |
| **Interaction** | `Sim/Interaction.cpp` | Mobj: `damage` (was `damageMobj`); Player: `giveAmmo`, `giveWeapon`, `giveBody`, `giveArmor`, `giveCard`, `givePower` |
| **Specials (player)** | `Sim/Specials.cpp` | Player: `inSpecialSector` (was `playerInSpecialSector`) |
| **Specials — Sector queries** | `Sim/Specials.cpp` | Sector: `findLowestFloorSurrounding`, `findHighestFloorSurrounding`, `findNextHighestFloor`, `findLowestCeilingSurrounding`, `findHighestCeilingSurrounding`, `findMinSurroundingLight` |
| **Specials — Line dispatch** | `Sim/Specials.cpp` | Line: `findSectorFromLineTag`, `crossSpecialLine`, `shootSpecialLine`, `doDonut` |
| **Floors** | `Sim/Floors.cpp` | Sector: `movePlane` (the shared height-mover); Line: `doFloor`, `buildStairs` |
| **Doors** | `Sim/Doors.cpp` | Line: `doDoor`, `doLockedDoor`, `verticalDoor`; Sector: `spawnDoorCloseIn30`, `spawnDoorRaiseIn5Mins` |
| **Ceilings** | `Sim/Ceilings.cpp` | Line: `doCeiling`, `activateInStasisCeiling`, `ceilingCrushStop` |
| **Plats** | `Sim/Plats.cpp` | Line: `doPlat`, `stopPlat` |
| **Lights** | `Sim/Lights.cpp` | Sector: `spawnFireFlicker`, `spawnLightFlash`, `spawnStrobeFlash`, `spawnGlowingLight`; Line: `startLightStrobing`, `turnTagLightsOff`, `lightTurnOn` |
| **Teleport** | `Sim/Teleport.cpp` | Line: `teleport` |
| **Switches** | `Sim/Switches.cpp` | Line: `startButton`, `changeSwitchTexture`, `useSpecialLine` |

### Renames worth knowing

`setMobjState`→`setState`, `removeMobj`→`remove`, `damageMobj`→`damage`,
`playerThink`→`think`, `playerInSpecialSector`→`inSpecialSector`. And one *forced*
rename: vanilla `A_Tracer` → `Mobj::traceTarget()`, because `Mobj` already has a
`tracer` data **member**.

## What deliberately stayed a free function

Documented at each site (and in the relevant `.h`):

- **Blockmap / traversal callbacks** — taken by address, so they must stay free:
  `vileCheck`, `recursiveSound`, `stompThing`, `checkLine`, `checkThing`,
  `hitSlideLine`, `slideTraverse`, `aimTraverse`, `shootTraverse`.
- **Genuinely two-object operations, no single owner**: `noiseAlert` (source +
  emitter), `touchSpecialThing` (special + toucher), `killMobj` (source + target).
- **Nullable primary**: `aimLineAttack` — its `t1` may be null (`fire()`/`bfgSpray`
  pass a mobj's `.target` unguarded), and `this` is never null.
- **Factories** (no instance to be a method of): `spawnMobj`, `spawnPuff`,
  `spawnBlood`, `spawnPlayer`, `spawnMapThing`, `respawnSpecials`.
- **The `::`-scope p_spec utilities** — `getSide`, `getSector`, `twoSided`,
  `getNextSector`. They sit at global `::` scope, take sector/line *indices* or two
  objects, and the port includes them; left as they were.
- **Active-special registry inserts** — `addActiveCeiling`, `removeActiveCeiling`,
  `addActivePlat`, `removeActivePlat`. Their primary argument is a `Ceiling&`/`Plat&`,
  so they *look* like methods, but they insert/remove that thinker in the level's
  slot-indexed `activeceilings`/`activeplats` table — exactly the registry role
  `addThinker`/`removeThinker` keep as free functions, so these do too. Their sibling
  `activateInStasis(int tag)` keys off a bare tag and could not be a method at all.
- **Level-wide coordinators** (no single owner): `initPicAnims`, `updateSpecials`,
  `spawnSpecials`, `initSwitchList`.

## Style (per the code owner's direction during the work)

- **Bare implicit `this`.** Member access is `x`, not `this->x` and not a
  `T& obj = *this;` alias. Both of those were tried and rejected.
- **Colliding locals/params renamed with the repo's `…ToUse` suffix** (e.g.
  `angleToUse`, `xToUse`, `cmdToUse`, `stateToUse`, `armortypeToUse`), or a
  descriptive name where clearly better (`ammoType`, `spawnType`, `corpseInfo`,
  `playerToUse`, block indices `bx`/`by`).

## How it was done (tooling, in the scratchpad)

Three throwaway scripts drove the mechanical bulk; the compiler + goldens verified.

- **`deinline.py`** — per function (brace-matched), strips the `T& p = *this;` alias
  and rewrites the body: `p.x`→`x`, `&p`→`this`, bare `p`→`*this`, `p.*pmf`→`this->*pmf`,
  and `method(*this)`→`method()`. Handles `Mobj`/`Player`/`Sector`, any return type.
- **`methodize.py`** — rewrites call sites `f(obj, rest)`→`obj.f(rest)` with balanced-
  paren arg parsing (nested calls/templates safe). Handles `*p`→`p->f()`, `*this`→
  `f()` (self), value→`.`. **Comment/string-aware** (skips matches inside `//`, `/* */`,
  `"…"`) and won't match `std::move(`, `->m(`, or `::m(`.
- **`movement_convert.py`** — special case for the `Fixed x, Fixed y` coordinate
  params that shadow the `x`/`y` members: renames the params to `xToUse`/`yToUse`
  *before* de-inline (while `thing.x` is still distinguishable from the param).

The Specials stage was smaller and did without the scripts: line-range-scoped
`perl -pe` for the repetitive body rewrites (`line.` → member, `(line, ` → `(`,
`find…(*sec)` → `sec->find…()`), hand edits for the signatures and lambda captures,
then the compiler + goldens + `-Wshadow-all`.

`-Wshadow-all` (via `cmake -B build -DCMAKE_CXX_FLAGS=-Wshadow-all`) was the
**authoritative** shadow detector — grepping for shadows by hand under- and
over-reported (brace-init, pointer-attached decls, `other->member` false hits).

## Verification

Re-run after each stage; the last run (the Specials subsystem) held all of it:

- **Debug**: 103/103, **0 warnings**. **Release**: 103/103, **0 warnings**.
- **eacp app** target (`build-app`, example ON) compiles clean.
- **`-Wshadow-all`** (the authoritative shadow detector, hazard 7): **0** across the
  engine. The method bodies fold `sec.`/`line.` member access down to bare names, so a
  local or param equal to a member would have been a silent shadow — none were.
- **No golden file touched** — every `*.hashes` and `*.frames` is byte-identical.
  The world and every frame are exactly as before; this was a pure structural
  rewrite. `record-goldens` was never run.
- clang-format clean. Specials stage: ~28 files, roughly net-neutral line count.

## Hazards this work turned up (read before continuing)

1. **The alias vs. implicit-this vs. rename question is settled**: bare `this`,
   `…ToUse` for collisions, never `this->`.
2. **A param whose name equals a member name breaks naive de-inline.** `damageMobj`'s
   `Mobj& target` collided with the `target` member; `target.target = source` came out
   as `*this = source`. Hand-fix these (only `setMobjState`'s `state` and
   `damageMobj`'s `target` had it).
3. **Coordinate params (`x`,`y`) shadow members and `tryMove` *writes* `thing.x = x`** —
   a blind rewrite yields `x = x` (member never set). Rename params first.
4. **Scoped `perl -pe` line-range renames can re-hit a line you already hand-fixed.**
   That produced a `Player* playerToUse = playerToUse;` self-init that **segfaulted the
   demo combat path** — caught by the goldens, not shipped. Exclude already-fixed lines.
5. **De-inline and the scoped perls run over comment text too.** They left `*this`
   and renamed tokens (`ammoType`, `playerToUse`) inside comments; all were swept, but
   check comments after any such pass.
6. **Tests call engine functions too.** `Tests/SimProbe.cpp` used
   `Doom::checkPosition(*mobj, …)`; `methodize.py` skips `Doom::`-qualified calls, so
   fix `Tests/*.cpp` by hand and rebuild — a stale test binary will pass on the *old*
   build and hide a break.
7. **`-Wshadow` (plain) does not flag data-member shadows on Clang — `-Wshadow-all`
   does.** The default build (`-Wall -Wextra`) has neither, so a member/local shadow
   is silent unless you ask for it.
8. **The declaration site inverts the dependency, and the enums are the snag.** A
   method must be *declared inside* its class, so the `Sector`/`Line` definitions in
   the low-level `Sim/MapTypes.h` now have to name `FloorType`, `DoorType`, …,
   `MoveResult`, `ButtonWhere` — types that live in the higher-level `Thinkers/*.h`
   and `Sim/SpecialTypes.h`, which themselves forward-declare `Sector`. Including
   them back would be a cycle. The fix: **opaque-declare the scoped enums**
   (`enum class FloorType;`) at the top of `MapTypes.h`. A scoped enum has a fixed
   underlying type (int), so the opaque declaration is a *complete* type and is legal
   as a by-value parameter/return in the method declarations; the real definitions,
   pulled in by each `.cpp`, must stay plain `enum class X { … }` (default int) to
   match. This keeps `MapTypes.h` free of the special family while the bodies in the
   `.cpp`s see the full enums.
9. **A local named like the member it was initialised from becomes a self-init when
   the member arrives.** `teleport` opened with `int tag = line.tag;`; as a `Line`
   method `line.tag` is the member `tag`, and the naive rewrite is `int tag = tag;` —
   a shadowing self-init. Here the local was redundant (it only cached the member),
   so it was deleted and the member used directly; where such a local is genuinely
   needed, rename it (`…ToUse`). This is hazard 2's collision in its subtlest form —
   `-Wshadow-all` catches it, a plain build does not.
10. **`crossSpecialLine` shed its `linenum` indirection.** Vanilla passed a line
    *index* and re-looked-up `level().lines[linenum]`; the sole caller (`xyMovement`)
    already held the `Line*`, so the method takes no index and the caller is
    `ld->crossSpecialLine(oldside, *this)` — the lookup disappeared.

## The Specials line/sector handlers — done

The last coherent subsystem, now converted. It keyed off `Line&`/`Sector&` rather
than the Player/Mobj surface, and `Sector::changeSector` had already shown the shape.

- **`Sector` gained the surrounding-sector queries** (`findLowest/HighestFloor…`,
  `findNextHighestFloor`, `findLowest/HighestCeiling…`, `findMinSurroundingLight`),
  the shared height-mover `movePlane`, and the per-sector light/door spawners.
- **`Line` gained the whole line-special surface** — the cross/shoot/use dispatchers,
  `doDonut`, `findSectorFromLineTag`, and every `EV_*` door/floor/ceiling/plat/light/
  teleport/switch handler.
- The method **declarations** all live on `Sector`/`Line` in `Sim/MapTypes.h` (see
  hazard 8 for the enum snag that forced); the **bodies** stayed in their subsystem
  `.cpp`s (`Sim/{Specials,Floors,Doors,Ceilings,Plats,Lights,Teleport,Switches}.cpp`),
  whose now-empty `.h` declaration blocks became short "these are methods now"
  comments (the headers stay as include points — `Floors.h` still carries `MoveResult`
  to the thinkers).
- The collisions hazard 2 warned of were live throughout (`special`, `tag`,
  `floorheight`, `sidenum`, `frontsector` are all `Sector`/`Line` members). The one
  that bit was `teleport`'s `int tag = line.tag` (hazard 9).

Nothing that fits the rule cleanly is left. The registry inserts and level-wide
coordinators that stayed free are listed under **What deliberately stayed a free
function** above, each for a stated reason.
