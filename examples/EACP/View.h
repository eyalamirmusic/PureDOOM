#pragma once

#include "AutomapShader.h"
#include "Common.h"
#include "HudShader.h"
#include "Input.h"
#include "Layout.h"
#include "OverlayShader.h"
#include "ScreenShader.h"
#include "Textures.h"
#include "WorldShader.h"

#include <optional>

namespace PureDoom
{
// The game's window contents: it ticks the engine once per display refresh,
// then draws the frame either as DOOM's own software image or as GPU world
// geometry with the software status bar composited beneath it, and feeds
// keyboard and mouse back into the engine.
struct View final : GPU::GPUView
{
    View()
    {
        setSampleCount(1);
        setDepth(true);
        shader.setVertices(unitQuad);
        shader.prepare(sampleCount(), true);
        shader.screenIndices = framebuffer;
        shader.palette = paletteTexture;

        worldShader.prepare(sampleCount(), true);
        worldShader.colormap = colormapTexture;
        worldShader.palette = paletteTexture;

        hudShader.setVertices(unitQuad);
        hudShader.prepare(sampleCount(), true);
        hudShader.colormap = colormapTexture;
        hudShader.palette = paletteTexture;

        automapShader.prepare(sampleCount(), true);
        automapShader.colormap = colormapTexture;
        automapShader.palette = paletteTexture;

        overlayShader.setVertices(unitQuad);
        overlayShader.prepare(sampleCount(), true);
        overlayShader.overlay = overlayTexture;
        overlayShader.colormap = colormapTexture;
        overlayShader.palette = paletteTexture;

        geometry.getVector().resize(maxVertices);
        draws.getVector().resize(maxDraws);
        automap.getVector().resize(maxAutomapVertices);
        overlayPixels.getVector().resize(overlayBytes);

        for (auto& sprite: hud)
            sprite.textureId = -1;

        previousHud = hud;

        setHandlesMouseEvents(true);
        setGrabsFocusOnMouseDown(true);
        setContinuous(true);
    }

    void update(Threads::FrameTime) override
    {
        if (window != nullptr)
        {
            if (window->isCommandPressed())
                window->setMouseLocked(false);

            syncModifierKeys(window->getModifiers());
        }

        // One reading of the engine's clock decides both things the frame needs
        // from it: whether a tic is due, and how far into the tic the frame
        // sits. Reading it twice lets a tic boundary fall between the two, and
        // the frame is then drawn a whole tic in the past — a jump backwards,
        // and a jump forwards to recover. That was the stutter.
        auto now = eacpDoomTicTime();
        auto tic = (std::int64_t) now;

        alpha = (float) (now - (double) tic);

        // The engine's state only moves on a tic, 35 times a second, so it is
        // stepped when its own clock says there is a tic to run and left alone
        // on the refreshes in between. A screen wipe animates per frame instead
        // and is stepped every time.
        if (tic != lastTic || eacpDoomIsWiping())
        {
            lastTic = tic;
            flushMouse();

            previousCamera = currentCamera;
            previousHud = hud;

            // Taken before the tic runs: this is where everything *was*.
            eacpDoomSnapshotTic();

            doom_update();

            currentCamera = eacpDoomGetCamera();
            eacpDoomGetHudSprites(hud.data());

            // A fresh level or a teleport moves the camera somewhere else
            // entirely; there is nothing to interpolate across that.
            if (!hasCamera || jumped(previousCamera, currentCamera))
            {
                previousCamera = currentCamera;
                previousHud = hud;
            }

            hasCamera = true;
            frameChanged = true;
        }
    }

    static bool jumped(const EacpDoomCamera& from, const EacpDoomCamera& to)
    {
        constexpr auto limit = 128.0f;

        return std::abs(to.x - from.x) > limit || std::abs(to.y - from.y) > limit
               || std::abs(to.z - from.z) > limit;
    }

    // Where the camera is *now*, rather than where it was when the last tic
    // ran. The engine moves the player 35 times a second while the display
    // refreshes two to four times as often, so a view taken straight from the
    // engine sits still for two or three frames and then jumps — which reads as
    // lag however fast the frames arrive.
    //
    // The position is interpolated across the tic it is part-way through; the
    // heading is not, and viewAngle() says why.
    EacpDoomCamera viewCamera() const
    {
        auto camera = currentCamera;
        auto alpha = ticAlpha();

        camera.x = mix(previousCamera.x, currentCamera.x, alpha);
        camera.y = mix(previousCamera.y, currentCamera.y, alpha);
        camera.z = mix(previousCamera.z, currentCamera.z, alpha);
        camera.angle = viewAngle(alpha);

        return camera;
    }

    // The heading the frame is drawn from.
    //
    // Interpolating it, as the position is, costs a tic of lag on the one thing
    // the hand is holding: the aim spends every frame catching up to where the
    // mouse already was. So the tic's turn is split by where it came from. What
    // the *keyboard* turned is interpolated — a held key turns at a steady rate,
    // and interpolating is what makes that read as smooth. What the *mouse*
    // turned is applied at once, and the view then runs on past it by the
    // movement the engine has not been handed yet, which is the turn it is about
    // to make anyway. The aim therefore follows the hand every refresh, and when
    // the tic lands, the engine's angle arrives exactly where the view already
    // was, so nothing jumps.
    //
    // This is what GZDoom does — R_InterpolateView gives the local player's yaw
    // as `curYaw + LocalViewAngle` and never interpolates it — and it is why its
    // mouse feels stuck to the hand.
    //
    // Running ahead is safe because the movement it runs ahead on is the
    // *accumulated* mouse, not the last delta: the per-sample raggedness a mouse
    // reports integrates away. Against a deliberately ragged sweep it measured
    // not merely no worse but far steadier than interpolating (frame-to-frame
    // wobble 0.3ms against 10.2ms), because interpolation quantises the aim to
    // the tic and then has to swing between the results.
    //
    // Shift+F7 drops back to plain interpolation to compare.
    float viewAngle(float alpha) const
    {
        auto turn = shortestTurn(previousCamera.angle, currentCamera.angle);

        if (!predictAim)
            return previousCamera.angle + turn * alpha;

        auto keyboardTurn = turn - appliedTurn;

        return previousCamera.angle + keyboardTurn * alpha + appliedTurn
               + pendingTurn();
    }

    // The way round the circle that is actually shorter.
    static float shortestTurn(float from, float to)
    {
        auto turn = to - from;

        while (turn > pi)
            turn -= 2.0f * pi;

        while (turn < -pi)
            turn += 2.0f * pi;

        return turn;
    }

    // How far the engine's clock had moved into the tic being drawn, as read
    // once at the top of the frame — the same reading that decided whether the
    // tic ran, so the two can never disagree.
    float ticAlpha() const
    {
        return alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);
    }

    static float mix(float from, float to, float amount)
    {
        return from + (to - from) * amount;
    }

    // DOOM reads the mouse once a tic, and the last event it saw wins:
    // G_Responder assigns the movement rather than adding to it. Handing it one
    // event per platform mouse move - which arrive several times per tic -
    // would therefore throw all but the last of them away, and the aim would
    // crawl. Accumulate the movement here and give the engine the whole of it,
    // once per tic, which is what it expects. It also keeps the engine's
    // 64-event queue from filling with mouse motion and dropping keystrokes.

    float pendingTurn() const { return turnFor(mouseMovement.x); }

    // The heading change the engine makes from a given amount of mouse
    // movement: it scales by its sensitivity, then subtracts eight times that
    // from its heading, in units where a full circle is 2^32.
    static float turnFor(float movement)
    {
        auto sensitivity = (float) (eacpDoomMouseSensitivity() + 5) / 10.0f;

        return -movement * sensitivity * pi / 4096.0f;
    }

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
        // The game's state moves 35 times a second and the display refreshes
        // two to four times as often, so everything derived from that state -
        // the software frame, the palette, the world's geometry - is rebuilt
        // only when a tic has actually run.
        if (frameChanged)
        {
            framebuffer.update(doom_get_framebuffer(1));
            updatePalette();
            updateOverlay();
        }

        ensureWorldTextures();

        auto bounds = getLocalBounds();
        auto dst = letterboxedDisplayRect(bounds);
        auto gpuView = gpuWorld && eacpDoomViewActive() && worldTextures.size() > 0;

        auto pass = frame.beginPass({Graphics::Color::black()});

        // What the menu darkens behind itself. The status bar needs none of it:
        // the engine darkens its own frame, which is where the strip comes from.
        auto darkenRow = (float) eacpDoomDarkenRow();

        worldShader.darkenRow = darkenRow;
        hudShader.darkenRow = darkenRow;
        automapShader.darkenRow = darkenRow;
        overlayShader.darkenRow = darkenRow;

        if (gpuView)
        {
            auto viewport = dst.withHeight(dst.h * worldViewportShare);

            // The engine skips the 3D view entirely while the automap is up, so
            // the two never share the frame.
            if (eacpDoomAutomapActive())
                drawAutomap(pass, bounds, viewport);
            else
            {
                drawWorld(pass, bounds, viewport);
                drawWeapon(pass, bounds, viewport);
            }

            auto strip = Graphics::Rect {dst.x,
                                         dst.y + dst.h * worldViewportShare,
                                         dst.w,
                                         dst.h * (1.0f - worldViewportShare)};
            drawScreen(pass, bounds, strip, viewRows / (float) doomHeight, 1.0f);

            // The menu, the messages and the PAUSE graphic are already in the
            // software frame whenever that is what is on the screen; over the GPU
            // view they have to be put back.
            if (overlayVisible)
                drawOverlay(pass, bounds, dst);
        }
        else
            drawScreen(pass, bounds, dst, 0.0f, 1.0f);

        frameChanged = false;
    }

    void drawScreen(GPU::RenderPass& pass,
                    const Graphics::Rect& bounds,
                    const Graphics::Rect& dst,
                    float uvTop,
                    float uvBottom)
    {
        shader.viewSize = std::array {bounds.w, bounds.h};
        shader.dstOrigin = std::array {dst.x, dst.y};
        shader.dstSize = std::array {dst.w, dst.h};
        shader.uvY = std::array {uvTop, uvBottom};
        pass.draw(shader);
    }

    void drawWorld(GPU::RenderPass& pass,
                   const Graphics::Rect& bounds,
                   const Graphics::Rect& viewport)
    {
        auto camera = viewCamera();
        auto vertexCount = 0;

        // Rebuilt every frame rather than every tic, because the billboards and
        // the sky are built around the camera being drawn from, and that moves
        // with the view between tics — not with the engine.
        auto drawCount = eacpDoomBuildGeometry(&camera,
                                               ticAlpha(),
                                               geometry.data(),
                                               maxVertices,
                                               draws.data(),
                                               maxDraws,
                                               &vertexCount);

        if (drawCount <= 0 || vertexCount <= 0)
            return;

        worldBuffer.update(geometry.data(),
                           (std::size_t) vertexCount * sizeof(EacpDoomVertex));

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
            const auto& draw = draws[(std::size_t) i];

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
        auto alpha = ticAlpha();

        hudShader.viewSize = std::array {bounds.w, bounds.h};

        for (auto i = std::size_t {0}; i < hud.size(); ++i)
        {
            const auto& sprite = hud[i];
            const auto& was = previousHud[i];

            if (sprite.textureId < 0)
                continue;

            // The weapon bobs on the tic like everything else, so it is placed
            // between tics like everything else — otherwise it would jitter
            // against a world that glides.
            auto x = sprite.x;
            auto y = sprite.y;

            if (was.textureId >= 0)
            {
                x = mix(was.x, sprite.x, alpha);
                y = mix(was.y, sprite.y, alpha);
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

    // The map, drawn at the window's resolution rather than at DOOM's. It is
    // rebuilt every frame, not every tic, because it is centred on the view, and
    // between tics the view is somewhere the engine has not been yet.
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
                             (std::size_t) vertexCount
                                 * sizeof(EacpDoomAutomapVertex));

        automapShader.viewSize = std::array {bounds.w, bounds.h};
        automapShader.dstOrigin = std::array {viewport.x, viewport.y};
        automapShader.dstSize = std::array {viewport.w, viewport.h};
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
        overlayShader.viewSize = std::array {bounds.w, bounds.h};
        overlayShader.dstOrigin = std::array {dst.x, dst.y};
        overlayShader.dstSize = std::array {dst.w, dst.h};
        pass.draw(overlayShader);
    }

    void updateOverlay()
    {
        overlayVisible = eacpDoomBuildOverlay(overlayPixels.data()) != 0;

        if (overlayVisible)
            overlayTexture.update(overlayPixels.data());
    }

    // The WAD's graphics are loaded once, so the slots are sized once; the
    // colormap comes along with them.
    void ensureWorldTextures()
    {
        auto count = eacpDoomGetTextureCount();

        if (count <= 0 || (int) worldTextures.size() == count)
            return;

        worldTextures.getVector().clear();
        worldTextures.getVector().resize((std::size_t) count);

        auto rows = Vector<std::uint8_t> {};
        rows.getVector().assign(256 * EACP_DOOM_COLORMAP_ROWS, 0);
        eacpDoomGetColormaps(rows.data());
        colormapTexture.update(rows.data());
    }

    // Uploaded the first time something is drawn with it: a WAD holds well over
    // a thousand sprite lumps, and a level shows a small fraction of them.
    GPU::Texture& textureFor(int id)
    {
        auto& slot = worldTextures[(std::size_t) id];

        if (!slot.has_value())
        {
            auto info = eacpDoomGetTextureInfo(id);
            auto width = info.width > 0 ? info.width : 1;
            auto height = info.height > 0 ? info.height : 1;
            auto bytes = (std::size_t) (width * height) * (info.masked ? 4 : 1);

            auto pixels = Vector<std::uint8_t> {};
            pixels.getVector().assign(bytes, 0);
            eacpDoomGetTexturePixels(id, pixels.data());

            slot.emplace(
                makeWorldTexture(width, height, info.masked != 0, pixels.data()));
        }

        return *slot;
    }

    void updatePalette()
    {
        for (auto i = 0; i < 256; ++i)
        {
            paletteData[(std::size_t) i * 4 + 0] = screen_palette[i * 3 + 0];
            paletteData[(std::size_t) i * 4 + 1] = screen_palette[i * 3 + 1];
            paletteData[(std::size_t) i * 4 + 2] = screen_palette[i * 3 + 2];
            paletteData[(std::size_t) i * 4 + 3] = 255;
        }

        paletteTexture.update(paletteData.data());
    }

    // DOOM binds Ctrl/Shift/Alt as ordinary keys (fire/run/strafe), but eacp
    // reports them only as modifier state, never as key events. Diff the
    // polled state once per frame into synthetic down/up events.
    void syncModifierKeys(const Graphics::ModifierKeys& current)
    {
        syncModifierKey(current.shift, modifiers.shift, DOOM_KEY_SHIFT);
        syncModifierKey(current.control, modifiers.control, DOOM_KEY_CTRL);
        syncModifierKey(current.alt, modifiers.alt, DOOM_KEY_ALT);
        modifiers = current;
    }

    static void syncModifierKey(bool pressed, bool wasPressed, doom_key_t key)
    {
        if (pressed == wasPressed)
            return;

        if (pressed)
            doom_key_down(key);
        else
            doom_key_up(key);
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
        if (window == nullptr)
            return;

        if (!window->isMouseLocked())
        {
            window->setMouseLocked(true);
            return;
        }

        doom_button_down(toDoomButton(event.button));
    }

    void mouseUp(const Graphics::MouseEvent& event) override
    {
        if (window != nullptr && window->isMouseLocked())
            doom_button_up(toDoomButton(event.button));
    }

    void mouseMoved(const Graphics::MouseEvent& event) override { aim(event); }

    void mouseDragged(const Graphics::MouseEvent& event) override { aim(event); }

    void aim(const Graphics::MouseEvent& event)
    {
        // The device's own movement, not the pointer's: the system's
        // acceleration curve is there to help a cursor reach a target, and
        // through it the same flick of the hand turns the player a different
        // amount depending how fast it was made.
        if (window != nullptr && window->isMouseLocked())
        {
            mouseMovement.x += event.rawDelta.x * mouseSpeed;
            mouseMovement.y += event.rawDelta.y * mouseSpeed;
        }
    }

    ScreenShader shader;
    WorldShader worldShader;
    HudShader hudShader;
    AutomapShader automapShader;
    OverlayShader overlayShader;
    GPU::Texture framebuffer = makeIndexTexture();
    GPU::Texture paletteTexture = makePaletteTexture();
    GPU::Texture colormapTexture = makeColormapTexture();
    GPU::Texture overlayTexture = makeOverlayTexture();
    GPU::Buffer worldBuffer {GPU::Device::shared(),
                             nullptr,
                             (std::size_t) maxVertices * sizeof(EacpDoomVertex),
                             GPU::BufferUsage::Vertex};
    GPU::Buffer automapBuffer {GPU::Device::shared(),
                               nullptr,
                               (std::size_t) maxAutomapVertices
                                   * sizeof(EacpDoomAutomapVertex),
                               GPU::BufferUsage::Vertex};
    Vector<std::optional<GPU::Texture>> worldTextures;
    Vector<EacpDoomVertex> geometry;
    Vector<EacpDoomDraw> draws;
    Vector<EacpDoomAutomapVertex> automap;
    Vector<std::uint8_t> overlayPixels;
    bool overlayVisible = false;
    std::array<std::uint8_t, 256 * 4> paletteData {};

    using HudSprites = std::array<EacpDoomHudSprite, EACP_DOOM_HUD_SPRITES>;
    HudSprites hud {};
    HudSprites previousHud {};

    // Shift+F8 flips between the GPU world renderer and the software frame.
    bool gpuWorld = true;

    // Whether a tic has run since the last frame was drawn, and so whether
    // anything the frame is built from has moved.
    bool frameChanged = true;
    std::int64_t lastTic = -1;
    float alpha = 0.0f;

    // Mouse movement gathered since the last tic, in DOOM's units, and the turn
    // the engine made from the movement handed to it at that tic.
    Graphics::Point mouseMovement;
    float appliedTurn = 0.0f;

    // Shift+F7 drops the aim back to plain interpolation — a tic of lag, and
    // nothing running ahead — to compare the two.
    bool predictAim = true;

    // The camera at the last two tics, and when the latest one ran, so the view
    // can be placed between them.
    EacpDoomCamera previousCamera {};
    EacpDoomCamera currentCamera {};
    bool hasCamera = false;

    Graphics::Window* window = nullptr;
    Graphics::ModifierKeys modifiers;
};
} // namespace PureDoom
