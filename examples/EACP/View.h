#pragma once

#include "AutomapShader.h"
#include "HudShader.h"
#include "Input.h"
#include "Layout.h"
#include "OverlayShader.h"
#include "ScreenShader.h"
#include "Textures.h"
#include "WipeShader.h"
#include "WorldShader.h"

#include <algorithm>
#include <optional>

namespace PureDoom
{
// A fresh level or a teleport moves the camera somewhere else entirely, and
// there is nothing to interpolate across that.
inline bool jumped(const EacpDoomCamera& from, const EacpDoomCamera& to)
{
    constexpr auto limit = 128.0f;

    return std::abs(to.x - from.x) > limit || std::abs(to.y - from.y) > limit
           || std::abs(to.z - from.z) > limit;
}

inline float shortestTurn(float from, float to)
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
inline float turnFor(float movement)
{
    auto sensitivity = (float) (eacpDoomMouseSensitivity() + 5) / 10.0f;

    return -movement * sensitivity * pi / 4096.0f;
}

inline void syncModifierKey(bool pressed, bool wasPressed, doom_key_t key)
{
    if (pressed == wasPressed)
        return;

    if (pressed)
        doom_key_down(key);
    else
        doom_key_up(key);
}

// The game's window contents: it steps the engine when the engine's own clock
// says a tic is due, draws the frame either as DOOM's own software image or as
// GPU world geometry with the software status bar beneath it, and feeds the
// keyboard and the mouse back in.
struct View final : GPU::GPUView
{
    explicit View(Graphics::Window& windowToUse)
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
        wipePixels.resize(screenPixels);
        paletteData.resize(256 * 4);

        hud.resize(EACP_DOOM_HUD_SPRITES);

        for (auto& sprite: hud)
            sprite.textureId = -1;

        previousHud = hud;
    }

    void prepareShader(DoomShader& shader)
    {
        shader.prepare(sampleCount(), true);
        shader.colormap = colormapTexture;
        shader.palette = paletteTexture;
    }

    void prepareQuadShader(ScreenQuadShader& shader)
    {
        shader.setVertices(unitQuad);
        prepareShader(shader);
    }

    void update(Threads::FrameTime) override
    {
        if (window.isCommandPressed())
            window.setMouseLocked(false);

        syncModifierKeys(window.getModifiers());

        // One reading of the engine's clock answers both of the questions the
        // frame has for it: whether a tic is due, and how far into the tic the
        // frame sits. Reading it twice lets a tic boundary fall between the two,
        // and the frame is then drawn a whole tic in the past.
        auto now = eacpDoomTicTime();
        auto tic = (std::int64_t) now;

        ticFraction = std::clamp((float) (now - (double) tic), 0.0f, 1.0f);

        // The engine's state only moves on a tic, 35 times a second, so it is
        // left alone on the refreshes in between. A melt animates per frame.
        if (tic == lastTic && !eacpDoomIsWiping())
            return;

        lastTic = tic;
        flushMouse();

        previousCamera = currentCamera;
        previousHud = hud;

        eacpDoomSnapshotTic();
        doom_update();

        // The engine reveals a wall on the map by drawing it, and draws nothing
        // while the map is up - so vanilla's map stops filling in the moment it
        // is looked at. This keeps it filling in.
        eacpDoomRevealAutomap();

        currentCamera = eacpDoomGetCamera();
        eacpDoomGetHudSprites(hud.data());

        if (!hasCamera || jumped(previousCamera, currentCamera))
        {
            previousCamera = currentCamera;
            previousHud = hud;
        }

        hasCamera = true;
        frameChanged = true;
    }

    // Where the camera is now, rather than where it was when the last tic ran:
    // the engine moves the player 35 times a second while the display refreshes
    // two to four times as often, so a view taken straight from the engine sits
    // still for two or three frames and then jumps.
    EacpDoomCamera viewCamera() const
    {
        auto camera = currentCamera;

        camera.x = std::lerp(previousCamera.x, currentCamera.x, ticFraction);
        camera.y = std::lerp(previousCamera.y, currentCamera.y, ticFraction);
        camera.z = std::lerp(previousCamera.z, currentCamera.z, ticFraction);
        camera.angle = viewAngle();

        return camera;
    }

    // The position is interpolated across the tic it is part-way through; the
    // heading is not. Interpolating the heading would cost a whole tic of lag on
    // the one thing the hand is holding, so the tic's turn is split by where it
    // came from: what the keyboard turned is interpolated, because a held key
    // turns at a steady rate, while what the mouse turned is applied at once and
    // the view then runs on past it by the movement the engine has not been
    // handed yet - which is the turn it is about to make anyway. The aim
    // therefore follows the hand every refresh, and when the tic lands the
    // engine's angle arrives exactly where the view already was.
    //
    // Shift+F7 drops back to plain interpolation to compare.
    float viewAngle() const
    {
        auto turn = shortestTurn(previousCamera.angle, currentCamera.angle);

        if (!predictAim)
            return previousCamera.angle + turn * ticFraction;

        auto keyboardTurn = turn - appliedTurn;

        return previousCamera.angle + keyboardTurn * ticFraction + appliedTurn
               + pendingTurn();
    }

    float pendingTurn() const { return turnFor(mouseMovement.x); }

    // DOOM reads the mouse once a tic and the last event it saw wins:
    // G_Responder assigns the movement rather than adding to it. Handing it one
    // event per platform mouse move - which arrive several times per tic - would
    // throw all but the last of them away, so the whole of the movement goes over
    // at once, as vanilla's I_StartTic does.
    void flushMouse()
    {
        auto x = (int) mouseMovement.x;
        auto y = (int) mouseMovement.y;

        appliedTurn = turnFor((float) x);

        if (x == 0 && y == 0)
            return;

        doom_mouse_move(x, y);

        // Keep the fraction, so slow movement accumulates instead of rounding
        // away to nothing.
        mouseMovement.x -= (float) x;
        mouseMovement.y -= (float) y;
    }

    void render(GPU::Frame& frame) override
    {
        // Everything derived from the engine's state is rebuilt only when a tic
        // has actually run.
        if (frameChanged)
        {
            framebuffer.update(doom_get_framebuffer(1));
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
        auto gpuView = gpuWorld && eacpDoomViewActive() && !worldTextures.empty()
                       && (!eacpDoomIsWiping() || wipeVisible);

        auto pass = frame.beginPass({Graphics::Color::black()});

        setDarkenRow((float) eacpDoomDarkenRow());

        if (!gpuView)
        {
            drawScreen(pass, bounds, dst, 0.0f, 1.0f);
            frameChanged = false;
            return;
        }

        auto viewport = dst.withHeight(dst.h * worldViewportShare);

        // The engine skips the 3D view entirely while the automap is up, so the
        // two never share the frame.
        if (eacpDoomAutomapActive())
            drawAutomap(pass, bounds, viewport);
        else
        {
            drawWorld(pass, bounds, viewport);
            drawWeapon(pass, bounds, viewport);
        }

        drawScreen(pass, bounds, statusBarRect(dst), viewRows / doomHeight, 1.0f);

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

    // What the menu darkens behind itself. The status bar needs none of it: the
    // engine darkens its own frame, which is where the strip comes from.
    void setDarkenRow(float row)
    {
        worldShader.darkenRow = row;
        hudShader.darkenRow = row;
        automapShader.darkenRow = row;
        overlayShader.darkenRow = row;
    }

    static Graphics::Rect statusBarRect(const Graphics::Rect& dst)
    {
        return {dst.x,
                dst.y + dst.h * worldViewportShare,
                dst.w,
                dst.h * (1.0f - worldViewportShare)};
    }

    void drawScreen(GPU::RenderPass& pass,
                    const Graphics::Rect& bounds,
                    const Graphics::Rect& dst,
                    float uvTop,
                    float uvBottom)
    {
        screenShader.setDestination(bounds, dst);
        screenShader.uvY = std::array {uvTop, uvBottom};
        pass.draw(screenShader);
    }

    void drawWorld(GPU::RenderPass& pass,
                   const Graphics::Rect& bounds,
                   const Graphics::Rect& viewport)
    {
        // Rebuilt every frame rather than every tic, because the billboards and
        // the sky are built around the camera being drawn from, and that moves
        // with the view between tics rather than with the engine.
        auto camera = viewCamera();
        auto vertexCount = 0;
        auto drawCount = eacpDoomBuildGeometry(&camera,
                                               ticFraction,
                                               geometry.data(),
                                               maxVertices,
                                               draws.data(),
                                               maxDraws,
                                               &vertexCount);

        if (drawCount <= 0 || vertexCount <= 0)
            return;

        worldBuffer.update(geometry.data(), vertexCount * sizeof(EacpDoomVertex));

        worldShader.camX = camera.x;
        worldShader.camY = camera.z;
        worldShader.camZ = -camera.y;
        worldShader.yaw = camera.angle - pi / 2.0f;

        worldShader.ndcScale =
            std::array {viewport.w / bounds.w, viewport.h / bounds.h};
        worldShader.ndcOffset =
            std::array {(viewport.x + viewport.w * 0.5f) / bounds.w * 2.0f - 1.0f,
                        1.0f - (viewport.y + viewport.h * 0.5f) / bounds.h * 2.0f};

        pass.setPipeline(worldShader.pipeline());
        pass.setVertexBuffer(worldBuffer);
        pass.setVertexUniforms(worldShader);
        pass.setFragmentUniforms(worldShader);

        for (auto i = 0; i < drawCount; ++i)
        {
            const auto& draw = draws[i];

            worldShader.texture = textureFor(draw.textureId);
            worldShader.bindTextures(pass);
            pass.draw(draw.vertexCount, draw.firstVertex);
        }
    }

    void drawWeapon(GPU::RenderPass& pass,
                    const Graphics::Rect& bounds,
                    const Graphics::Rect& viewport)
    {
        auto scaleX = viewport.w / (float) doomWidth;
        auto scaleY = viewport.h / viewRows;

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

    // Rebuilt every frame rather than every tic, because it is centred on the
    // view, and between tics the view is somewhere the engine has not been yet.
    void drawAutomap(GPU::RenderPass& pass,
                     const Graphics::Rect& bounds,
                     const Graphics::Rect& viewport)
    {
        auto camera = viewCamera();
        auto vertexCount =
            eacpDoomBuildAutomap(&camera, automap.data(), maxAutomapVertices);

        if (vertexCount <= 0)
            return;

        automapBuffer.update(automap.data(),
                             vertexCount * sizeof(EacpDoomAutomapVertex));

        automapShader.setDestination(bounds, viewport);
        automapShader.frameSize = std::array {automapWidth, automapHeight};
        automapShader.lineWidth = automapLineWidth;

        pass.setPipeline(automapShader.pipeline());
        pass.setVertexBuffer(automapBuffer);
        pass.setVertexUniforms(automapShader);
        pass.setFragmentUniforms(automapShader);
        automapShader.bindTextures(pass);
        pass.draw(vertexCount, 0);
    }

    void drawOverlay(GPU::RenderPass& pass,
                     const Graphics::Rect& bounds,
                     const Graphics::Rect& dst)
    {
        overlayShader.setDestination(bounds, dst);
        pass.draw(overlayShader);
    }

    void drawWipe(GPU::RenderPass& pass,
                  const Graphics::Rect& bounds,
                  const Graphics::Rect& dst)
    {
        wipeShader.setDestination(bounds, dst);
        pass.draw(wipeShader);
    }

    void updateOverlay()
    {
        overlayVisible = eacpDoomBuildOverlay(overlayPixels.data()) != 0;

        if (overlayVisible)
            overlayTexture.update(overlayPixels.data());
    }

    void updateWipe()
    {
        wipeVisible = eacpDoomBuildWipe(wipePixels.data(), wipeOffsets.data()) != 0;

        if (wipeVisible)
        {
            wipeTexture.update(wipePixels.data());
            wipeOffsetTexture.update(wipeOffsets.data());
        }
    }

    void updatePalette()
    {
        for (auto i = 0; i < 256; ++i)
        {
            paletteData[i * 4 + 0] = screen_palette[i * 3 + 0];
            paletteData[i * 4 + 1] = screen_palette[i * 3 + 1];
            paletteData[i * 4 + 2] = screen_palette[i * 3 + 2];
            paletteData[i * 4 + 3] = 255;
        }

        paletteTexture.update(paletteData.data());
    }

    // The WAD's graphics are loaded once, so the slots are sized once; the
    // colormap comes along with them.
    void ensureWorldTextures()
    {
        auto count = eacpDoomGetTextureCount();

        if (count <= 0 || worldTextures.size() == count)
            return;

        worldTextures.clear();
        worldTextures.resize(count);

        auto rows = Vector<std::uint8_t> {};
        rows.resize(256 * EACP_DOOM_COLORMAP_ROWS);
        eacpDoomGetColormaps(rows.data());
        colormapTexture.update(rows.data());
    }

    // Uploaded the first time something is drawn with it: a WAD holds well over a
    // thousand sprite lumps, and a level shows a small fraction of them.
    GPU::Texture& textureFor(int id)
    {
        auto& slot = worldTextures[id];

        if (!slot.has_value())
        {
            auto info = eacpDoomGetTextureInfo(id);
            auto width = info.width > 0 ? info.width : 1;
            auto height = info.height > 0 ? info.height : 1;

            auto pixels = Vector<std::uint8_t> {};
            pixels.resize(width * height * (info.masked ? 4 : 1));
            eacpDoomGetTexturePixels(id, pixels.data());

            slot.emplace(
                makeWorldTexture(width, height, info.masked != 0, pixels.data()));
        }

        return *slot;
    }

    // DOOM binds Ctrl/Shift/Alt as ordinary keys (fire/run/strafe), but eacp
    // reports them only as modifier state, never as key events (see the gap log).
    void syncModifierKeys(const Graphics::ModifierKeys& current)
    {
        syncModifierKey(current.shift, modifiers.shift, DOOM_KEY_SHIFT);
        syncModifierKey(current.control, modifiers.control, DOOM_KEY_CTRL);
        syncModifierKey(current.alt, modifiers.alt, DOOM_KEY_ALT);
        modifiers = current;
    }

    void keyDown(const Graphics::KeyEvent& event) override
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

        if (auto key = toDoomKey(event); key != DOOM_KEY_UNKNOWN)
            doom_key_down(key);
    }

    void keyUp(const Graphics::KeyEvent& event) override
    {
        if (auto key = toDoomKey(event); key != DOOM_KEY_UNKNOWN)
            doom_key_up(key);
    }

    void mouseDown(const Graphics::MouseEvent& event) override
    {
        if (!window.isMouseLocked())
        {
            window.setMouseLocked(true);
            return;
        }

        doom_button_down(toDoomButton(event.button));
    }

    void mouseUp(const Graphics::MouseEvent& event) override
    {
        if (window.isMouseLocked())
            doom_button_up(toDoomButton(event.button));
    }

    void mouseMoved(const Graphics::MouseEvent& event) override { aim(event); }

    void mouseDragged(const Graphics::MouseEvent& event) override { aim(event); }

    // The device's own movement, not the pointer's: the system's acceleration
    // curve is there to help a cursor reach a target, and through it the same
    // flick of the hand turns the player a different amount depending how fast it
    // was made.
    void aim(const Graphics::MouseEvent& event)
    {
        if (!window.isMouseLocked())
            return;

        mouseMovement.x += event.rawDelta.x * mouseSpeed;
        mouseMovement.y += event.rawDelta.y * mouseSpeed;
    }

    static constexpr float mouseSpeed = 4.0f;

    ScreenShader screenShader;
    WorldShader worldShader;
    HudShader hudShader;
    AutomapShader automapShader;
    OverlayShader overlayShader;
    WipeShader wipeShader;

    GPU::Texture framebuffer = makeIndexTexture();
    GPU::Texture paletteTexture = makePaletteTexture();
    GPU::Texture colormapTexture = makeColormapTexture();
    GPU::Texture overlayTexture = makeOverlayTexture();
    GPU::Texture wipeTexture = makeIndexTexture();
    GPU::Texture wipeOffsetTexture = makeWipeOffsetTexture();

    GPU::Buffer worldBuffer {GPU::Device::shared(),
                             nullptr,
                             maxVertices * sizeof(EacpDoomVertex),
                             GPU::BufferUsage::Vertex};
    GPU::Buffer automapBuffer {GPU::Device::shared(),
                               nullptr,
                               maxAutomapVertices * sizeof(EacpDoomAutomapVertex),
                               GPU::BufferUsage::Vertex};

    Vector<std::optional<GPU::Texture>> worldTextures;
    Vector<EacpDoomVertex> geometry;
    Vector<EacpDoomDraw> draws;
    Vector<EacpDoomAutomapVertex> automap;
    Vector<std::uint8_t> overlayPixels;
    Vector<std::uint8_t> wipePixels;
    Vector<std::uint8_t> paletteData;
    std::array<std::uint8_t, EACP_DOOM_WIPE_COLUMNS> wipeOffsets {};

    Vector<EacpDoomHudSprite> hud;
    Vector<EacpDoomHudSprite> previousHud;

    bool overlayVisible = false;
    bool wipeVisible = false;

    // Shift+F8 flips between the GPU world renderer and the software frame.
    bool gpuWorld = true;

    // Whether a tic has run since the last frame was drawn, and so whether
    // anything the frame is built from has moved.
    bool frameChanged = true;
    std::int64_t lastTic = -1;

    // How far the engine's clock has moved into the tic being drawn.
    float ticFraction = 0.0f;

    // Mouse movement gathered since the last tic, in DOOM's units, and the turn
    // the engine made from the movement handed to it at that tic.
    Graphics::Point mouseMovement;
    float appliedTurn = 0.0f;
    bool predictAim = true;

    EacpDoomCamera previousCamera {};
    EacpDoomCamera currentCamera {};
    bool hasCamera = false;

    Graphics::Window& window;
    Graphics::ModifierKeys modifiers;
};
} // namespace PureDoom
