#pragma once

// Plain-C snapshot interface between the DOOM engine internals and the eacp
// renderer. Implemented in EngineAccess.c, which DoomImpl.c includes at the end
// of the engine translation unit so the implementation can reach the engine's
// (non-static) types and globals; nothing DOOM-typed leaks out.

// Every row of the COLORMAP lump: 32 progressively darker remaps of the palette,
// then the invulnerability sphere's inverse map and a spare. The light
// calculation only ever picks one of the first 32, but a powerup can lock the
// view to row 32 outright (see EacpDoomVertex::falloff), so all of them are
// wanted on the GPU.
#define EACP_DOOM_COLORMAP_ROWS 34

#define EACP_DOOM_SCREEN_WIDTH 320
#define EACP_DOOM_SCREEN_HEIGHT 200

// The COLORMAP row Doom::drawMenu darkens its background with (DOOM_FLAG_MENU_DARKEN_BG).
#define EACP_DOOM_MENU_DARKEN_ROW 20

// The screen melt slides the outgoing frame down a two-pixel column at a time.
#define EACP_DOOM_WIPE_COLUMNS (EACP_DOOM_SCREEN_WIDTH / 2)

// One world vertex, already in the renderer's coordinate space: DOOM's map
// (x, y) ground plane with z up becomes (x, z, -y).
typedef struct
{
    float position[3];
    float uv[2];

    // The COLORMAP row the surface starts at (0 = brightest); the shader
    // subtracts the depth term from it.
    float light;

    // How much of that depth term applies: 1 normally, and 0 for a surface the
    // engine locks to a single row - the sky, a fullbright sprite frame, and
    // everything in sight while the invulnerability sphere or the light-amp visor
    // is up (R_SetupFrame's fixedcolormap, which overrides light and distance
    // both). Carried per vertex rather than as a uniform because the sky is
    // exempt from the powerups: vanilla draws it through row 0 whatever the
    // player has picked up.
    float falloff;
} EacpDoomVertex;

// A run of vertices sharing one texture - one draw call.
typedef struct
{
    int textureId;
    int firstVertex;
    int vertexCount;
} EacpDoomDraw;

typedef struct
{
    int width;
    int height;

    // Masked textures (every sprite, and the wall textures with holes in them)
    // carry an alpha channel and are stored four bytes per pixel: the palette
    // index in red, 0 or 255 in alpha. Everything else is one byte per pixel, and
    // the shader reads its alpha as an implicit 1.
    int masked;
} EacpDoomTextureInfo;

// The weapon and its muzzle flash: sprites the engine draws in screen space
// rather than in the world, positioned in the 320 x 168 view with a top-left
// origin. An empty slot has a negative textureId.
#define EACP_DOOM_HUD_SPRITES 2

typedef struct
{
    int textureId;
    float x, y;
    float width, height;

    // The COLORMAP row to draw it through, outright: a weapon is at no distance
    // from the camera, so nothing further is subtracted from this.
    float light;

    int flip;
} EacpDoomHudSprite;

typedef struct
{
    float x, y;
    float z;
    float angle;
} EacpDoomCamera;

// One corner of the quad a map line is drawn as, in the engine's own automap
// frame (origin top-left), carrying a palette index - the automap picks raw
// colours rather than texturing anything.
//
// The quad is not widened here: that needs the line's length, and the only square
// root in this translation unit is eacpSqrt, which is Newton's method (the engine
// links no libm). The vertex shader offsets each corner along the line's
// perpendicular instead, and the GPU normalizes for nothing.
typedef struct
{
    float position[2];
    float direction[2];
    float side;
    float color;
} EacpDoomAutomapVertex;

// The 3D view's rect, which the automap's geometry is emitted in.
#define EACP_DOOM_AUTOMAP_WIDTH 320
#define EACP_DOOM_AUTOMAP_HEIGHT 168

#ifdef __cplusplus
extern "C"
{
#endif

    // True when the GPU renderer owns the view area rather than the software
    // frame: the game is in a level and is not mid screen-melt.
    int eacpDoomViewActive();

    // True when the automap has replaced the 3D view. The engine skips
    // R_RenderPlayerView entirely while it is up, so the two never coexist.
    int eacpDoomAutomapActive();

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
    int eacpDoomStatusBarVisible();

    // The rows the 3D view occupies, which is the above as a number.
    float eacpDoomViewRows();

    // Marks the walls the player can see from where they are standing, so the map
    // keeps filling in while it is being looked at. Call once a tic, after the
    // engine has run it; does nothing unless the automap is up.
    //
    // This is the port's one deliberate departure from vanilla. A line is
    // revealed as a side effect of being drawn - Doom::storeWallRange sets ML_MAPPED
    // as the software renderer lays the wall down - but Doom::displayFrame skips the render
    // entirely while the automap is up, so vanilla's map stops filling in at the
    // moment you open it and only catches up once you close it again.
    //
    // It walks the BSP the way R_RenderPlayerView would and no further: the planes
    // and the sprites are never drawn, and the four Doom::netUpdate calls it makes on
    // the way are not wanted here, as they drain the event queue.
    void eacpDoomRevealAutomap();

    // The COLORMAP row the view is remapped through before it reaches the palette:
    // 0 while playing, and the row the menu darkens its background with while the
    // menu is up. Row 0 is the identity map, so applying it unconditionally costs
    // one lookup and changes nothing.
    int eacpDoomDarkenRow();

    // The layers the engine draws over the view in software and nothing else
    // reproduces: HUD messages, the level name, the PAUSE graphic, the menu, and
    // the automap's marks. Writes EACP_DOOM_SCREEN_WIDTH x EACP_DOOM_SCREEN_HEIGHT
    // RGBA pixels - palette index in red, whether the pixel darkens with the view
    // in green, coverage in alpha - and returns whether any pixel was covered.
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
    // every pixel, and so read as total coverage. eacpDoomDarkenRow applies it to
    // the GPU view instead, which keeps the world behind the menu at full
    // resolution.
    int eacpDoomBuildOverlay(unsigned char* outRgba);

    // Re-applies the key bindings the app asked for with doom_set_default_int.
    // Call once, after doom_init, which reads ~/.doomrc back over them.
    void eacpDoomBindKeys();

    // The engine's clock, in tics - 35 a second - with the fraction: the whole
    // part says which tic the world is on, the rest how far into it time has
    // moved. Both answers must come from ONE reading, or a tic boundary can fall
    // between the two asks and the frame is placed a whole tic in the past.
    //
    // It counts from the epoch rather than from when the game started, so only its
    // steps and its fraction are meaningful; both agree with the engine's.
    double eacpDoomTicTime();

    int eacpDoomIsWiping();

    // The screen melt, while one is running: the frame it is sliding away, as
    // palette indices, and how far down each column of it has moved so far. The
    // frame coming in is neither wanted nor asked for - it is the framebuffer left
    // as it is - which is what lets the melt run with no offscreen render target.
    //
    // Writes EACP_DOOM_SCREEN_WIDTH x EACP_DOOM_SCREEN_HEIGHT indices and
    // EACP_DOOM_WIPE_COLUMNS offsets in rows, and returns whether a melt is
    // running. On the first frame of one it is not yet: the engine raises its
    // wiping flag at the end of the frame that renders the incoming screen and
    // only sets the melt up on the next.
    int eacpDoomBuildWipe(unsigned char* outStart, unsigned char* outOffsets);

    // The engine's mouse sensitivity (the options menu changes it). It scales the
    // movement it is handed by (sensitivity + 5) / 10, so a view that runs ahead of
    // the engine has to scale by the same amount or the two disagree.
    int eacpDoomMouseSensitivity();

    EacpDoomCamera eacpDoomGetCamera();

    // Every wall texture, flat and sprite the game loaded, in one id space and in
    // that order. Stable for the whole run, so the renderer can upload each one
    // lazily and keep it. Zero until the engine has initialised.
    int eacpDoomGetTextureCount();
    EacpDoomTextureInfo eacpDoomGetTextureInfo(int id);

    // Fills out with the texture's pixels, row-major, top row first: one byte per
    // pixel, or four when the texture is masked (see EacpDoomTextureInfo).
    void eacpDoomGetTexturePixels(int id, unsigned char* out);

    // Fills out with EACP_DOOM_COLORMAP_ROWS rows of 256 palette indices: row r
    // maps a palette index to its appearance at light level r.
    void eacpDoomGetColormaps(unsigned char* out);

    // Builds this frame's world geometry - textured walls, floors, ceilings, the
    // sky and every thing in the level as a camera-facing billboard, read fresh
    // from the live level so moving sectors, animated textures and moving monsters
    // are always current - grouped into per-texture draw runs. Writes the vertex
    // count through outVertexCount and returns the draw count.
    //
    // `camera` is where the frame is drawn from, which between tics is not quite
    // where the engine has the player: the billboards have to square up to the view
    // actually being drawn, or they sit half edge-on and flicker as it turns.
    //
    // `alpha` is how far into the current tic the frame sits (see eacpDoomTicTime).
    // Everything that moves on the tic - the things in the level, and the floors
    // and ceilings a door or a lift is driving - is placed at that point between
    // where it was and where it is, so it glides with the camera instead of
    // stepping 35 times a second against it.
    int eacpDoomBuildGeometry(const EacpDoomCamera* camera,
                              float alpha,
                              EacpDoomVertex* vertices,
                              int maxVertices,
                              EacpDoomDraw* draws,
                              int maxDraws,
                              int* outVertexCount);

    // Builds the automap as triangles in its own frame: the level's walls in the
    // colours the automap picks for them, the grid, the player's arrow, the things
    // (with the map cheat on) and the crosshair. Returns the vertex count.
    //
    // Vanilla rasterizes the same lines with Bresenham straight into the frame, so
    // its map is stepped in space - it snaps the view to whole map pixels - and in
    // time. Neither is reproduced: the lines carry their real endpoints, and
    // `camera` recentres the map on the interpolated view rather than on the
    // player's last tic. Zooming and manual panning still step, being the engine's
    // own per-tic quantities.
    int eacpDoomBuildAutomap(const EacpDoomCamera* camera,
                             EacpDoomAutomapVertex* vertices,
                             int maxVertices);

    // Remembers where every sector's floor and ceiling are, so the next frame can
    // draw them part-way to wherever they move next. Call once per tic, just
    // before the engine runs it.
    void eacpDoomSnapshotTic();

    // The player's weapon and muzzle flash. Always writes EACP_DOOM_HUD_SPRITES
    // entries, one per slot, so a caller can match a slot against the one it saw
    // last tic and interpolate between them.
    void eacpDoomGetHudSprites(EacpDoomHudSprite* out);

#ifdef __cplusplus
}
#endif
