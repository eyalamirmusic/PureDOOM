#include "View.h"
#include "Layout.h"

namespace PureDoom
{
// A fresh level or a teleport moves the camera somewhere else entirely, and
// there is nothing to interpolate across that.
static bool jumped(const Engine::Camera& from, const Engine::Camera& to)
{
    constexpr auto limit = 128.0f;

    return std::abs(to.x - from.x) > limit || std::abs(to.y - from.y) > limit
           || std::abs(to.z - from.z) > limit;
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
    prepareQuadShader(hudShader);
    prepareQuadShader(overlayShader);
    prepareQuadShader(wipeShader);
    prepareShader(automapShader);
    prepareShader(worldShader);

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
void View::update(Threads::FrameTime frame_time)
{
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

    camera.x = std::lerp(previousCamera.x, currentCamera.x, ticFraction);
    camera.y = std::lerp(previousCamera.y, currentCamera.y, ticFraction);
    camera.z = std::lerp(previousCamera.z, currentCamera.z, ticFraction);
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

    auto pass = frame.beginPass({Graphics::Color::black()});

    setDarkenRow((float) Engine::darkenRow());

    if (!gpuView)
    {
        drawScreen(pass, bounds, dst, 0.0f, 1.0f);
        frameChanged = false;
        return;
    }

    // The last notch of the menu's screen size takes the status bar away, and
    // the view is then the whole frame rather than the rows above the bar.
    auto rows = Engine::viewRows();
    auto viewport = dst.withHeight(dst.h * worldViewportShare(rows));

    // The engine skips the 3D view entirely while the automap is up, so the
    // two never share the frame.
    if (Engine::automapActive())
        drawAutomap(pass, bounds, viewport);
    else
    {
        drawWorld(pass, bounds, viewport, rows);
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
void View::drawWorld(GPU::RenderPass& pass,
                     const Graphics::Rect& bounds,
                     const Graphics::Rect& viewport,
                     float rows)
{
    // Rebuilt every frame rather than every tic, because the billboards and
    // the sky are built around the camera being drawn from, and that moves
    // with the view between tics rather than with the engine.
    auto camera = viewCamera();
    auto world = Engine::buildGeometry(camera, ticFraction, {geometry, draws});

    if (world.draws.empty())
        return;

    worldBuffer.update(world.vertices.data(), world.vertices.size_bytes());

    worldShader.camX = camera.x;
    worldShader.camY = camera.z;
    worldShader.camZ = -camera.y;
    worldShader.yaw = camera.angle - pi / 2.0f;

    // The projection is built for the view with the status bar up, so a
    // taller view has to widen its vertical field of view by the same
    // proportion - which on a perspective projection is one scale on y, and
    // the viewport already applies one.
    worldShader.ndcScale =
        std::array {viewport.w / bounds.w,
                    viewport.h / bounds.h * (viewRowsWithStatusBar / rows)};

    worldShader.ndcOffset =
        std::array {(viewport.x + viewport.w * 0.5f) / bounds.w * 2.0f - 1.0f,
                    1.0f - (viewport.y + viewport.h * 0.5f) / bounds.h * 2.0f};

    pass.setPipeline(worldShader.pipeline());
    pass.setVertexBuffer(worldBuffer);
    pass.setVertexUniforms(worldShader);
    pass.setFragmentUniforms(worldShader);

    for (const auto& draw: world.draws)
    {
        worldShader.texture = textureFor(draw.textureId);
        worldShader.bindTextures(pass);
        pass.draw(draw.vertexCount, draw.firstVertex);
    }
}
void View::drawWeapon(GPU::RenderPass& pass,
                      const Graphics::Rect& bounds,
                      const Graphics::Rect& viewport,
                      float rows)
{
    auto scaleX = viewport.w / (float) Engine::screenWidth;
    auto scaleY = viewport.h / rows;

    hudShader.viewSize = std::array {bounds.w, bounds.h};

    for (auto i = 0; i < hud.size(); ++i)
    {
        const auto& sprite = hud[i];
        const auto& was = previousHud[i];

        if (sprite.textureId < 0)
            continue;

        // The weapon bobs on the tic like everything else, so it is placed
        // between tics like everything else.
        auto x = sprite.x;
        auto y = sprite.y;

        if (was.textureId >= 0)
        {
            x = std::lerp(was.x, sprite.x, ticFraction);
            y = std::lerp(was.y, sprite.y, ticFraction);
        }

        hudShader.dstOrigin =
            std::array {viewport.x + x * scaleX, viewport.y + y * scaleY};
        hudShader.dstSize =
            std::array {sprite.width * scaleX, sprite.height * scaleY};
        hudShader.uRange =
            sprite.flip ? std::array {1.0f, 0.0f} : std::array {0.0f, 1.0f};
        hudShader.light = sprite.light;
        hudShader.texture = textureFor(sprite.textureId);

        pass.draw(hudShader);
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
    pass.setVertexUniforms(automapShader);
    pass.setFragmentUniforms(automapShader);
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