# eacp Rendering Plan

## Where this work happens

Read this before touching anything, because almost every item below changes eacp
rather than this repository.

**eacp lives at `~/Code/eacp-puredoom`, on a branch, always.** Clone it there if it
is not there yet; check out the branch this work is on, and create one if it does
not exist. That path is deliberately *not* `~/Code/eacp` — the plain checkout is
whatever the developer happens to be doing in eacp that day, and this port needs a
tree it can hold at a known commit for as long as it takes to get a feature in.
Two working copies, two purposes, neither disturbing the other.

**This repository is developed on a branch too**, and the two branches move
together: a change here that depends on an unmerged eacp feature is meaningless on
`master`, and a green build proves nothing unless it says which eacp it was green
against.

The two are linked with CPM, which is the whole reason the split is cheap:

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug \
      -DCPM_eacp_SOURCE=$HOME/Code/eacp-puredoom
```

`$HOME`, never `~` — CMake does not expand tildes, and a quoted `~/...` silently
configures against a directory that does not exist. That trap is already recorded
under **Build** in `CLAUDE.md`; it costs an afternoon every time.

None of this weakens the standing rule, it is what makes the rule workable: **eacp
is never modified from this repository.** Part 2 and Part 3 are commits in
`~/Code/eacp-puredoom`, on its branch, reaching this port through the flag above —
and only through it.

---

This port exists for two reasons, and only one of them is DOOM. The other is goal
2 in `CLAUDE.md`: **exercise eacp as a game platform layer and surface what it is
missing.** A large batch of GPU work has landed in eacp since the gap log was last
read against it, and this document is what that batch is worth here — plus what
still has to be built there, and where the API made this port write something it
should not have had to.

Both halves matter. A feature that exists but can only be reached by taking
`RenderPass::draw` apart and reassembling it by hand is not finished, and the place
that discovers it is a real consumer with real geometry. That is what this port is
for.

Measured against eacp `main` at `a114455` (2026-07-27), which is where this batch
of work started. `main` has moved well past it since — it now carries everything
below marked **Closed**, plus GPU pass timing, `StreamingBuffers` and packed vertex
formats. Every claim below was checked in the checkout rather than inferred from a
commit message.

**What this repository's branch is green against**, which the rule above says has
to be stated: **eacp `main`**, and nothing else is needed any more. E1, E5, E2, E3
and E4 all lived on the `puredoom` branch for a while — that is what the paragraph
here used to say, and why `-DCPM_eacp_SOURCE` was mandatory — and all five have
since merged upstream, along with the Windows fixes that followed them.

Measured rather than assumed, because this is exactly the claim that rots: a
scratch tree configured with **no** `CPM_eacp_SOURCE` builds every target including
the app, and all 121 tests pass in `Release`. The flag remains the right tool for
co-developing against a local eacp; it is no longer a prerequisite.

## What is left

Everything that was *blocking* is done: the renderer's feature list is finished and
so is the eacp work it waited on, and **nothing in the renderer still runs at
320x200 that does not have to**. **The measurement that was to decide the next
three items has been taken** (see **The measurement** below), and its answer is that
none of the three is worth building for its cost.

| | Item | State |
|---|---|---|
| ~~**next**~~ | ~~Measure `buildGeometry` and the per-frame upload~~ | **Done.** `Tests/Bench/GeometryBench.cpp`, target `port-bench` |
| | **P3** instancing, **P4** per-sector heights, **E6** texture arrays | **Not worth building for the cost they attack** — the whole of it is 4% of a refresh. See below; E6 keeps a case that is not about speed |
| ~~**next**~~ | ~~**P5** full-resolution melt~~ | **Done.** Not the way this plan expected — see below |
| **next** | **E7** `R8Unorm` as a `PixelFormat` | not blocking; carries a latent `pixelFormatFor` mismatch worth fixing regardless |
| | **I1** app-owned geometry falls off `draw(program)` | open, and the clearest interface finding of the port |
| | **I2** `bindTextures` public only because of I1 | closes when I1 does |

Withdrawn rather than done: **I3** and **I4**, both of which asked eacp to document
something it had already documented (`SAMPLERS.md`, `GPUView.h`). **I5** is
half-answered — `prepare` takes a descriptor now, but still not a target.

---

## The measurement

`Tests/Bench/GeometryBench.cpp` (target `port-bench`) builds a frame of world
geometry three times per tic through all three attract demos and reports what it
cost. It is a benchmark, not a test: ctest never runs it, and the output is a
number to read. It needs no GPU for the same reason `Tests/Port` does not, so the
only part of the real frame it cannot see is the upload — which the app was
instrumented for separately.

It carries **its own gate**: a hash of every vertex and every draw the run
emitted. A benchmark that reports only a time cannot tell a change that made the
builder faster from one that made it emit something else, and no golden here can
help — the frame goldens run the software renderer, which never executes a line of
this. So an optimisation is honest exactly when the hash is unchanged.

**Release, Apple silicon, 34,230 frames.** Before any of the work below:

| demo | level | geometry | `buildGeometry` mean | p95 | per frame |
|---|---|---|---|---|---|
| demo1 | E1M5 | 825 lines, 384 subsectors, 216 things | 80.9µs | 85.3µs | 10,978 vertices, 300 KB, 119 draws |
| demo2 | E1M3 | 1,026 / 461 / 280 | 99.1µs | 102.2µs | 13,805 vertices, 378 KB, 131 draws |
| demo3 | E1M7 | 958 / 467 / 254 | 96.2µs | 101.9µs | 13,044 vertices, 357 KB, 125 draws |

**In the app, the renderer is nowhere near its budget.** `render()` spends a few
hundred microseconds of CPU against an 8.33ms refresh, and the display holds
**120.5Hz without dropping a frame**, before and after. The proportions are the
reliable part of that reading and the absolute numbers are not: everything in the
frame scales together by up to 3x depending which core the display-link thread
lands on, which is a fact about Apple silicon's scheduler rather than about this
renderer. Of `render()`'s CPU, `buildGeometry` is ~45-55%, submitting the
per-texture draws ~26-31%, and **the upload is 2.2-2.5% — 4 to 13µs**. The
per-frame upload, in other words, was never a cost at all.

### What it decided

- **P3, instanced billboards — do not build.** Sprites are 12% of the vertices and
  `emitSprite` does not appear in the profile at all (under 0.5%). Every thing in
  the level is already cheaper than the walls around it, so moving the quad
  expansion to the GPU wins nothing measurable. The *invariant* argument in P3's
  entry still stands on its own merits; the performance argument is gone.
- **P4, per-sector heights — do not build.** It attacks the largest share (walls
  and flats are ~74% of the builder's time and 88% of its bytes), and that share is
  still under 1% of a refresh. What it costs is a rewrite of the emitter with the
  pegging rules moved into the vertex shader, plus an answer to animated textures
  changing which draw a wall belongs to. That is the largest structural change on
  the list, bought with the smallest measured saving on it.
- **E6, texture arrays — keep, but not for speed.** ~125 draws a frame is the
  biggest single item left, and it is still only the submission cost above. The
  reason to want it is what its own entry says — the group-by-texture bookkeeping
  in `buildGeometry` disappears, and one draw over app-owned geometry is a far
  smaller ask of **I1** than 125 are. It is an eacp feature worth having; it is not
  a frame-time fix.

### What it found that was not on the list

**Just over half of `buildGeometry` was the engine's state accessors, not
geometry.** `Doom::level()`, `Doom::graphicsData()`, `Doom::skyState()`,
`Doom::playerState()` and `Doom::lighting()` are out-of-line calls over a
function-local static — each one reaching `Doom::engine()`, which loads a guard —
and nothing in `EngineAccess.cpp` can inline any of them. The emitter asked per
line, per side, per texture band, and then walked the whole world a second time to
write what the first pass had counted: tens of thousands of calls a frame. Their
disjoint self time came to **51.7%** of the builder, against a fifth for the
arithmetic they were fetching operands for.

`CLAUDE.md` already states the rule this breaks — hoist a cluster's reference once
per function rather than calling the accessor per access — for the engine's own
per-pixel drawers. The port had never been held to it. Hoisting it through the
emitters costs about forty lines and **cut `buildGeometry` by 43%**: 80.9 → 45.7µs,
99.1 → 57.5µs, 96.2 → 53.2µs, with all three geometry hashes **bit-identical** and
all 120 tests green.

That is larger than anything P3, P4 or E6 would have returned, at a fraction of the
risk, and it was invisible to every gate in the repository. Two things generalise:

- **The item that pays is rarely the item on the list.** Three plan entries had been
  written against a cost none of their authors had measured, and the real answer was
  in none of them. Measuring first was worth more than the measurement's stated
  purpose.
- **A benchmark wants a correctness gate as much as a test does.** The hash is what
  made a 43% speedup safe to believe — without it, "faster" and "emitting less" read
  the same on the way out.

Gap-log entries with no plan item behind them, because none is a rendering problem:
1 (audio, answered outside eacp), 2 and 2b (input), 3 (CPM app bundles), 10
(`-fno-gnu-unique`).

---

## Status: the gap log against today's eacp

| # | Entry | Status |
|---|---|---|
| 1 | No audio subsystem | Open — answered outside eacp (MakeASound + Nuked-OPL3) |
| 2 | Modifier keys produce no key events | Open |
| 2b | `charactersIgnoringModifiers` is macOS-only | Open |
| 3 | CPM consumers don't get app-bundle setup | Open |
| 4 | No display-metrics API | **Closed** — see **E3** |
| 5 | No declarative window aspect-ratio constraint | **Closed** — `WindowOptions::aspectRatio` |
| 6 | The shader EDSL has almost no scalar maths | **Closed** — the full intrinsic set landed |
| 7 | No offscreen render targets | **Closed** — `TextureDescriptor::depth`. See **E1** |
| 8 | No cull-mode state | **Closed** — see **E2** |
| 9 | A `View` cannot reach the `Window` it is in | **Closed** — see **E4** |
| 10 | `-fno-gnu-unique` is added for every language | Open |
| 11 | eacp binds the uniform buffer to both stages | **Closed** — see **E5** |

Gap 6 is not merely closed, it is obsolete as written: the EDSL now has `floor
fract abs min max clamp step smoothstep mix sign fmod pow sqrt rsqrt exp log ceil
round atan2 dot cross normalize length distance reflect`, each taking a float
literal in any argument position, plus the `Int`/`Bool` vector families, statements
(`var`, `select`, `ifThen`, `loop`), `Array<T, N>`, and texel `fetch`. The entry's
premise — "B2 dodged this by letting the samplers do the work" — describes a
constraint that no longer exists.

Gap 7 was the one to read carefully, and **E1 has since answered it** on the
`puredoom` branch. `TextureDescriptor::renderTarget` and
`Frame::beginPass(target, …)` were real, but `Frame.h:56` said it outright:
*"Multisampling and depth are deliberately absent."* The GPU world path sets
`setDepth(true)` and depends on it, so the world could not be rendered into a
texture — the one thing this port wanted offscreen targets for.
`TextureDescriptor::depth` is what that entry became; see **E1** for what it
turned out to cost and what it taught.

---

## Part 1 — Work available in this repository today

No eacp change needed for any of these. Ranked by value per unit of risk. **P1 and
P2 are done** (see **Sequencing**); they are kept below because each says what it
was for, and P1's caveat governs everything in this part.

### P1. `fetch()` for the COLORMAP and palette lookups — **done**

`DoomShader.h:30-47` reaches both lookup tables through `sample` with hand-written
half-texel arithmetic: `(index + 0.5f) / 256.0f` and `(row + 0.5f) /
colormapRows`, leaning on `TextureFilter::Nearest` to round back to the texel that
arithmetic was aimed at. `fetch(t, Int2)` (`ShaderValue.h:422`) reads a texel by
its integer coordinate with no sampler, no filtering and no wrap, so the whole
class of off-by-one-row error stops being possible by construction.

This is the same hazard family that produced the automap collapse recorded in
`CLAUDE.md` under `Tests/Port` — a units-and-rounding mismatch that compiles
clean. Deleting the arithmetic is better than getting it right again.

The wall/flat sampler stays exactly as it is: `WorldShader`'s `texture.sampling`
must remain `{Nearest, Repeat}`, because a floor's UVs are world coordinates over
64 and run to hundreds. Only the two lookup tables move to `fetch`.

**Caveat, and it applies to every shader change below.** The GPU path has no
golden over it. `Tests/Port` covers `Engine::buildGeometry` and
`Engine::buildAutomap` — the *builders*, as data — and the frame goldens run the
software renderer, which does not execute a line of the shaders. A shader change
is eyeball-verified. Weigh that when sequencing.

Spectre fuzz sharpened both halves of that and neither is a golden. The **data**
half went where it belongs: `Port/spectresAreFuzzed` spawns a spectre into a level
that holds none and reads the split off the vertex counts, which is a decision the
port makes and can therefore be tested. The **picture** half now has a method
rather than a glance — capture the window on its own (`screencapture -l<id>`, never
the screen) with `MTL_DEBUG_LAYER=1` on, and read the validation layer's silence as
the other half of the measurement. Breaking a shader's *intent* still needs a
deliberate experiment: marking every surface, or forcing the weapon's flag, each
for one build.

### P2. `WindowOptions::aspectRatio` — **done**

`Layout.h:31` sets `options.onWillResize = keepDisplayAspect` and `Layout.h:9-21`
hand-rolls the snap. `Window.h:124` now carries `std::optional<Point>
aspectRatio`, honoured by macOS's native `setContentAspectRatio`
(`Window-macOS.mm:399-403`) and by a `WM_SIZING` snap on Windows
(`Window-Windows.cpp:370-395`). The native path anchors resize better and also
governs zoom, which is what the gap entry asked for.

Delete `keepDisplayAspect`. `letterboxedDisplayRect` stays — its comment about
zoom and fullscreen is still true, and the letterbox is still needed there.

### P3. Instancing for billboards and HUD sprites — **measured, and not worth it**

**The performance case is gone.** Sprites are 12% of the emitted vertices and
`emitSprite` does not reach 0.5% of the profile: every thing in the level is
already cheaper than the walls around it. See **The measurement**. What survives is
the invariant below — the shader cannot get a billboard's facing wrong the way the
CPU can — which is a reason to want it that has nothing to do with speed. The
original entry follows.

`instanceInput(&Instance::field, slot)` + `setInstances` + `pass.drawInstanced`
(`ShaderProgram.h:47-53`, and `RenderPass.h`'s `drawInstanced(Program&, int, int)`).
Two places:

- **Things.** `Engine::buildGeometry` expands every visible thing into a
  camera-facing quad on the CPU, every refresh (`View::renderWorld`). As one
  instance record per thing — world position, size, frame UV, light, falloff — the
  quad expansion moves into the vertex shader, which *has* the camera. The
  constraint recorded in `CLAUDE.md` ("billboards must be built around the camera
  being drawn from") stops being something the CPU has to honour and becomes
  something the shader cannot get wrong.

  Note what spectre fuzz added here: a thing is now emitted into one of *two*
  streams, so an instanced emitter has to keep that split. It is one more field on
  the instance record, or two buffers.
- **The weapon.** `View::drawWeapon` issues one `pass.draw(hudShader)` per HUD
  sprite with every uniform rebound between them. There are at most a handful, so
  this is tidiness rather than throughput.

`firstInstance`, that call's last parameter, lets one shared instance buffer be drawn in
per-texture runs, which fits the existing group-by-texture loop unchanged. **Be
honest about the win**: it is CPU work and a nicer invariant, not draw count. The
draw count stays one per sprite texture until **E6**.

### P4. `Uniform<InputBuffer>` for per-sector heights — **measured, and not worth it**

Structurally the biggest thing available, and the one most likely to be a mistake
if taken on faith. `View::renderWorld` rebuilds the world and re-uploads it — up to
262,144 vertices (`Common.h`) — every refresh, largely because sector floor and
ceiling heights move. A small per-sector buffer read by the *vertex* stage would
make wall and flat geometry static per level.

Two caveats, both real:

- Wall UVs derive from those same heights through the pegging rules. That is
  arithmetic the vertex shader can do, but it is a rewrite of the emitter, not a
  swap.
- Animated textures change which texture a wall belongs to, frame to frame, and
  the grouping is what the draw loop is built on.

**Do not start this without measuring the rebuild first.** It is worth it only if
`buildGeometry` and the upload are actually hot, and nothing in this repository
has measured them.

**Now measured, and they are not.** The rebuild is under 1% of a refresh and the
upload — the 262,144-vertex re-upload this entry is built around — is 4 to 13
microseconds, 2% of the frame's CPU. Walls and flats are the largest share of what
is left, so this entry does aim at the right part; the part is just very small.
Against that: a rewrite of the emitter with the pegging arithmetic moved into the
vertex shader, and an answer to animated textures changing which draw a wall
belongs to. **Do not build it.** See **The measurement**.

### P5. A full-resolution melt — **done**, and the entry's premise was wrong

Built here, no eacp change. The improvement is real and visible: at the end of
every level the whole screen used to drop to 320x200 at the instant the melt
started, and now the level slides away at the window's resolution over the
intermission artwork it reveals. Verified by capturing the same scripted
`Doom::exitLevel()` twice, once with the new path forced off.

**Two things this entry got wrong, and the second is the one worth keeping.**

- *"The world target is the outgoing frame."* It is not. The world target holds
  the 3D viewport in **index** space, before the resolve, with no weapon, no
  status bar and no overlay on it. The outgoing frame vanilla captures is
  `screens[0]` — everything, composited.
- *The melts that would benefit never reach `drawWipe`.* `Engine::viewActive()`
  requires `gamestate == Level`, and a melt out of a level runs with `gamestate`
  already moved on to the intermission. So the entire transition is on the
  **software path**, where the engine has composited both halves into `screens[0]`
  at 320x200 and the port simply draws it. No amount of improving the GPU melt's
  outgoing texture could have touched the case the entry was written for — and the
  cases `drawWipe` *does* handle (intermission→level, title→level) have an outgoing
  frame that is genuinely 320x200 artwork and was already right.

  **The general form: before improving a path, check that the case you care about
  goes down it.** Both wrong claims read as obviously true from the renderer's
  feature list, and neither survives reading `viewActive()`.

**What it actually took.** The finished frame is composited into a full-window
colour target and blitted to the drawable whenever the GPU renderer owns the view,
so the frame that was on the screen is still somewhere after it has been presented.
Nothing is composited into it while a melt is running, which is what leaves it
holding the frame the melt began over. `Engine::wipeIncoming` exports the incoming
screen so the software path can draw that half alone with the capture over it —
drawing the capture over the engine's own composite would leave the copy it
replaces showing wherever the two disagreed by a pixel.

It costs one extra pass and one full-screen draw while in a level, and **that was
measured rather than waved at**: the display holds 120.0 refreshes a second with
the capture and 120.0 without it.

**And it has a gate**, which is more than the rest of the port's melt ever had.
`Tests/Port/WipeTests.cpp` recomposites from `buildWipe` and `wipeIncoming` by the
shader's own rule and holds it against `screens[0]` every tic of a real melt, with
three vacuity guards and three demonstrated breakages. See `CLAUDE.md` under
`Tests/Port`.

One departure from vanilla, recorded at `CaptureShader`: the engine slides indices
and resolves them through the current palette; the capture was resolved when taken.
They differ only if the palette changes mid-melt, and then the captured answer is
the one that matches what was on the screen.

---

## Part 2 — What eacp needs

Ranked by what unblocks the most here. Each names what already exists in eacp to
build on, because in every case something does.

### E1. A depth attachment on an offscreen pass — **done**

Built on `~/Code/eacp-puredoom`, branch `puredoom`, and not upstream, so the gap
log keeps its entry until it merges. All 809 eacp tests pass and so do this
port's 120 against it — and spectre fuzz, the thing it was built for, is now
running on top of it (sequencing step 3).

**What shipped.** `TextureDescriptor::depth`, beside `renderTarget` and
`computeWrite` — the first of the two shapes below, and it held up: the buffer is
created with the colour texture and dies with it, so a render target stays one
object with no second lifetime to keep in step, and `Texture::hasDepth()` is what
a pipeline gets built from. Metal attaches a private `Depth32Float`, cleared to
the far plane and `DontCare`-stored exactly as the drawable pass does it; D3D12
gets a `D32_FLOAT` resource with its own DSV heap, created in `DEPTH_WRITE` and
left there so it needs no barrier, with the optimised clear value D3D12 demands.
Four cases in `Tests/GPU/RenderTargetDepthTests.cpp`.

**What it taught, which is worth more than the feature.** Breaking the attachment
on purpose — the sharpness check the goldens here are held to — left the new test
**green**. On Apple silicon the tile memory is there whether or not anything
attached a depth buffer, so the hardware goes on depth-testing while Metal's
validation layer reports `MTLDepthStencilDescriptor sets depth test but
MTLRenderPassDescriptor has a nil depthAttachment texture` for every draw. **A
passing render test is not evidence that an attachment happened.** Run the GPU
suite under `MTL_DEBUG_LAYER=1` and treat a silent validation layer as the other
half of the measurement — which, with the real implementation in, it is. D3D12
has no such luck, since `OMSetRenderTargets` with a null DSV genuinely disables
the test.

**One consequence for this port** — predicted here, and it cost nothing. A texture
pass is single-sampled, so the world rendered offscreen needs a pipeline at
`sampleCount` 1. This view has always been `setSampleCount(1)`, so the world's
pipeline already was one; what it did have to learn is the target's *pixel format*,
which is the other half of the same sentence in `Frame.h` and the one that would
have failed the draw. `View::prepareTargetShader` says both in one place.

---

The original entry, kept because it is what the work was scoped against:

**What.** `Frame::beginPass(const Texture&, …)` renders into a colour attachment
and nothing else (`Frame-Apple.mm:157-183`, `Frame-Windows.cpp:256+`).

**Why it matters here.** It is the last thing standing between this port and the
last item on its renderer list. **Spectre fuzz (B4)** is a read of the pixels
beneath the sprite; the faithful implementation is world→texture, then a fuzz pass
sampling that texture at a jittered offset through COLORMAP row 6. The world needs
depth testing, so the world cannot go into a texture, so there is nothing to
sample. The **screen melt** is blocked by the same wall from a second direction:
it composites the outgoing frame, and that frame stays a 320x200 software capture
rather than a full-resolution GPU one for exactly this reason.

**What already exists.** Nearly all of it. `OffscreenTarget` already carries a
`depthTexture` (`Frame.h:24`) and the snapshot path already uses it
(`Frame-Apple.mm:32`). The drawable pass already creates a `Depth32Float` texture
(`GPUView-Apple.mm:126-141`) and attaches it (`Frame-Apple.mm:136-142`). D3D12 has
`D3D12DepthTarget` and already passes a DSV to `OMSetRenderTargets` when the frame
has one (`Frame-Windows.cpp:210-213`). The texture-target `beginPass` is the one
path that does not reach for any of it.

**Shape.** A `depth` companion on `TextureDescriptor` beside `renderTarget` and
`computeWrite` (`Texture.h:144-157`) reads naturally and keeps the lifetime with
the target it belongs to. A `RenderPassDescriptor` field naming an app-owned depth
texture is the other option and is more flexible for a shared depth buffer.

MSAA on an offscreen pass is genuinely optional and should stay absent unless
something asks; depth is not.

### E2. Cull mode (gap 8) — **done**

Built on `~/Code/eacp-puredoom` at `f002eba`. `RenderPipelineDescriptor::cullMode`
reaches Metal's encoder and D3D12's rasterizer desc; `None` stays the default.

**The winding was the work, and the entry underestimated it.** It said the two
backends "want a winding convention stated once", which reads like paperwork. They
want more than that: both default to *"clockwise is front-facing"* and mean
different things by it — Metal decides facing in **clip** space and D3D12 in
**screen** space, one viewport y-flip apart — so a mesh culled correctly on one is
inside out on the other. eacp now states the convention in the space a shader is
written in (counter-clockwise in clip space, as glTF has it) and configures each
backend to produce it.

**How that was settled is the transferable part.** The first attempt reasoned it
out from the y-flip and got it backwards; the test failed, and the answer came from
printing the pixels and from a second experiment that proved the snapshot path does
not flip (a clip-space top-half quad lands at image row 0). *Reasoning about
handedness is not evidence; a rendered pixel is.*

**And the D3D12 half was wrong too, which is the part worth keeping.** This entry
used to end by saying the Metal half was measured while the D3D12 half was "what
its rasterizer rule implies", with `CullModeTests` there to say so on Windows if
the implication was wrong "rather than an app finding its world inside out." The
first Windows run of that test failed exactly two cases —
`CullMode/backKeepsTheFrontFace` and `CullMode/frontKeepsTheBackFace`, on all four
Windows rows — and the fix is upstream as `5bc7ec2`.

So the inference *was* wrong, on the backend where reasoning was all there was, and
the gate caught it on its first exposure rather than an app doing so. That is worth
more than the feature: **a cross-backend convention cannot be established on one
backend and inferred onto the other.** Both halves are measured now.

Four cases, and the shape is the point: two quads of opposite winding side by side,
so `None` is a control that makes `Front` and `Back` mean something and neither can
pass by drawing nothing. The fourth covers what Metal's encoder state costs — the
mode is set on every `setPipeline`, not only the culling ones — and was demonstrated
sharp by gating it on `cullMode != None`.

`ShaderProgram::prepare` gained a **descriptor overload** on the way, because cull
mode had nowhere else to go: the positional form was already five arguments deep.
That is **I5**, answered as a side effect rather than as its own item.

**This port has not enabled culling**, and should not until something measures the
winding `Engine::buildGeometry` emits. Walls come from both sides of a linedef and
floors from clipped subsector polygons; a wrongly-wound triangle under culling does
not draw wrongly, it does not draw at all — which is the Windows-missing-floors
failure again, and `Tests/Port/GeometryTests` is where the measurement belongs.

### E3. Display metrics (gap 4) — **done**

Built on `~/Code/eacp-puredoom` at `3c19ea9`. `Graphics::primaryDisplay()` returns a
frame, a work area and a backing scale, in **points** — the unit
`WindowOptions::width` is already in, so a size read from it goes straight to a
window with no conversion.

Two things it is careful about, and each is a way the naive version is wrong. An
implementation handing back **pixels** would open every window at twice the intended
size on a Retina panel while passing any is-it-positive check, which is what
`Display/frameIsInPoints` exists to catch. And the **work area** is not the frame:
the difference is the menu bar and the Dock, or the taskbar — measured here as
1312x848 with a 1312x822 work area at y=26.

The macOS backend flips AppKit's bottom-left origin about the **primary** screen
rather than about the screen being converted, which is the subtlety a single-monitor
test cannot see: a display sitting above or below the primary one otherwise comes
back in the wrong place. Enumerating those other displays is the obvious next step
and is not done.

**The port's half**: `Layout.h`'s `windowScale()` replaced the 3x guess with the
largest *whole* multiple of 320x240 that fits 90% of the work area, capped at 4.
Whole, because a fractional multiple puts a texel grid on a pixel grid it does not
divide into. It picks 3 on the machine this was built on — the same number the guess
had, now derived — and 4 on a larger display.

### E4. A `View` can reach its `Window` (gap 9) — **done**

Built on `~/Code/eacp-puredoom` at `c3e9026`. `View::getWindow()` returns the window
or null; `Window::setContentView` is what establishes the link, and everything under
the adopted view walks up to find it.

**The lifetime is the whole of the design.** A back-pointer that outlives what it
points at is worse than none, so `Window` owns the link as a *member* rather than
clearing it from each platform's destructor — three of which are `= default`, and a
fourth would have to be remembered the day a fourth platform arrives. Four tests over
the four answers it can give (none yet, this one, none any more, none after the
window adopted someone else), and the lifetime case was demonstrated sharp by
emptying that destructor.

**The port's half**: `View` no longer takes a `Graphics::Window&`, so `App`'s member
order is an order again rather than a constraint. Verified in the running app, not
only in eacp's tests — a probe printed a non-null `getWindow()` on the first
refresh.

### E5. Gate the uniform bind on the stage that reads it (gap 11) — **done**

Built on `~/Code/eacp-puredoom`, branch `puredoom`, at `a644b3e`, and not
upstream, so the gap log keeps its entry until it merges. All 811 eacp tests
pass (one pre-existing failure aside, below) and so do this port's 120 against
it.

**What shipped.** The predicate the emitter already computed is now the one the
bind asks: `vertexReadsUniforms`/`fragmentReadsUniforms` are public in
`ShaderEmitter.h`, `GeneratedShader` carries both, `ShaderProgram` hands them
on, and `draw`/`drawInstanced` go through a new
`RenderPass::setUniforms(program)`. One walk decides both the signature and the
bind aimed at it, so they cannot drift — which is worth more than the saved
bind, and is the same argument as `Tests/Port` covering the port's builders.

**What it is worth here.** `setUniforms` is also the call app code should make
when it hand-rolls a draw over its own geometry, so it is I1's workaround made
one line shorter rather than longer: `View::drawGeometry` and the automap draw
both use it. Two of this port's shaders are the case the entry was written for —
`FuzzShader` and `HudFuzzShader` write a constant colour, so their fragment
stage declares no block and was bound anyway, every frame.

**Two tests, demonstrated sharp** the way this repository's goldens are: forcing
`vertexReadsUniforms` true fails both. `codegenUniformStages` checks each flag
against the emitted Metal signature beside it, over a vertex-only, a
fragment-only and a declared-but-unread uniform; `codegenUniformInStatementBinds`
covers what the colour expression alone would miss — a uniform read only from
inside a branch, which is the trap the declaration walk already guards against
and the bind now inherits.

**Measured after**: the app run under `MTL_DEBUG_LAYER=1` prints `Metal API
Validation Enabled` and then nothing at all across the title, the attract demo
and the menu, with the picture unchanged. No before/after count of the unused
binds themselves — gap 11 says they land in Xcode's runtime-issues panel, which
a terminal capture cannot read.

**One thing found on the way, and it is eacp's, not this port's.**
`GPU/textureUpdates` **aborts** under `MTL_DEBUG_LAYER=1` and passes without it:
the test feeds `Texture::update` a `bytesPerRow` of 10 to exercise the stride
path, and Metal requires a multiple of the pixel size (4 for RGBA8). Since the
validation layer's silence is the measurement standard for a shader change here,
a suite that aborts under it costs something real. Left alone — it is not E5's
to fix, and the stride the test means to exercise is any multiple of 4 greater
than `width * 4`.

### E6. Texture arrays, or an atlas primitive

A new entry, surfaced by this port and not yet in the gap log. There is no
`Texture2DArray` and no array-slice binding anywhere in the GPU module. That is
why the world is drawn as one draw per texture (`View::drawGeometry`'s run loop)
and why P3's
instancing wins CPU work rather than draw count. With an array texture — or a
sampler-visible atlas with a slice index per vertex — the entire level collapses
into a single draw, and the group-by-texture bookkeeping in `buildGeometry`
disappears with it.

**Measured**: 119 to 131 draws a frame across the three attract demos, costing
26-31% of `render()`'s CPU — the largest single item left in the renderer, and
still around 1% of a refresh. So this stays on the list, but **for its shape rather
than its speed**: the bookkeeping goes, and asking **I1** to put *one* draw over
app-owned geometry back on the supported path is a far smaller request than asking
it for 125.

### E7. `R8Unorm` as a `PixelFormat`, and a latent mismatch worth fixing regardless

`PixelFormat` (`RenderPipeline.h:17-23`) has BGRA8, RGBA8, RGBA16F and RGBA32F —
no R8 — so a single-channel render target is not expressible.

**Written as the thing blocking the faithful spectre fuzz, and that was wrong.**
The claim was that only an R8 target could keep the world in palette-index space
and so let the fuzz remap through row 6 exactly as `R_DrawFuzzColumn` does. The
port has since done exactly that on the RGBA8 fallback: index space was what made
it faithful, and RGBA8 round-trips an 8-bit unorm index exactly, so the picture is
vanilla's own algorithm and the format never entered into it. The extra channels
then turned out to be *load-bearing* — a spectre raises its mark in green while
leaving the index in red alone, which is one more channel and an additive blend —
so R8 could not have carried this at all, and what it would save is two channels
rather than three. Worth having; not blocking.

Independently: `pixelFormatFor(TextureFormat::R8Unorm)` falls through the `default:`
at `RenderPipeline.h:35` and returns `PixelFormat::RGBA8Unorm` — a silent
disagreement between a pipeline and its attachment, of the kind the same function's
comment says neither backend will accept. Unreachable today because R8 cannot be a
render target. It stops being unreachable the moment this entry lands.

---

## Part 3 — Interface, as a consumer sees it

The feature list above is the easy half. These are the places where eacp can do
the thing but the shape of the API made this port write something awkward, and
they are worth as much as any feature.

### I1. `ShaderProgram` owns its vertex buffer, so app-owned geometry falls off the path

**Open, and the clearest interface finding of the port.**

`pass.draw(program)` (`RenderPass.h`, `draw(Program&)`) does six things: sets the
pipeline, binds `program.vertices()`, binds the uniform block to the stage that
reads it, binds textures, binds storage buffers, and issues the draw. It assumes
the program owns its geometry.

The world's geometry is app-owned and persistent — a `Buffer` updated in place
every frame — and it is drawn as sub-ranges with a different texture per range. So
`View::drawGeometry` cannot use `draw(program)` and instead reassembles it by hand:

```cpp
pass.setPipeline(shader.pipeline());
pass.setVertexBuffer(worldBuffer);
pass.setUniforms(shader);

for (const auto& run: runs)
{
    shader.texture = textureFor(run.textureId);
    shader.bindTextures(pass);
    pass.draw(run.vertexCount, run.firstVertex);
}
```

Every line of that is eacp's own `draw(program)` body, inlined because one
assumption in it does not hold. A `draw(program, buffer, vertexCount,
firstVertex)` overload — or a program that can be told its geometry lives
elsewhere — puts the largest draw in the whole renderer back on the supported
path.

**E5 shortened it by a line without answering it**, which is worth noting because
it is the shape of a half-fix: `setUniforms(program)` replaced the two per-stage
setters, so the hand-rolled body now *shares* eacp's per-stage rule instead of
reimplementing it. The body is still hand-rolled, and still has to be kept in step
with `draw(program)` by hand.

Spectre fuzz made it worse in the way that confirms the diagnosis: the fuzz marks
come off the *same* app-owned buffer with a different program, so the hand-rolled
body had to be lifted into a function taking a `WorldViewShader&` and called twice.
The workaround is now a small abstraction of eacp's own draw, living here.

### I2. `bindTextures` is public only because `draw(program)` calls it

**Open**, and a direct consequence of I1: `program.bindTextures(pass)` reads like
an internal — its own comment says "RenderPass::draw(program) calls this" — and app
code has to call it because it took the draw apart. Fix I1 and this leaves app code
on its own.

### I3. A texture's sampling is fixed when the shader compiles — **already answered**

**Withdrawn: eacp had already written this down, and this entry was the result of
reading the header rather than the document beside it.** `SAMPLERS.md` states both
halves — the sampling is fixed at `compile()` (with the `texture.sampling` line
shown in place), and *"a program cannot change its mind per draw… needs one
compiled program per configuration"*, with `Sprites::SpriteRenderer` as the worked
case that keeps one shader per configuration.

Worth keeping as a **finding about this plan** rather than about eacp: an entry
asking for documentation is the one kind that costs nothing to write and can be
wrong the moment it is written. Check the prose next to the code before filing one.

### I4. `setFramesInFlight` means two different things — **already answered**

**Withdrawn, same way.** The entry asked that eacp's header say at the declaration
what this port's gap log said. `GPUView.h` already does, at more length and with
the same measurement — the DXGI present queue against Metal's
`maximumDrawableCount`, *"a smaller pool measurably raises latency: on the Maze
view, sample-to-screen goes from 23ms at three to 32ms at two"*.

That the number matches this port's own measurement of it is the interesting part:
the two were measured independently, on different content, and agree.

### I5. `prepare(int sampleCount, bool depth)` is a positional bool

Written as minor, and mentioned only because it is at the front door of every
shader: this port writes `shader.prepare(sampleCount(), true)`, and nothing at the
call site says what `true` is.

**Rendering into a texture is what promoted it.** The tail of that parameter list
is where a *target's* answers live, and a shader drawing into the world target has
to give four of them — sample count 1, depth, topology, pixel format — so the call
becomes

```cpp
shader.prepare(1,
               true,
               GPU::PrimitiveTopology::Triangles,
               blend,
               GPU::pixelFormatFor(worldTargetFormat));
```

five positional arguments of which three exist only to reach the fifth, and two of
them (the sample count and the format) are not the shader's choice at all but the
target's. `View::prepareTargetShader` hides it, which is the workaround and also
the shape of the fix: a `prepare(const Texture&, …)` overload could read all four
off the target it is handed, and a descriptor would at least name them.

**Half-answered, by E2 rather than on its own.** `ShaderProgram::prepare` now takes
a `RenderPipelineDescriptor` as well, so every field has a name at the call site —
which is what let cull mode land at all, there being no sixth positional slot worth
adding. What is *not* answered is the better half of the entry: the descriptor still
has to be filled in by hand from a target the caller is holding, so
`prepareTargetShader` still exists to do it. A `prepare(const Texture&, …)` that
reads the sample count, the depth and the format off the target remains the fix.

---

## Part 4 — Edits to the gap log in `CLAUDE.md` — **done**

The gap log is the deliverable of goal 2, so it should be accurate. All of the
below has landed:

- ~~**Close 5**~~ and move it to the "already merged" paragraph.
- ~~**Close 6.**~~ Replaced with nothing; the entry's premise is gone.
- ~~**Rewrite 7**~~ from "no offscreen render targets" to **"an offscreen pass has
  no depth attachment"**, with the B4 and melt consequences named. That is the
  entry E1 answers, and it is sharper than what it replaces.
- ~~**Add** E6 (no texture arrays) and E7 (no single-channel render-target
  format)~~ — 12 and 13 in the log.
- ~~**Add** the Part 3 items as an *interface* section of the log.~~
- Leave 1, 2, 2b, 3, 4, 8, 9, 10 and 11 as they stand — all re-verified against
  `a114455`.

Since then, building spectre fuzz on top of E1 revised three of them, and the
revisions are in the log rather than here: **7** gains what the feature cost in
practice (nothing for the sample count, and the melt unblocked as a side effect),
**13** is downgraded — the fuzz mask needs a second channel, so R8 could not have
served the case the entry was written for — and **I5** is upgraded, for the reason
above.

---

## Sequencing

1. ~~**P2** (aspect ratio) and the **Part 4** gap-log edits.~~ **Done.**
   `keepDisplayAspect` is gone; the window carries `aspectRatio` instead. The gap
   log closes 5 and 6, rewrites 7 as "an offscreen pass has no depth attachment",
   adds 12 (texture arrays) and 13 (R8 render target), and gains an **Interface
   findings** section carrying I1–I5.
2. ~~**P1** (`fetch`).~~ **Done, and now eyeballed.** Both lookups are
   `fetch(t, Int2)`. Two things the sampler had been supplying silently are now
   written down: the rounding (`texelOf` adds the half before truncating, because
   an index is a unorm scaled back up) and the **clamp** — `Clamp` addressing was
   holding the light row at the table's first entry, and a texel outside a
   `fetch` reads zero instead, which without the clamp would have drawn every
   near surface black. The picture was compared against the software frame while
   building the item below, and the light banding, the palette and the flats all
   come out where they were.
3. ~~**E1** in eacp, then **spectre fuzz here**.~~ **Done — the renderer's last
   item.** What it cost, beyond the fuzz itself: the world now renders into a
   texture in *index* space and a full-screen pass resolves it, because a pass
   cannot sample the target it is drawing into. See **Renderer status** in
   `CLAUDE.md` for the three pieces and for the additive mark that is what keeps
   the pixels behind a spectre readable.

   Three things it settled that were open questions here. The sample-count
   consequence recorded under E1 **cost nothing** — this view has always been
   `setSampleCount(1)`. Entry **13** (R8 render targets) turns out not to have
   been blocking this at all, and is worth less than it looked: the fuzz mask
   needs a second channel regardless, so what R8 would save is two channels
   rather than three. And **I5** got sharper, a pipeline that names a target
   needing five positional arguments of which three exist only to reach the
   fifth.

   Still open, and no longer blocked: the **melt** composites a 320x200 software
   capture of the outgoing frame, which was a constraint and is now only a
   choice — the world target could be it. That is **P5**. (It could not, as step
   6 records; but the melt was genuinely unblocked, and by the same fact — a frame
   the GPU drew can be kept in a texture.)

**Where it stands: the renderer's feature list is finished, and so is the eacp
work it was waiting on.** Of the gap log's thirteen entries, seven are closed and
the six that remain are one that eacp was never going to answer (1, audio), two
input gaps (2, 2b), a CMake one (3), a toolchain one (10), and the two this port
added last (12, texture arrays; 13, R8 targets) — neither blocking. What is left
below is measurement, and one item that measurement should decide.

4. ~~**E2**, **E5**, **E3**, **E4** in eacp.~~ **Done — all four, plus I5 as a side
   effect.** `eacp-puredoom` at `a644b3e`, `f002eba`, `3c19ea9`, `c3e9026`; 823 eacp
   tests and this port's 120 green against the last of them. Three of the four
   removed a workaround here (the two hand-rolled draws bind through
   `setUniforms`, the window is sized from the display, `View` no longer takes a
   `Graphics::Window&`) and **E2 deliberately did not** — see its entry for why
   enabling culling needs `buildGeometry`'s winding measured first.

   **What the batch taught, which is more than the features.** Each of the four was
   written up as small and independent, and each turned out to hide a decision the
   entry had not noticed:

   - **E5** was "expose a predicate", and the value is that the bind and the
     signature it is aimed at now come from *one* walk. A predicate computed twice
     is a predicate that can disagree with itself.
   - **E2** was "add a cull field", and the field is the easy half: the two
     backends' defaults both read "clockwise is front" and mean opposite things,
     one viewport y-flip apart. **The first attempt reasoned it out and got it
     backwards** — a printed pixel corrected it, and a second experiment (a
     clip-space top-half quad landing at image row 0) ruled out the snapshot path
     as the explanation. Handedness is not a thing to reason about when a rendered
     pixel can be read.
   - **E3** was "report the screen size", and the two things that make it usable
     are *points, not pixels* and *work area, not frame* — each of which a naive
     implementation gets wrong while passing any plausible check.
   - **E4** was "a getter", and the getter is trivial; the lifetime is not. The
     back-pointer is cleared by a *member's* destructor rather than by a line in
     each platform's `= default` one, which is what makes it impossible to forget
     on the fourth platform.

   Every one of the four is pinned by tests **demonstrated sharp** the way this
   repository's goldens are — forcing the predicate true, gating the cull bind on
   `!= None`, emptying the link's destructor — because a new gate that no plausible
   mistake would fail reads as coverage and is not.
5. ~~**Measure** `buildGeometry` and the per-frame upload, then decide between
   **P3**, **P4** and **E6**.~~ **Done — and it decided against all three.** See
   **The measurement** above for the numbers and the verdicts. The whole renderer
   costs a few hundred microseconds of CPU against an 8.33ms refresh and the
   display never drops a frame, so none of the three buys anything a player could
   see; the upload the plan was most suspicious of turned out to be 2% of the
   frame's CPU.

   **What the measurement actually paid for was not on the list**: half of
   `buildGeometry` was the engine's out-of-line state accessors rather than
   geometry, and hoisting them cut it by 43% with the emitted geometry
   bit-identical. Three entries had been written against a cost nobody had
   measured, and the answer was in none of them.

   The benchmark is `Tests/Bench/GeometryBench.cpp`, kept as the target
   `port-bench` so the next change to the emitter can be held against it. The two
   things the fuzz work added and nobody had measured — a second full-frame pass
   and a full-window RGBA8 target — are inside the app figures above and are not
   separately visible; they are part of the ~30% spent submitting, and the frame
   has 95% of itself spare either way.
6. ~~**P5**, the full-resolution melt.~~ **Done — and the entry it was written as
   was wrong about both of its premises.** The world target could not be the
   outgoing frame (it is the 3D viewport in index space, with no weapon, status bar
   or overlay on it), and the melts that would have benefited never reach the GPU
   melt path at all: a melt out of a level runs with `gamestate` already on the
   intermission, so `Engine::viewActive()` is false and the whole transition is the
   software path's 320x200 composite.

   What it took instead is the finished frame composited into a target of its own
   and blitted, so the frame that was on the screen survives the frame drawn over
   it — one extra pass while in a level, measured at no cost to the refresh rate
   (120.0/sec either way). See **P5** above.

   **Two things generalise, and the first is the same lesson the measurement in
   step 5 taught one level up.** *Before improving a path, check that the case you
   care about goes down it* — both of this entry's wrong claims read as obviously
   true from the renderer's feature list and neither survives reading
   `viewActive()`. And *a plan entry written from the feature list is a hypothesis,
   not a specification*: P3, P4 and E6 were retired by measuring them, and P5 was
   rewritten by reading the code it named.

   It also closed the port's oldest untested rule. The melt's composite is
   something the engine and the port each implement and neither shares, and
   `Tests/Port/WipeTests.cpp` now holds the two against each other every tic of a
   real melt.
7. **E7**, **I1**/**I2**. What is left is one eacp feature that blocks nothing and
   one interface finding that is worth more than any feature on the list. **Next.**

Two standing constraints from `CLAUDE.md` apply throughout and are worth
restating here because everything above is renderer work:

- **eacp is never modified from this repository.** Everything in Part 2 and Part 3
  is a commit in `~/Code/eacp-puredoom`, on its branch, arriving here through
  `CPM_eacp_SOURCE` — see **Where this work happens** at the top. What lands here
  in the meantime is a workaround plus a gap-log entry.
- **The demo goldens pin the simulation and the software frames, not the GPU
  path.** Nothing in this plan should move a `*.hashes` or a `*.frames` golden. If
  one moves, the change was wrong — and `Tests/Port` is where any new coverage for
  the port's own decisions belongs, because it is the only gate that has ever
  caught a bug in them.
