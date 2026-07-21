#pragma once

#include <eacp/Core/Utils/Containers.h>

#include <array>
#include <cstdint>
#include <span>

// The snapshot interface between the DOOM engine's internals and the eacp
// renderer. EngineAccess.cpp is an ordinary translation unit that includes the
// engine's headers; nothing DOOM-typed leaks out through here, and the renderer
// never sees a fixed_t.
//
// PureDoom::Engine is this port's view of the engine, not Doom::Engine - which
// is the engine's own composition root and is always spelled with its namespace.
namespace PureDoom
{
// The container vocabulary, re-exported the way eacp and src/DOOM's
// Containers.h do it, so a signature here reads Vector<T>& rather than
// EA::Vector<T>&. This header is included before Common.h opens the namespace,
// so it has to say so itself.
using eacp::Array;
using eacp::Vector;
} // namespace PureDoom

namespace PureDoom::Engine
{
// Every row of the COLORMAP lump: 32 progressively darker remaps of the palette,
// then the invulnerability sphere's inverse map and a spare. The light
// calculation only ever picks one of the first 32, but a powerup can lock the
// view to row 32 outright (see WorldVertex::falloff), so all of them are wanted
// on the GPU.
inline constexpr auto colormapRows = 34;

inline constexpr auto screenWidth = 320;
inline constexpr auto screenHeight = 200;
inline constexpr auto screenPixels = screenWidth * screenHeight;

// The COLORMAP row Doom::drawMenu darkens its background with
// (Doom::DOOM_FLAG_MENU_DARKEN_BG).
inline constexpr auto menuDarkenRow = 20;

// The screen melt slides the outgoing frame down a two-pixel column at a time.
inline constexpr auto wipeColumns = screenWidth / 2;

// The 3D view's rect, which the automap's geometry is emitted in.
inline constexpr auto automapWidth = 320;
inline constexpr auto automapHeight = 168;

// The weapon and its muzzle flash: sprites the engine draws in screen space
// rather than in the world. An empty slot has a negative textureId.
inline constexpr auto hudSpriteCount = 2;

// One world vertex, already in the renderer's coordinate space: DOOM's map
// (x, y) ground plane with z up becomes (x, z, -y).
//
// The vector members are std::array rather than eacp's Array on purpose: this is
// a vertex layout, and GPU::ShaderValueOf - which is what turns
// vertexInput(&WorldVertex::position) into a Float3 - is specialised for
// std::array<float, N> and for raw float[N], and for nothing else.
struct WorldVertex
{
    std::array<float, 3> position {};
    std::array<float, 2> uv {};

    // The COLORMAP row the surface starts at (0 = brightest); the shader
    // subtracts the depth term from it.
    float light = 0.0f;

    // How much of that depth term applies: 1 normally, and 0 for a surface the
    // engine locks to a single row - the sky, a fullbright sprite frame, and
    // everything in sight while the invulnerability sphere or the light-amp visor
    // is up (setupFrame's fixedcolormap, which overrides light and distance
    // both). Carried per vertex rather than as a uniform because the sky is
    // exempt from the powerups: vanilla draws it through row 0 whatever the
    // player has picked up.
    float falloff = 0.0f;
};

// A run of vertices sharing one texture - one draw call.
struct TextureDraw
{
    int textureId = 0;
    int firstVertex = 0;
    int vertexCount = 0;
};

struct TextureInfo
{
    int width = 0;
    int height = 0;

    // Masked textures (every sprite, and the wall textures with holes in them)
    // carry an alpha channel and are stored four bytes per pixel: the palette
    // index in red, 0 or 255 in alpha. Everything else is one byte per pixel, and
    // the shader reads its alpha as an implicit 1.
    bool masked = false;
};

// One of the screen-space sprites, positioned in the 320 x 168 view with a
// top-left origin.
struct HudSprite
{
    int textureId = -1;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    // The COLORMAP row to draw it through, outright: a weapon is at no distance
    // from the camera, so nothing further is subtracted from this.
    float light = 0.0f;

    bool flip = false;
};

struct Camera
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float angle = 0.0f;
};

// One corner of the quad a map line is drawn as, in the engine's own automap
// frame (origin top-left), carrying a palette index - the automap picks raw
// colours rather than texturing anything.
//
// The quad is not widened here: that needs the line's length, and the vertex
// shader offsets each corner along the line's perpendicular instead, so the GPU
// normalizes for nothing.
struct AutomapVertex
{
    std::array<float, 2> position {};
    std::array<float, 2> direction {};
    float side = 0.0f;
    float color = 0.0f;
};

// True when the GPU renderer owns the view area rather than the software
// frame: the game is in a level and is not mid screen-melt.
bool viewActive();

// True when the automap has replaced the 3D view. The engine skips
// renderPlayerView entirely while it is up, so the two never coexist.
bool automapActive();

// Whether the engine is drawing the status bar, which the player sizes away
// with the last notch of the menu's screen size (screenblocks 11). The view
// is then the whole 320x200 frame rather than the 320x168 above the bar, and
// there is no strip to composite from the software frame - the rows the bar
// sat in hold the 3D view.
//
// The GPU renderer honours those two layouts and no others: at a smaller
// screen size it keeps drawing the full-width view rather than shrinking it
// into a border, that being a concession to 1993 hardware and not something
// this renderer needs to reproduce.
bool statusBarVisible();

// The rows the 3D view occupies, which is the above as a number.
float viewRows();

// Marks the walls the player can see from where they are standing, so the map
// keeps filling in while it is being looked at. Call once a tic, after the
// engine has run it; does nothing unless the automap is up.
//
// This is the port's one deliberate departure from vanilla. A line is revealed
// as a side effect of being drawn - Doom::storeWallRange sets ML_MAPPED as the
// software renderer lays the wall down - but Doom::displayFrame skips the render
// entirely while the automap is up, so vanilla's map stops filling in at the
// moment you open it and only catches up once you close it again.
//
// It walks the BSP the way renderPlayerView would and no further: the planes and
// the sprites are never drawn, and the four Doom::netUpdate calls it makes on the
// way are not wanted here, as they drain the event queue.
void revealAutomap();

// The COLORMAP row the view is remapped through before it reaches the palette:
// 0 while playing, and the row the menu darkens its background with while the
// menu is up. Row 0 is the identity map, so applying it unconditionally costs
// one lookup and changes nothing.
int darkenRow();

// The layers the engine draws over the view in software and nothing else
// reproduces: HUD messages, the level name, the PAUSE graphic, the menu, and
// the automap's marks. Writes screenWidth x screenHeight RGBA pixels - palette
// index in red, whether the pixel darkens with the view in green, coverage in
// alpha - and returns whether any pixel was covered.
//
// The engine offers no way to draw these anywhere but over the frame it has
// just rendered, so they are captured: screens[0] is pointed at scratch, the
// overlay drawers alone are run, and the real frame is put back. Coverage
// cannot be read off a single pass, because a pixel the menu legitimately drew
// may hold whatever the scratch was primed with, so the drawers run twice over
// two differently-primed buffers and a pixel counts as covered exactly where
// the two agree.
//
// The menu's background darkening is left out of the capture - it would touch
// every pixel, and so read as total coverage. darkenRow applies it to the GPU
// view instead, which keeps the world behind the menu at full resolution.
bool buildOverlay(std::span<std::uint8_t> outRgba);

// Re-applies the key bindings the app asked for with Doom::setDefaultInt.
// Call once, after Doom::initGame, which reads ~/.doomrc back over them.
void bindKeys();

// The engine's clock, in tics - 35 a second - with the fraction: the whole
// part says which tic the world is on, the rest how far into it time has
// moved. Both answers must come from ONE reading, or a tic boundary can fall
// between the two asks and the frame is placed a whole tic in the past.
//
// It counts from the epoch rather than from when the game started, so only its
// steps and its fraction are meaningful; both agree with the engine's.
double ticTime();

bool isWiping();

// The screen melt, while one is running: the frame it is sliding away, as
// palette indices, and how far down each column of it has moved so far. The
// frame coming in is neither wanted nor asked for - it is the framebuffer left
// as it is - which is what lets the melt run with no offscreen render target.
//
// Writes screenWidth x screenHeight indices and wipeColumns offsets in rows, and
// returns whether a melt is running. On the first frame of one it is not yet:
// the engine raises its wiping flag at the end of the frame that renders the
// incoming screen and only sets the melt up on the next.
bool buildWipe(std::span<std::uint8_t> outStart, std::span<std::uint8_t> outOffsets);

// The engine's mouse sensitivity (the options menu changes it). It scales the
// movement it is handed by (sensitivity + 5) / 10, so a view that runs ahead of
// the engine has to scale by the same amount or the two disagree.
int mouseSensitivity();

// Where the engine has the player standing as of the last tic, which between
// tics is not where the frame is drawn from - see View::viewCamera, which
// interpolates across the tic this one ends.
Camera camera();

// The live palette as RGBA, including the damage, pickup and invulnerability
// flashes, which are palette swaps. Writes 256 x 4 bytes.
void readPalette(std::span<std::uint8_t> outRgba);

// Every wall texture, flat and sprite the game loaded, in one id space and in
// that order. Stable for the whole run, so the renderer can upload each one
// lazily and keep it. Zero until the engine has initialised.
int textureCount();
TextureInfo textureInfo(int id);

// Fills out with the texture's pixels, row-major, top row first: one byte per
// pixel, or four when the texture is masked (see TextureInfo).
void readTexturePixels(int id, std::span<std::uint8_t> out);

// Fills out with colormapRows rows of 256 palette indices: row r maps a palette
// index to its appearance at light level r.
void readColormaps(std::span<std::uint8_t> out);

// The scratch a frame of world geometry is laid down in, and the prefix of it
// that was actually filled.
struct GeometryBuffers
{
    std::span<WorldVertex> vertices;
    std::span<TextureDraw> draws;
};

struct WorldGeometry
{
    std::span<const WorldVertex> vertices;
    std::span<const TextureDraw> draws;
};

// Builds this frame's world geometry - textured walls, floors, ceilings, the
// sky and every thing in the level as a camera-facing billboard, read fresh
// from the live level so moving sectors, animated textures and moving monsters
// are always current - grouped into per-texture draw runs.
//
// `camera` is where the frame is drawn from, which between tics is not quite
// where the engine has the player: the billboards have to square up to the view
// actually being drawn, or they sit half edge-on and flicker as it turns.
//
// `alpha` is how far into the current tic the frame sits (see ticTime).
// Everything that moves on the tic - the things in the level, and the floors
// and ceilings a door or a lift is driving - is placed at that point between
// where it was and where it is, so it glides with the camera instead of
// stepping 35 times a second against it.
WorldGeometry
    buildGeometry(const Camera& camera, float alpha, const GeometryBuffers& into);

// Builds the automap as triangles in its own frame: the level's walls in the
// colours the automap picks for them, the grid, the player's arrow, the things
// (with the map cheat on) and the crosshair.
//
// Vanilla rasterizes the same lines with Bresenham straight into the frame, so
// its map is stepped in space - it snaps the view to whole map pixels - and in
// time. Neither is reproduced: the lines carry their real endpoints, and
// `camera` recentres the map on the interpolated view rather than on the
// player's last tic. Zooming and manual panning still step, being the engine's
// own per-tic quantities.
std::span<const AutomapVertex> buildAutomap(const Camera& camera,
                                            std::span<AutomapVertex> into);

// Remembers where every sector's floor and ceiling are, so the next frame can
// draw them part-way to wherever they move next. Call once per tic, just
// before the engine runs it.
void snapshotTic();

// The player's weapon and muzzle flash, one entry per slot, so a caller can
// match a slot against the one it saw last tic and interpolate between them.
Array<HudSprite, hudSpriteCount> hudSprites();
} // namespace PureDoom::Engine
