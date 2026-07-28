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

Measured against eacp `main` at `a114455` (2026-07-27) — the commit the branch
above should start from, and what the default CPM fetch resolves to today. Every
claim below was checked in the checkout rather than inferred from a commit
message.

**What this repository's branch is green against**, which the rule above says has
to be stated: `eacp-puredoom` at `1f98229`, one commit past `a114455` — E1. It does
not build against plain `~/Code/eacp` and is not meant to, `TextureDescriptor::depth`
being what the world target is created with.

---

## Status: the gap log against today's eacp

| # | Entry | Status |
|---|---|---|
| 1 | No audio subsystem | Open — answered outside eacp (MakeASound + Nuked-OPL3) |
| 2 | Modifier keys produce no key events | Open |
| 2b | `charactersIgnoringModifiers` is macOS-only | Open |
| 3 | CPM consumers don't get app-bundle setup | Open |
| 4 | No display-metrics API | **Closed on the branch** — see **E3** |
| 5 | No declarative window aspect-ratio constraint | **Closed** — `WindowOptions::aspectRatio` |
| 6 | The shader EDSL has almost no scalar maths | **Closed** — the full intrinsic set landed |
| 7 | No offscreen render targets | **Closed on the branch** — `TextureDescriptor::depth`. See **E1** |
| 8 | No cull-mode state | **Closed on the branch** — see **E2** |
| 9 | A `View` cannot reach the `Window` it is in | Open — **E4** |
| 10 | `-fno-gnu-unique` is added for every language | Open |
| 11 | eacp binds the uniform buffer to both stages | **Closed on the branch** — see **E5** |

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

### P3. Instancing for billboards and HUD sprites — medium

`instanceInput(&Instance::field, slot)` + `setInstances` + `pass.drawInstanced`
(`ShaderProgram.h:47-53`, `RenderPass.h:209`). Two places:

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

`firstInstance` (`RenderPass.h:209`) lets one shared instance buffer be drawn in
per-texture runs, which fits the existing group-by-texture loop unchanged. **Be
honest about the win**: it is CPU work and a nicer invariant, not draw count. The
draw count stays one per sprite texture until **E6**.

### P4. `Uniform<InputBuffer>` for per-sector heights — large, measure first

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

### P5. A full-resolution melt — small, and newly possible

A new entry, and the one thing spectre fuzz unblocked without meaning to. The
screen melt composites the outgoing frame, which is a 320x200 software capture
(`Engine::buildWipe` → `wipeTexture`) because there was no full-resolution GPU
frame to be had. There is one now: the world target *is* the outgoing frame, at the
window's resolution, and the melt only ever reads it.

What it needs is a second target and a swap — the frame being melted away has to
survive the frame that is drawing over it, and a texture cannot be sampled by the
pass that writes it (`Frame.h` says so at `beginPass`). Two targets and a swap on
the tic the melt starts.

Two things not to lose. The outgoing frame is often *not* a world at all — the
title, the intermission and the finale are genuinely 320x200 artwork — so the
software capture stays as the other branch rather than being replaced. And the melt
reads its own outgoing frame in *index* space, so if it moves onto the target it
should composite before the resolve, not after.

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
handedness is not evidence; a rendered pixel is.* The Metal half is now measured and
the D3D12 half is what its rasterizer rule implies — and `CullModeTests` is what
says so on Windows if that implication is wrong, rather than an app finding its
world inside out.

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

### E4. A `View` can reach its `Window` (gap 9)

Everything a view needs from its window — the mouse lock, the modifier keys — is
handed to it by the app. This port declares the window before the view and passes
a `Graphics::Window&` at construction (`View::View`), which makes it impossible to
be null and constrains member order in `App` forever. A `View::getWindow()`, or a
window reference given on `setContentView`, settles it.

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
why the world is drawn as one draw per texture (`View.cpp:318-322`) and why P3's
instancing wins CPU work rather than draw count. With an array texture — or a
sampler-visible atlas with a slice index per vertex — the entire level collapses
into a single draw, and the group-by-texture bookkeeping in `buildGeometry`
disappears with it.

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

`pass.draw(program)` (`RenderPass.h:180-199`) does six things: sets the pipeline,
binds `program.vertices()`, binds the uniform block to both stages, binds textures,
binds storage buffers, and issues the draw. It assumes the program owns its
geometry.

The world's geometry is app-owned and persistent — a `Buffer` updated in place
every frame — and it is drawn as sub-ranges with a different texture per range. So
`View::drawGeometry` cannot use `draw(program)` and instead reassembles it by hand:

```cpp
pass.setPipeline(shader.pipeline());
pass.setVertexBuffer(worldBuffer);
pass.setVertexUniforms(shader);
pass.setFragmentUniforms(shader);

for (const auto& run: runs)
{
    shader.texture = textureFor(run.textureId);
    shader.bindTextures(pass);
    pass.draw(run.vertexCount, run.firstVertex);
}
```

Every line of that is eacp's own `draw(program)` body, inlined because one
assumption in it does not hold. This is the single clearest interface finding of
the port. A `draw(program, buffer, vertexCount, firstVertex)` overload — or a
program that can be told its geometry lives elsewhere — puts the largest draw in
the whole renderer back on the supported path.

Spectre fuzz made it worse in the way that confirms the diagnosis: the fuzz marks
come off the *same* app-owned buffer with a different program, so the hand-rolled
body had to be lifted into a function taking a `WorldViewShader&` and called twice.
The workaround is now a small abstraction of eacp's own draw, living here.

### I2. `bindTextures` is public only because `draw(program)` calls it

A direct consequence of I1: `program.bindTextures(pass)` reads like an internal —
it is documented as "RenderPass::draw(program) calls this"
(`ShaderProgram.h:738-741`) — and app code has to call it because it took the draw
apart. Fix I1 and this leaves app code on its own.

### I3. A texture's sampling is fixed when the shader compiles

Deliberate, documented, and with a Windows driver bug behind it (`SAMPLERS.md`),
so this is a note rather than a request. But the consequence should be written
down: `WorldShader` has to set `texture.sampling` *before* `compile()`, and a
program that wants one texture slot sampled two ways needs two programs. Worth a
sentence in the GPU README's sampling section, since the constraint is invisible
until a shader wants it.

### I4. `setFramesInFlight` means two different things

Already in this repo's gap log as a note, and it belongs in eacp's header instead:
on DXGI it is the depth of the present queue and lowering it lowers latency; on
Metal it is `maximumDrawableCount`, and lowering it *raises* latency (measured
here: 23ms at three, 32ms at two). One name, two meanings, and the wrong intuition
is the natural one. Either the header says so at the declaration, or the two are
named separately.

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
   choice — the world target could be it. That is **P5**.

**Where it stands: the renderer's feature list is finished.** Everything below is
either eacp work that removes a workaround here, or a cost that has not been
measured yet. Nothing left is a hole in the picture.

4. **E2**, **E5**, **E3**, **E4** in eacp — small, independent, and each removes a
   workaround that exists in this repository today. **E5** is the one this port
   would notice: the world target's two pipelines add passes whose vertex stage
   declares no uniform block, so Metal's validation layer now logs more unused
   binds than it did — and the validation layer's silence is the port's own
   measurement for a shader change (see P1's caveat), so noise there costs
   something real.
5. **Measure** `buildGeometry` and the per-frame upload. Only then decide between
   **P3**, **P4** and **E6**, which all aim at the same cost from different sides
   and should not all be built. The fuzz work added a second full-frame pass and a
   full-window RGBA8 target to every frame, neither measured either — so a
   measurement pass over the whole renderer is worth more now than it was.
6. **P5**, the full-resolution melt — small, self-contained, and the last thing in
   the renderer still running at 320x200 when it does not have to.

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
