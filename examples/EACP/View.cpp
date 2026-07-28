#include "View.h"
#include "Layout.h"

namespace PureDoom
{
// A fresh level or a teleport moves the camera somewhere else entirely, and
// there is nothing to interpolate across that.
static bool jumped(const Engine::Camera& from, const Engine::Camera& to)
{
    constexpr auto limit = 128.0f;

    return std::abs(to.pos.x - from.pos.x) > limit
           || std::abs(to.pos.y - from.pos.y) > limit
           || std::abs(to.pos.z - from.pos.z) > limit;
}

static float shortestTurn(float from, float to)
{
    auto turn = to - from;

    while (turn > pi)
        turn -= 2.0f * pi;

    while (turn < -pi)
        turn += 2.0f * pi;

    return turn;
}

// The heading change the engine makes from a given amount of mouse movement: it
// scales by its sensitivity, then subtracts eight times that from its heading, in
// units where a full circle is 2^32.
static float turnFor(float movement)
{
    auto sensitivity = (float) (Engine::mouseSensitivity() + 5) / 10.0f;

    return -movement * sensitivity * pi / 4096.0f;
}

static void syncModifierKey(bool pressed, bool wasPressed, Doom::Key key)
{
    if (pressed == wasPressed)
        return;

    if (pressed)
        Doom::keyDown(key);
    else
        Doom::keyUp(key);
}

View::View(Graphics::Window& windowToUse)
    : window(windowToUse)
{
    setSampleCount(1);
    setDepth(true);
    setHandlesMouseEvents(true);
    setGrabsFocusOnMouseDown(true);
    setContinuous(true);

    prepareQuadShader(screenShader);
    prepareQuadShader(resolveShader);
    prepareQuadShader(hudShader);
    prepareQuadShader(overlayShader);
    prepareQuadShader(wipeShader);
    prepareShader(automapShader);

    prepareTargetShader(worldShader, GPU::BlendMode::None);
    prepareTargetShader(fuzzShader, GPU::BlendMode::Additive);

    hudFuzzShader.setVertices(unitQuad);
    prepareTargetShader(hudFuzzShader, GPU::BlendMode::Additive);

    // The world resolves its own COLORMAP row on the way into the target; the
    // palette waits for the resolve, and the fuzz shaders write no colour at
    // all, so neither lookup is theirs.
    worldShader.colormap = colormapTexture;

    screenShader.screenIndices = framebuffer;
    overlayShader.overlay = overlayTexture;
    wipeShader.start = wipeTexture;
    wipeShader.offsets = wipeOffsetTexture;

    geometry.resize(maxVertices);
    draws.resize(maxDraws);
    automap.resize(maxAutomapVertices);
    overlayPixels.resize(overlayBytes);
    wipePixels.resize(Engine::screenPixels);
    paletteData.resize(256 * 4);
}

void View::prepareShader(DoomShader& shader) const
{
    shader.prepare(sampleCount(), true);
    shader.colormap = colormapTexture;
    shader.palette = paletteTexture;
}
void View::prepareQuadShader(ScreenQuadShader& shader)
{
    shader.setVertices(unitQuad);
    prepareShader(shader);
}
void View::prepareTargetShader(GPU::ShaderProgram& shader,
                               GPU::BlendMode blend) const
{
    shader.prepare(worldTargetSamples,
                   true,
                   GPU::PrimitiveTopology::Triangles,
                   blend,
                   GPU::pixelFormatFor(worldTargetFormat));
}
void View::update(Threads::FrameTime)
{
    // Ahead of the early return below: the device wants feeding on every
    // refresh, not only on the ones a tic falls on.
    audio.pump();

    if (window.isCommandPressed())
        window.setMouseLocked(false);

    syncModifierKeys(window.getModifiers());

    // One reading of the engine's clock answers both of the questions the
    // frame has for it: whether a tic is due, and how far into the tic the
    // frame sits. Reading it twice lets a tic boundary fall between the two,
    // and the frame is then drawn a whole tic in the past.
    auto now = Engine::ticTime();
    auto tic = (std::int64_t) now;

    ticFraction = std::clamp((float) (now - (double) tic), 0.0f, 1.0f);

    // The engine's state only moves on a tic, 35 times a second, so it is
    // left alone on the refreshes in between. A melt animates per frame.
    if (tic == lastTic && !Engine::isWiping())
        return;

    lastTic = tic;
    flushMouse();

    previousCamera = currentCamera;
    previousHud = hud;

    Engine::snapshotTic();
    Doom::updateGame();

    // The engine reveals a wall on the map by drawing it, and draws nothing
    // while the map is up - so vanilla's map stops filling in the moment it
    // is looked at. This keeps it filling in.
    Engine::revealAutomap();

    currentCamera = Engine::camera();
    hud = Engine::hudSprites();

    if (!hasCamera || jumped(previousCamera, currentCamera))
    {
        previousCamera = currentCamera;
        previousHud = hud;
    }

    hasCamera = true;
    frameChanged = true;
}
Engine::Camera View::viewCamera() const
{
    auto camera = currentCamera;

    camera.pos = Engine::lerp(previousCamera.pos, currentCamera.pos, ticFraction);
    camera.angle = viewAngle();

    return camera;
}
float View::viewAngle() const
{
    auto turn = shortestTurn(previousCamera.angle, currentCamera.angle);

    if (!predictAim)
        return previousCamera.angle + turn * ticFraction;

    auto keyboardTurn = turn - appliedTurn;

    return previousCamera.angle + keyboardTurn * ticFraction + appliedTurn
           + pendingTurn();
}
float View::pendingTurn() const
{
    return turnFor(mouseMovement.x);
}
void View::flushMouse()
{
    auto x = (int) mouseMovement.x;
    auto y = (int) mouseMovement.y;

    appliedTurn = turnFor((float) x);

    if (x == 0 && y == 0)
        return;

    Doom::mouseMove(x, y);

    // Keep the fraction, so slow movement accumulates instead of rounding
    // away to nothing.
    mouseMovement.x -= (float) x;
    mouseMovement.y -= (float) y;
}
void View::render(GPU::Frame& frame)
{
    // Everything derived from the engine's state is rebuilt only when a tic
    // has actually run.
    if (frameChanged)
    {
        framebuffer.update(Doom::framebuffer(1));
        updatePalette();
        updateOverlay();
        updateWipe();
    }

    ensureWorldTextures();

    auto bounds = getLocalBounds();
    auto dst = letterboxedDisplayRect(bounds);

    // The engine raises its wiping flag at the end of the frame that renders
    // the incoming screen and only sets the melt up on the next, so for that
    // one frame there is nothing to draw over the view with and the software
    // frame has to stand in.
    auto gpuView = gpuWorld && Engine::viewActive() && !worldTextures.empty()
                   && (!Engine::isWiping() || wipeVisible);

    setDarkenRow((float) Engine::darkenRow());

    if (!gpuView)
    {
        auto screenPass = frame.beginPass({Graphics::Color::black()});
        drawScreen(screenPass, bounds, dst, 0.0f, 1.0f);
        frameChanged = false;
        return;
    }

    // The last notch of the menu's screen size takes the status bar away, and
    // the view is then the whole frame rather than the rows above the bar.
    auto rows = Engine::viewRows();
    auto viewport = dst.withHeight(dst.h * worldViewportShare(rows));

    // The engine skips the 3D view entirely while the automap is up, so the
    // two never share the frame.
    auto onMap = Engine::automapActive();

    // Into a texture, and before the screen's pass opens: what a spectre shows
    // is the frame behind it, and no pass can read the one it is writing.
    auto worldDrawn = !onMap && renderWorld(frame, bounds, viewport, rows);

    auto pass = frame.beginPass({Graphics::Color::black()});

    if (onMap)
        drawAutomap(pass, bounds, viewport);
    else
    {
        if (worldDrawn)
            resolveWorld(pass, bounds, viewport, rows);

        drawWeapon(pass, bounds, viewport, rows);
    }

    // With no status bar there is no strip to composite: the rows it sat in
    // hold the software renderer's own view of the world, which is the one
    // thing that must not reach the screen.
    if (Engine::statusBarVisible())
        drawScreen(pass,
                   bounds,
                   statusBarRect(dst, rows),
                   rows / Engine::screenHeight,
                   1.0f);

    // Over the whole frame, status bar included: the melt slides the outgoing
    // screen down across all 200 rows.
    if (wipeVisible)
        drawWipe(pass, bounds, dst);

    // The menu, the messages and the PAUSE graphic are already in the
    // software frame whenever that is what is on the screen; over the GPU
    // view they have to be put back.
    if (overlayVisible)
        drawOverlay(pass, bounds, dst);

    frameChanged = false;
}
void View::setDarkenRow(float row)
{
    worldShader.darkenRow = row;
    hudShader.darkenRow = row;
    automapShader.darkenRow = row;
    overlayShader.darkenRow = row;
}
Graphics::Rect View::statusBarRect(const Graphics::Rect& dst, float rows)
{
    auto share = worldViewportShare(rows);

    return {dst.x, dst.y + dst.h * share, dst.w, dst.h * (1.0f - share)};
}
void View::drawScreen(GPU::RenderPass& pass,
                      const Graphics::Rect& bounds,
                      const Graphics::Rect& dst,
                      float uvTop,
                      float uvBottom)
{
    screenShader.setDestination(bounds, dst);
    screenShader.uvY = std::array {uvTop, uvBottom};
    pass.draw(screenShader);
}
bool View::renderWorld(GPU::Frame& frame,
                       const Graphics::Rect& bounds,
                       const Graphics::Rect& viewport,
                       float rows)
{
    ensureWorldTarget();

    if (!worldTarget)
        return false;

    // Rebuilt every frame rather than every tic, because the billboards and
    // the sky are built around the camera being drawn from, and that moves
    // with the view between tics rather than with the engine.
    auto camera = viewCamera();
    auto world = Engine::buildGeometry(camera, ticFraction, {geometry, draws});

    if (world.draws.empty())
        return false;

    worldBuffer.update(world.vertices.data(), world.vertices.size_bytes());

    worldShader.setView(camera, bounds, viewport, rows);
    fuzzShader.setView(camera, bounds, viewport, rows);

    auto pass = frame.beginPass(*worldTarget, {Graphics::Color::black()});

    drawGeometry(pass, worldShader, world.draws);

    // Last, and only now: a mark stands for the pixels beneath it, so they all
    // have to be there. Depth keeps a spectre behind a wall from being marked
    // at all - the weapon needs no such test, being in front of everything.
    drawGeometry(pass, fuzzShader, world.fuzzDraws);
    markWeaponFuzz(pass, bounds, viewport, rows);

    return true;
}
void View::drawGeometry(GPU::RenderPass& pass,
                        WorldViewShader& shader,
                        std::span<const Engine::TextureDraw> runs)
{
    if (runs.empty())
        return;

    pass.setPipeline(shader.pipeline());
    pass.setVertexBuffer(worldBuffer);
    pass.setUniforms(shader);

    for (const auto& run: runs)
    {
        shader.texture = textureFor(run.textureId);
        shader.bindTextures(pass);
        pass.draw(run.vertexCount, run.firstVertex);
    }
}
void View::resolveWorld(GPU::RenderPass& pass,
                        const Graphics::Rect& bounds,
                        const Graphics::Rect& viewport,
                        float rows)
{
    auto scale = backingScale();

    resolveShader.setDestination(bounds, viewport);
    resolveShader.targetSize = std::array {targetPixels.x, targetPixels.y};

    // One column and one row of DOOM's own frame, in this target's pixels: the
    // fuzz keeps 1993's grain however large the window is.
    resolveShader.fuzzGrain = std::array {
        viewport.w * scale / (float) Engine::screenWidth, viewport.h * scale / rows};

    resolveShader.fuzzPhase = (float) Engine::fuzzPhase();

    pass.draw(resolveShader);
}
void View::ensureWorldTarget()
{
    auto scale = backingScale();
    auto bounds = getLocalBounds();
    auto pixels =
        Graphics::Point {std::round(bounds.w * scale), std::round(bounds.h * scale)};

    if (pixels.x < 1.0f || pixels.y < 1.0f)
        return;

    if (worldTarget && pixels.x == targetPixels.x && pixels.y == targetPixels.y)
        return;

    targetPixels = pixels;
    worldTarget.emplace(makeWorldTarget((int) pixels.x, (int) pixels.y));
    resolveShader.world = *worldTarget;
}
View::HudPlacement
    View::placeHudSprite(int slot, const Graphics::Rect& viewport, float rows) const
{
    const auto& sprite = hud[slot];
    const auto& was = previousHud[slot];

    if (sprite.textureId < 0)
        return {};

    auto scaleX = viewport.w / (float) Engine::screenWidth;
    auto scaleY = viewport.h / rows;
    auto at = sprite.at;

    if (was.textureId >= 0)
        at = Engine::lerp(was.at, sprite.at, ticFraction);

    return {{viewport.x + at.x * scaleX,
             viewport.y + at.y * scaleY,
             sprite.size.x * scaleX,
             sprite.size.y * scaleY},
            sprite.flip ? std::array {1.0f, 0.0f} : std::array {0.0f, 1.0f},
            true};
}
void View::drawWeapon(GPU::RenderPass& pass,
                      const Graphics::Rect& bounds,
                      const Graphics::Rect& viewport,
                      float rows)
{
    for (auto i = 0; i < hud.size(); ++i)
    {
        const auto& sprite = hud[i];
        auto placed = placeHudSprite(i, viewport, rows);

        if (!placed.visible || sprite.fuzz)
            continue;

        hudShader.setDestination(bounds, placed.dst);
        hudShader.uRange = placed.uRange;
        hudShader.light = sprite.light;
        hudShader.texture = textureFor(sprite.textureId);

        pass.draw(hudShader);
    }
}
void View::markWeaponFuzz(GPU::RenderPass& pass,
                          const Graphics::Rect& bounds,
                          const Graphics::Rect& viewport,
                          float rows)
{
    for (auto i = 0; i < hud.size(); ++i)
    {
        const auto& sprite = hud[i];
        auto placed = placeHudSprite(i, viewport, rows);

        if (!placed.visible || !sprite.fuzz)
            continue;

        hudFuzzShader.setDestination(bounds, placed.dst);
        hudFuzzShader.uRange = placed.uRange;
        hudFuzzShader.texture = textureFor(sprite.textureId);

        pass.draw(hudFuzzShader);
    }
}
void View::drawAutomap(GPU::RenderPass& pass,
                       const Graphics::Rect& bounds,
                       const Graphics::Rect& viewport)
{
    auto camera = viewCamera();

    // Not `lines`: that is the engine's own linedef array, at :: scope and
    // visible here.
    auto map = Engine::buildAutomap(camera, automap);

    if (map.empty())
        return;

    automapBuffer.update(map.data(), map.size_bytes());

    automapShader.setDestination(bounds, viewport);
    automapShader.frameSize =
        std::array {(float) Engine::automapWidth, (float) Engine::automapHeight};
    automapShader.lineWidth = automapLineWidth;

    // The map window is routinely smaller than the level, so lines run past
    // the edges of it. Vanilla clips each one by hand before rasterizing
    // (clipMline's Cohen-Sutherland outcodes); here the rasterizer does it,
    // and without the bound they would spill over the status bar and the
    // letterbox bars. Scissor state persists for the rest of the pass, hence
    // the clear.
    pass.setScissorRect(inPixels(viewport));

    pass.setPipeline(automapShader.pipeline());
    pass.setVertexBuffer(automapBuffer);
    pass.setUniforms(automapShader);
    automapShader.bindTextures(pass);
    pass.draw((int) map.size(), 0);

    pass.clearScissorRect();
}

Graphics::Rect View::inPixels(const Graphics::Rect& rect) const
{
    auto scale = backingScale();

    return {rect.x * scale, rect.y * scale, rect.w * scale, rect.h * scale};
}

void View::drawOverlay(GPU::RenderPass& pass,
                       const Graphics::Rect& bounds,
                       const Graphics::Rect& dst)
{
    overlayShader.setDestination(bounds, dst);
    pass.draw(overlayShader);
}

void View::drawWipe(GPU::RenderPass& pass,
                    const Graphics::Rect& bounds,
                    const Graphics::Rect& dst)
{
    wipeShader.setDestination(bounds, dst);
    pass.draw(wipeShader);
}

void View::updateOverlay()
{
    overlayVisible = Engine::buildOverlay(overlayPixels);

    if (overlayVisible)
        overlayTexture.update(overlayPixels.data());
}

void View::updateWipe()
{
    wipeVisible = Engine::buildWipe(wipePixels, wipeOffsets);

    if (wipeVisible)
    {
        wipeTexture.update(wipePixels.data());
        wipeOffsetTexture.update(wipeOffsets.data());
    }
}

void View::updatePalette()
{
    Engine::readPalette(paletteData);
    paletteTexture.update(paletteData.data());
}

void View::ensureWorldTextures()
{
    auto count = Engine::textureCount();

    if (count <= 0 || worldTextures.size() == count)
        return;

    worldTextures.clear();
    worldTextures.resize(count);

    auto rows = Vector<std::uint8_t> {};
    rows.resize(256 * Engine::colormapRows);
    Engine::readColormaps(rows);
    colormapTexture.update(rows.data());
}

GPU::Texture& View::textureFor(int id)
{
    auto& slot = worldTextures[id];

    if (!slot.has_value())
    {
        auto info = Engine::textureInfo(id);
        auto width = info.width > 0 ? info.width : 1;
        auto height = info.height > 0 ? info.height : 1;

        auto pixels = Vector<std::uint8_t> {};
        pixels.resize(width * height * (info.masked ? 4 : 1));
        Engine::readTexturePixels(id, pixels);

        slot.emplace(makeWorldTexture(width, height, info.masked, pixels.data()));
    }

    return *slot;
}

void View::syncModifierKeys(const Graphics::ModifierKeys& current)
{
    syncModifierKey(current.shift, modifiers.shift, Doom::Key::Shift);
    syncModifierKey(current.control, modifiers.control, Doom::Key::Ctrl);
    syncModifierKey(current.alt, modifiers.alt, Doom::Key::Alt);
    modifiers = current;
}

void View::keyDown(const Graphics::KeyEvent& event)
{
    if (event.isRepeat)
        return;

    if (event.keyCode == Graphics::KeyCode::F8 && event.modifiers.shift)
    {
        gpuWorld = !gpuWorld;
        return;
    }

    if (event.keyCode == Graphics::KeyCode::F7 && event.modifiers.shift)
    {
        predictAim = !predictAim;
        return;
    }

    if (auto key = toDoomKey(event); key != Doom::Key::Unknown)
        Doom::keyDown(key);
}

void View::keyUp(const Graphics::KeyEvent& event)
{
    if (auto key = toDoomKey(event); key != Doom::Key::Unknown)
        Doom::keyUp(key);
}

void View::mouseDown(const Graphics::MouseEvent& event)
{
    if (!window.isMouseLocked())
    {
        window.setMouseLocked(true);
        return;
    }

    Doom::buttonDown(toDoomButton(event.button));
}

void View::mouseUp(const Graphics::MouseEvent& event)
{
    if (window.isMouseLocked())
        Doom::buttonUp(toDoomButton(event.button));
}

void View::mouseMoved(const Graphics::MouseEvent& event)
{
    aim(event);
}

void View::mouseDragged(const Graphics::MouseEvent& event)
{
    aim(event);
}

void View::aim(const Graphics::MouseEvent& event)
{
    if (!window.isMouseLocked())
        return;

    mouseMovement.x += event.rawDelta.x * mouseSpeed;
    mouseMovement.y += event.rawDelta.y * mouseSpeed;
}
} // namespace PureDoom