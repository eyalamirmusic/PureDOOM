#pragma once

#include "Audio.h"
#include "AutomapShader.h"
#include "FuzzShader.h"
#include "HudFuzzShader.h"
#include "HudShader.h"
#include "Input.h"
#include "OverlayShader.h"
#include "ResolveShader.h"
#include "ScreenShader.h"
#include "Textures.h"
#include "WipeShader.h"
#include "WorldShader.h"

#include <algorithm>
#include <optional>

#include <DOOM/Game/DoomMain.h>
#include <DOOM/UI/Automap.h>
namespace PureDoom
{

// The game's window contents: it steps the engine when the engine's own clock
// says a tic is due, draws the frame either as DOOM's own software image or as
// GPU world geometry with the software status bar beneath it, and feeds the
// keyboard and the mouse back in.
struct View final : GPU::GPUView
{
    explicit View(Graphics::Window& windowToUse);

    void prepareShader(DoomShader& shader) const;
    void prepareQuadShader(ScreenQuadShader& shader);

    // The three that draw into the world target rather than onto the screen: a
    // texture pass is single-sampled whatever the view is, and a pipeline has to
    // name the format of the attachment it writes.
    void prepareTargetShader(GPU::ShaderProgram& shader, GPU::BlendMode blend) const;

    void update(Threads::FrameTime) override;

    // Where the camera is now, rather than where it was when the last tic ran:
    // the engine moves the player 35 times a second while the display refreshes
    // two to four times as often, so a view taken straight from the engine sits
    // still for two or three frames and then jumps.
    Engine::Camera viewCamera() const;

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
    float viewAngle() const;

    float pendingTurn() const;

    // DOOM reads the mouse once a tic and the last event it saw wins:
    // Doom::gameResponder assigns the movement rather than adding to it. Handing it one
    // event per platform mouse move - which arrive several times per tic - would
    // throw all but the last of them away, so the whole of the movement goes over
    // at once, as vanilla's Doom::startTic does.
    void flushMouse();

    void render(GPU::Frame& frame) override;

    // What the menu darkens behind itself. The status bar needs none of it: the
    // engine darkens its own frame, which is where the strip comes from.
    void setDarkenRow(float row);

    static Graphics::Rect statusBarRect(const Graphics::Rect& dst, float rows);

    void drawScreen(GPU::RenderPass& pass,
                    const Graphics::Rect& bounds,
                    const Graphics::Rect& dst,
                    float uvTop,
                    float uvBottom);

    // Renders the level into the world target - the surfaces, then the spectres'
    // marks over them - and says whether anything came out. A pass of its own,
    // ended before the screen's begins: a command buffer has one encoder open at
    // a time, and a texture cannot be sampled by the pass that writes it.
    bool renderWorld(GPU::Frame& frame,
                     const Graphics::Rect& bounds,
                     const Graphics::Rect& viewport,
                     float rows);

    // One run of the world's vertex buffer per texture, drawn by hand: the
    // geometry is the app's rather than the program's, which is the one
    // assumption RenderPass::draw(program) makes that this cannot meet (see the
    // gap log's interface findings).
    void drawGeometry(GPU::RenderPass& pass,
                      WorldViewShader& shader,
                      std::span<const Engine::TextureDraw> runs);

    // The world target onto the screen: indices to colours, and the spectres.
    void resolveWorld(GPU::RenderPass& pass,
                      const Graphics::Rect& bounds,
                      const Graphics::Rect& viewport,
                      float rows);

    // Created at the window's pixel size and rebuilt when that changes, which is
    // the one thing that invalidates it.
    void ensureWorldTarget();

    // Where a HUD sprite lands in the window this frame - the weapon bobs on the
    // tic like everything else, so it is placed between tics like everything
    // else. An empty slot comes back invisible.
    struct HudPlacement
    {
        Graphics::Rect dst;
        std::array<float, 2> uRange {};
        bool visible = false;
    };

    HudPlacement
        placeHudSprite(int slot, const Graphics::Rect& viewport, float rows) const;

    void drawWeapon(GPU::RenderPass& pass,
                    const Graphics::Rect& bounds,
                    const Graphics::Rect& viewport,
                    float rows);

    // The weapon's other half, and only while the invisibility sphere is up: a
    // fuzzed weapon is marked into the world target with the spectres rather
    // than drawn over the screen with the rest of the HUD.
    void markWeaponFuzz(GPU::RenderPass& pass,
                        const Graphics::Rect& bounds,
                        const Graphics::Rect& viewport,
                        float rows);

    // Rebuilt every frame rather than every tic, because it is centred on the
    // view, and between tics the view is somewhere the engine has not been yet.
    void drawAutomap(GPU::RenderPass& pass,
                     const Graphics::Rect& bounds,
                     const Graphics::Rect& viewport);

    // The public geometry is in logical points; a scissor rect is in device
    // pixels.
    Graphics::Rect inPixels(const Graphics::Rect& rect) const;

    void drawOverlay(GPU::RenderPass& pass,
                     const Graphics::Rect& bounds,
                     const Graphics::Rect& dst);

    void drawWipe(GPU::RenderPass& pass,
                  const Graphics::Rect& bounds,
                  const Graphics::Rect& dst);

    void updateOverlay();

    void updateWipe();

    void updatePalette();

    // The WAD's graphics are loaded once, so the slots are sized once; the
    // colormap comes along with them.
    void ensureWorldTextures();

    // Uploaded the first time something is drawn with it: a WAD holds well over a
    // thousand sprite lumps, and a level shows a small fraction of them.
    GPU::Texture& textureFor(int id);

    // DOOM binds Ctrl/Shift/Alt as ordinary keys (fire/run/strafe), but eacp
    // reports them only as modifier state, never as key events (see the gap log).
    void syncModifierKeys(const Graphics::ModifierKeys& current);
    void keyDown(const Graphics::KeyEvent& event) override;
    void keyUp(const Graphics::KeyEvent& event) override;
    void mouseDown(const Graphics::MouseEvent& event) override;
    void mouseUp(const Graphics::MouseEvent& event) override;
    void mouseMoved(const Graphics::MouseEvent& event) override;
    void mouseDragged(const Graphics::MouseEvent& event) override;

    // The device's own movement, not the pointer's: the system's acceleration
    // curve is there to help a cursor reach a target, and through it the same
    // flick of the hand turns the player a different amount depending how fast it
    // was made.
    void aim(const Graphics::MouseEvent& event);

    static constexpr float mouseSpeed = 4.0f;

    ScreenShader screenShader;
    WorldShader worldShader;
    FuzzShader fuzzShader;
    ResolveShader resolveShader;
    HudShader hudShader;
    HudFuzzShader hudFuzzShader;
    AutomapShader automapShader;
    OverlayShader overlayShader;
    WipeShader wipeShader;

    // DOOM's frame at the window's resolution: what the world is drawn into, and
    // what the fuzz reads. Optional only because it is sized from the window,
    // which the constructor has no size for yet.
    std::optional<GPU::Texture> worldTarget;
    Graphics::Point targetPixels;

    GPU::Texture framebuffer = makeIndexTexture();
    GPU::Texture paletteTexture = makePaletteTexture();
    GPU::Texture colormapTexture = makeColormapTexture();
    GPU::Texture overlayTexture = makeOverlayTexture();
    GPU::Texture wipeTexture = makeIndexTexture();
    GPU::Texture wipeOffsetTexture = makeWipeOffsetTexture();

    GPU::Buffer worldBuffer {GPU::Device::shared(),
                             nullptr,
                             maxVertices * sizeof(Engine::WorldVertex),
                             GPU::BufferUsage::Vertex};
    GPU::Buffer automapBuffer {GPU::Device::shared(),
                               nullptr,
                               maxAutomapVertices * sizeof(Engine::AutomapVertex),
                               GPU::BufferUsage::Vertex};

    Vector<std::optional<GPU::Texture>> worldTextures;
    Vector<Engine::WorldVertex> geometry;
    Vector<Engine::TextureDraw> draws;
    Vector<Engine::AutomapVertex> automap;
    Vector<std::uint8_t> overlayPixels;
    Vector<std::uint8_t> wipePixels;
    Vector<std::uint8_t> paletteData;
    Array<std::uint8_t, Engine::wipeColumns> wipeOffsets;

    // Always hudSpriteCount entries, so this tic's slot can be matched against
    // last tic's and interpolated between.
    Array<Engine::HudSprite, Engine::hudSpriteCount> hud;
    Array<Engine::HudSprite, Engine::hudSpriteCount> previousHud;

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

    Engine::Camera previousCamera {};
    Engine::Camera currentCamera {};
    bool hasCamera = false;

    // The output device and the MIDI port. It is the view that owns them
    // because it is the view that is called once a display refresh, and the
    // engine's mixer has to be pulled from the same thread that steps it.
    Audio audio;

    Graphics::Window& window;
    Graphics::ModifierKeys modifiers;
};
} // namespace PureDoom
