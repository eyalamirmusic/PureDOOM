#include <eacp/GPU/GPU.h>

#include <array>
#include <cmath>
#include <cstdint>

#include "../../PureDOOM.h"

#include "EngineAccess.h"

// The engine's current palette (RGB triplets), kept up to date by
// I_SetPalette — including the damage/pickup/invulnerability flashes, which
// are palette swaps. Defined in DoomImpl.c; PureDOOM.h doesn't declare it in
// its public section.
extern "C" unsigned char screen_palette[256 * 3];

using namespace eacp;

namespace
{
constexpr auto doomWidth = 320;
constexpr auto doomHeight = 200;

// The software frame splits into the 3D view (168 rows) and the status bar
// (32 rows); with the 1.2 CRT stretch the view fills exactly 84% of the
// displayed height, and its displayed aspect (320 : 201.6) fixes the GPU
// camera's field of view: DOOM's horizontal FOV is 90 degrees.
constexpr auto viewRows = 168.0f;
constexpr auto worldViewportShare = viewRows * 1.2f / 240.0f;
constexpr auto worldAspect = 320.0f / (viewRows * 1.2f);

constexpr auto maxWalls = 8192;
constexpr auto pi = 3.14159265358979f;

// DOOM's 320x200 frame was designed for 4:3 CRTs, whose non-square pixels
// stretched it 1.2x vertically; 320x240 is the intended display shape.
constexpr auto displayWidth = 320.0f;
constexpr auto displayHeight = 240.0f;

// eacp has no display-metrics API yet (see the gap log), so the initial size
// is a guess that fits laptop screens; the window resizes from there.
constexpr auto windowScale = 3;

constexpr auto mouseSpeed = 4.0f;

doom_key_t toDoomKey(const Graphics::KeyEvent& event)
{
    using namespace Graphics;

    switch (event.keyCode)
    {
        case KeyCode::Tab:
            return DOOM_KEY_TAB;
        case KeyCode::Return:
            return DOOM_KEY_ENTER;
        case KeyCode::Escape:
            return DOOM_KEY_ESCAPE;
        case KeyCode::Space:
            return DOOM_KEY_SPACE;
        case KeyCode::Delete:
            return DOOM_KEY_BACKSPACE;
        case KeyCode::LeftArrow:
            return DOOM_KEY_LEFT_ARROW;
        case KeyCode::RightArrow:
            return DOOM_KEY_RIGHT_ARROW;
        case KeyCode::UpArrow:
            return DOOM_KEY_UP_ARROW;
        case KeyCode::DownArrow:
            return DOOM_KEY_DOWN_ARROW;
        case KeyCode::F1:
            return DOOM_KEY_F1;
        case KeyCode::F2:
            return DOOM_KEY_F2;
        case KeyCode::F3:
            return DOOM_KEY_F3;
        case KeyCode::F4:
            return DOOM_KEY_F4;
        case KeyCode::F5:
            return DOOM_KEY_F5;
        case KeyCode::F6:
            return DOOM_KEY_F6;
        case KeyCode::F7:
            return DOOM_KEY_F7;
        case KeyCode::F8:
            return DOOM_KEY_F8;
        case KeyCode::F9:
            return DOOM_KEY_F9;
        case KeyCode::F10:
            return DOOM_KEY_F10;
        case KeyCode::F11:
            return DOOM_KEY_F11;
        case KeyCode::F12:
            return DOOM_KEY_F12;
        default:
            break;
    }

    // DOOM's codes for letters, digits and punctuation are their ASCII
    // values, so everything else maps through the typed character. This also
    // covers keys eacp's KeyCode table has no constant for yet (see the gap
    // log in CLAUDE.md).
    const auto& characters = event.charactersIgnoringModifiers;

    if (characters.size() == 1)
    {
        auto character = characters.front();

        if (character >= 'A' && character <= 'Z')
            character = (char) (character - 'A' + 'a');

        if (character >= ' ' && character <= '~')
            return (doom_key_t) character;
    }

    return DOOM_KEY_UNKNOWN;
}

doom_button_t toDoomButton(Graphics::MouseButton button)
{
    switch (button)
    {
        case Graphics::MouseButton::Right:
            return DOOM_RIGHT_BUTTON;
        case Graphics::MouseButton::Middle:
            return DOOM_MIDDLE_BUTTON;
        default:
            return DOOM_LEFT_BUTTON;
    }
}

struct ScreenVertex
{
    float corner[2];
};

constexpr ScreenVertex unitQuad[] = {
    {{0.0f, 0.0f}},
    {{1.0f, 0.0f}},
    {{1.0f, 1.0f}},
    {{0.0f, 0.0f}},
    {{1.0f, 1.0f}},
    {{0.0f, 1.0f}},
};

// Draws the DOOM frame natively: the engine's palette-indexed framebuffer is
// sampled as an R8 texture and looked up in a 256x1 palette texture, so the
// only CPU pixel work left is the engine's own software rasterizer. The unit
// quad maps onto dstRect (view points) — the letterboxed 4:3 area.
struct ScreenShader final : GPU::ShaderProgram
{
    ScreenShader() { compile(); }

    void define() override
    {
        auto corner = vertexInput(&ScreenVertex::corner);

        auto x = dstOrigin.x() + corner.x() * dstSize.x();
        auto y = dstOrigin.y() + corner.y() * dstSize.y();
        auto ndcX = x / viewSize.x() * 2.0f - 1.0f;
        auto ndcY = 1.0f - y / viewSize.y() * 2.0f;
        setPosition(float4(ndcX, ndcY, 0.0f, 1.0f));

        auto v = uvY.x() + corner.y() * (uvY.y() - uvY.x());
        auto uv = varying(float2(corner.x(), v));
        auto index = sample(screenIndices, uv).x();
        auto paletteU = (index * 255.0f + 0.5f) / 256.0f;
        auto color = sample(palette, float2(paletteU, 0.5f));
        setFragment(float4(color.xyz(), 1.0f));
    }

    GPU::Uniform<GPU::Float2> viewSize;
    GPU::Uniform<GPU::Float2> dstOrigin;
    GPU::Uniform<GPU::Float2> dstSize;

    // Vertical source window into the frame (start, end in 0..1): the full
    // frame is {0, 1}, the status-bar strip {168/200, 1}.
    GPU::Uniform<GPU::Float2> uvY;
    GPU::Uniform<GPU::Texture2D> screenIndices;
    GPU::Uniform<GPU::Texture2D> palette;

    EACP_SHADER(viewSize, dstOrigin, dstSize, uvY, screenIndices, palette)
};

struct WorldVertex
{
    float position[3];
    float color[3];
};

// Draws the level geometry the engine access layer extracts, with DOOM's
// map coordinates (x, y ground plane, z up) mapped to GPU space as
// (x, z, -y). The full-frame perspective projection is then squeezed into
// the 3D viewport's sub-rect of the window (offsets scale by w so they
// survive the perspective divide), leaving the status bar strip and the
// letterbox bars untouched.
struct WorldShader final : GPU::ShaderProgram
{
    WorldShader() { compile(); }

    void define() override
    {
        auto position = vertexInput(&WorldVertex::position);
        auto color = vertexInput(&WorldVertex::color);

        auto view = rotateY(-yaw) * translate(-camX, -camY, -camZ);
        auto fovY = 2.0f * std::atan(1.0f / worldAspect);
        auto projection = perspective(constant(worldAspect), fovY, 4.0f, 16384.0f);
        auto clip = projection * view * float4(position, 1.0f);

        auto x = clip.x() * ndcScale.x() + clip.w() * ndcOffset.x();
        auto y = clip.y() * ndcScale.y() + clip.w() * ndcOffset.y();
        setPosition(float4(x, y, clip.z(), clip.w()));

        setFragment(float4(varying(color), 1.0f));
    }

    GPU::Uniform<GPU::Float> camX;
    GPU::Uniform<GPU::Float> camY;
    GPU::Uniform<GPU::Float> camZ;
    GPU::Uniform<GPU::Float> yaw;
    GPU::Uniform<GPU::Float2> ndcScale;
    GPU::Uniform<GPU::Float2> ndcOffset;

    EACP_SHADER(camX, camY, camZ, yaw, ndcScale, ndcOffset)
};

// Snaps a proposed content size back to DOOM's 4:3 shape by applying the
// smaller of the two possible corrections, so dragging any window edge or
// corner feels natural.
void keepDisplayAspect(int& width, int& height)
{
    constexpr auto aspect = displayWidth / displayHeight;

    auto heightFromWidth = (float) width / aspect;
    auto widthFromHeight = (float) height * aspect;

    if (std::abs(heightFromWidth - (float) height)
        <= std::abs(widthFromHeight - (float) width))
        height = (int) std::lround(heightFromWidth);
    else
        width = (int) std::lround(widthFromHeight);
}

Graphics::WindowOptions windowOptions()
{
    auto options = Graphics::WindowOptions {};
    options.width = (int) displayWidth * windowScale;
    options.height = (int) displayHeight * windowScale;
    options.title = "Pure DOOM (eacp)";
    options.minWidth = (int) displayWidth;
    options.minHeight = (int) displayHeight;
    options.onWillResize = keepDisplayAspect;
    return options;
}

// The largest centered 4:3 rect that fits the view, in view points; the
// window's aspect constraint keeps this a no-op except during zoom and
// fullscreen, where black bars fill the rest.
Graphics::Rect letterboxedDisplayRect(const Graphics::Rect& bounds)
{
    constexpr auto contentAspect = displayWidth / displayHeight;

    if (bounds.w <= 0.0f || bounds.h <= 0.0f)
        return bounds;

    auto width = bounds.h * contentAspect;

    if (width <= bounds.w)
        return {(bounds.w - width) / 2.0f, 0.0f, width, bounds.h};

    auto height = bounds.w / contentAspect;
    return {0.0f, (bounds.h - height) / 2.0f, bounds.w, height};
}

GPU::Texture makeIndexTexture()
{
    auto descriptor = GPU::TextureDescriptor {};
    descriptor.width = doomWidth;
    descriptor.height = doomHeight;
    descriptor.format = GPU::TextureFormat::R8Unorm;
    descriptor.filter = GPU::TextureFilter::Nearest;

    return GPU::Device::shared().makeTexture(descriptor, nullptr);
}

GPU::Texture makePaletteTexture()
{
    auto descriptor = GPU::TextureDescriptor {};
    descriptor.width = 256;
    descriptor.height = 1;
    descriptor.filter = GPU::TextureFilter::Nearest;

    return GPU::Device::shared().makeTexture(descriptor, nullptr);
}
} // namespace

struct DoomView final : GPU::GPUView
{
    DoomView()
    {
        setSampleCount(1);
        setDepth(true);
        shader.setVertices(unitQuad);
        shader.prepare(sampleCount(), true);
        shader.screenIndices = framebuffer;
        shader.palette = paletteTexture;
        worldShader.prepare(sampleCount(), true);
        walls.getVector().resize(maxWalls);
        wallVertices.getVector().reserve((std::size_t) maxWalls * 6);
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

        auto bounds = getLocalBounds();
        auto dst = letterboxedDisplayRect(bounds);

        auto pass = frame.beginPass({Graphics::Color::black()});

        if (gpuWorld && eacpDoomWorldVisible())
        {
            drawWorld(pass, bounds, dst.withHeight(dst.h * worldViewportShare));

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
        auto wallCount = eacpDoomGetWalls(walls.data(), maxWalls);
        buildWallVertices(wallCount);

        if (wallVertices.size() == 0)
            return;

        worldBuffer.update(wallVertices.data(),
                           wallVertices.size() * sizeof(WorldVertex));

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
        pass.draw((int) wallVertices.size());
    }

    void buildWallVertices(int wallCount)
    {
        wallVertices.getVector().clear();

        for (auto i = 0; i < wallCount; ++i)
            appendWallVertices(walls[(std::size_t) i]);
    }

    void appendWallVertices(const EacpDoomWall& wall)
    {
        // The software renderer's fake contrast: north/south walls a step
        // brighter, east/west a step darker, so corners stay readable.
        auto shade = wall.light;

        if (wall.x1 == wall.x2)
            shade = shade * 1.15f > 1.0f ? 1.0f : shade * 1.15f;
        else if (wall.y1 == wall.y2)
            shade = shade * 0.85f;

        auto color = std::array {0.62f * shade, 0.56f * shade, 0.48f * shade};

        auto a = WorldVertex {{wall.x1, wall.bottom, -wall.y1},
                              {color[0], color[1], color[2]}};
        auto b = WorldVertex {{wall.x2, wall.bottom, -wall.y2},
                              {color[0], color[1], color[2]}};
        auto c = WorldVertex {{wall.x2, wall.top, -wall.y2},
                              {color[0], color[1], color[2]}};
        auto d = WorldVertex {{wall.x1, wall.top, -wall.y1},
                              {color[0], color[1], color[2]}};

        wallVertices.add(a);
        wallVertices.add(b);
        wallVertices.add(c);
        wallVertices.add(a);
        wallVertices.add(c);
        wallVertices.add(d);
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
    GPU::Texture framebuffer = makeIndexTexture();
    GPU::Texture paletteTexture = makePaletteTexture();
    GPU::Buffer worldBuffer {GPU::Device::shared(),
                             nullptr,
                             (std::size_t) maxWalls * 6 * sizeof(WorldVertex),
                             GPU::BufferUsage::Vertex};
    Vector<EacpDoomWall> walls;
    Vector<WorldVertex> wallVertices;
    std::array<std::uint8_t, 256 * 4> paletteData {};

    // Shift+F8 flips between the GPU world renderer and the software frame.
    bool gpuWorld = true;
    Graphics::Window* window = nullptr;
    Graphics::ModifierKeys modifiers;
};

struct DoomApp
{
    DoomApp()
    {
        window.setContentView(view);
        view.window = &window;
        view.focus();
    }

    DoomView view;
    Graphics::Window window {windowOptions()};
};

int main(int argc, char** argv)
{
    // PureDOOM locates WAD files via DOOMWADDIR (defaulting to the current
    // directory), so point it at the repository's shareware WAD unless the
    // user already chose a directory.
    if (!getEnv("DOOMWADDIR"))
        setEnv("DOOMWADDIR", PUREDOOM_ROOT_DIR);

    doom_set_default_int("key_up", DOOM_KEY_W);
    doom_set_default_int("key_down", DOOM_KEY_S);
    doom_set_default_int("key_strafeleft", DOOM_KEY_A);
    doom_set_default_int("key_straferight", DOOM_KEY_D);
    doom_set_default_int("key_use", DOOM_KEY_E);
    doom_set_default_int("mouse_move", 0);

    doom_init(argc, argv, DOOM_FLAG_MENU_DARKEN_BG);

    return Apps::run<DoomApp>();
}
