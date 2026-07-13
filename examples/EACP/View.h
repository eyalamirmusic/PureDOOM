#pragma once

#include "Common.h"
#include "HudShader.h"
#include "Input.h"
#include "Layout.h"
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

        geometry.getVector().resize(maxVertices);
        draws.getVector().resize(maxDraws);
        hudSprites.getVector().resize(maxHudSprites);

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

        doom_update();
    }

    void render(GPU::Frame& frame) override
    {
        framebuffer.update(doom_get_framebuffer(1));
        updatePalette();
        ensureWorldTextures();

        auto bounds = getLocalBounds();
        auto dst = letterboxedDisplayRect(bounds);

        auto pass = frame.beginPass({Graphics::Color::black()});

        if (gpuWorld && eacpDoomWorldVisible() && worldTextures.size() > 0)
        {
            auto viewport = dst.withHeight(dst.h * worldViewportShare);

            drawWorld(pass, bounds, viewport);
            drawWeapon(pass, bounds, viewport);

            auto strip = Graphics::Rect {dst.x,
                                         dst.y + dst.h * worldViewportShare,
                                         dst.w,
                                         dst.h * (1.0f - worldViewportShare)};
            drawScreen(pass, bounds, strip, viewRows / (float) doomHeight, 1.0f);
        }
        else
            drawScreen(pass, bounds, dst, 0.0f, 1.0f);
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
        auto vertexCount = 0;
        auto drawCount = eacpDoomBuildGeometry(
            geometry.data(), maxVertices, draws.data(), maxDraws, &vertexCount);

        if (drawCount <= 0 || vertexCount <= 0)
            return;

        worldBuffer.update(geometry.data(),
                           (std::size_t) vertexCount * sizeof(EacpDoomVertex));

        auto camera = eacpDoomGetCamera();
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
        auto count = eacpDoomGetHudSprites(hudSprites.data(), maxHudSprites);

        if (count <= 0)
            return;

        auto scaleX = viewport.w / (float) doomWidth;
        auto scaleY = viewport.h / viewRows;

        hudShader.viewSize = std::array {bounds.w, bounds.h};

        for (auto i = 0; i < count; ++i)
        {
            const auto& sprite = hudSprites[(std::size_t) i];

            hudShader.dstOrigin = std::array {viewport.x + sprite.x * scaleX,
                                              viewport.y + sprite.y * scaleY};
            hudShader.dstSize =
                std::array {sprite.width * scaleX, sprite.height * scaleY};
            hudShader.uRange =
                sprite.flip ? std::array {1.0f, 0.0f} : std::array {0.0f, 1.0f};
            hudShader.light = sprite.light;
            hudShader.texture = textureFor(sprite.textureId);

            pass.draw(hudShader);
        }
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
        if (window != nullptr && window->isMouseLocked())
            doom_mouse_move((int) (event.delta.x * mouseSpeed),
                            (int) (event.delta.y * mouseSpeed));
    }

    ScreenShader shader;
    WorldShader worldShader;
    HudShader hudShader;
    GPU::Texture framebuffer = makeIndexTexture();
    GPU::Texture paletteTexture = makePaletteTexture();
    GPU::Texture colormapTexture = makeColormapTexture();
    GPU::Buffer worldBuffer {GPU::Device::shared(),
                             nullptr,
                             (std::size_t) maxVertices * sizeof(EacpDoomVertex),
                             GPU::BufferUsage::Vertex};
    Vector<std::optional<GPU::Texture>> worldTextures;
    Vector<EacpDoomVertex> geometry;
    Vector<EacpDoomDraw> draws;
    Vector<EacpDoomHudSprite> hudSprites;
    std::array<std::uint8_t, 256 * 4> paletteData {};

    // Shift+F8 flips between the GPU world renderer and the software frame.
    bool gpuWorld = true;
    Graphics::Window* window = nullptr;
    Graphics::ModifierKeys modifiers;
};
} // namespace PureDoom
