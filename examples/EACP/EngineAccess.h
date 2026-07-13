#pragma once

// Plain-C snapshot interface between the DOOM engine internals and the eacp
// renderer. Implemented in EngineAccess.c, which DoomImpl.c includes at the
// end of the engine translation unit so the implementation can reach the
// engine's (non-static) types and globals; nothing DOOM-typed leaks out.

// The COLORMAP rows the renderer uses: 32 progressively darker remaps of the
// palette. (The lump carries two more - the invulnerability inverse map and a
// spare - which the light calculation never selects.)
#define EACP_DOOM_COLORMAP_ROWS 32

// The engine's frame, which the software-only overlay is captured at.
#define EACP_DOOM_SCREEN_WIDTH 320
#define EACP_DOOM_SCREEN_HEIGHT 200

// The COLORMAP row M_Drawer darkens its background with (DOOM_FLAG_MENU_DARKEN_BG).
#define EACP_DOOM_MENU_DARKEN_ROW 20

// The screen melt slides the outgoing frame down a two-pixel column at a time.
#define EACP_DOOM_WIPE_COLUMNS (EACP_DOOM_SCREEN_WIDTH / 2)

// One world vertex, already in the renderer's coordinate space: DOOM's map
// (x, y) ground plane with z up becomes (x, z, -y).
typedef struct
{
    float position[3];
    float uv[2];

    // The sector's COLORMAP row before distance falloff (0 = brightest); the
    // shader subtracts the depth term from it.
    float light;
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

    // Masked textures (every sprite, and the wall textures with holes in them —
    // grates, windows) carry an alpha channel and are stored four bytes per
    // pixel: the palette index in red, 0 or 255 in alpha. Everything else is
    // one byte per pixel, and the shader reads its alpha as an implicit 1.
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
    float light;
    int flip;
} EacpDoomHudSprite;

typedef struct
{
    float x, y;
    float z;
    float angle;
} EacpDoomCamera;

// One automap vertex: a corner of the quad a map line is drawn as, in the
// engine's own 320 x 168 frame space (origin top-left), carrying a palette
// index - the automap picks raw colours rather than texturing anything.
//
// The quad is not widened here. Each corner carries the line's endpoint, the
// line's direction and which side of it the corner lies on, and the vertex
// shader offsets it perpendicular by half the line width. Widening it here would
// need the line's length, and the only square root in this translation unit is
// eacpSqrt, which is Newton's method (the engine links no libm) - fine for the
// once-per-level work it was written for, and not something to run over every
// line of the map every frame. The GPU normalizes for nothing.
typedef struct
{
    float position[2];
    float direction[2];
    float side;
    float color;
} EacpDoomAutomapVertex;

// The automap's frame, which its geometry is emitted in: the 3D view's rect,
// with the status bar below it.
#define EACP_DOOM_AUTOMAP_WIDTH 320
#define EACP_DOOM_AUTOMAP_HEIGHT 168

#ifdef __cplusplus
extern "C"
{
#endif

    // True when the GPU renderer owns the view area rather than the software
    // frame: the game is in a level and is not mid screen-melt (the wipe is the
    // one thing still drawn from the software frame, top to bottom).
    //
    // What fills that area is then eacpDoomAutomapActive's business, and what
    // sits on top of it is eacpDoomBuildOverlay's.
    int eacpDoomViewActive(void);

    // True when the automap has replaced the 3D view. The engine skips
    // R_RenderPlayerView entirely while it is up, so the two never coexist.
    int eacpDoomAutomapActive(void);

    // Marks the walls the player can see from where they are standing, so the
    // map keeps filling in while it is being looked at. Call once a tic, after
    // the engine has run it. Does nothing unless the automap is up.
    //
    // A line is revealed as a *side effect of being drawn*: R_StoreWallRange
    // sets ML_MAPPED as the software renderer lays the wall down. But D_Display
    // skips the render entirely while the automap is up, so vanilla's map stops
    // filling in at the moment you open it, and only catches up once you close
    // it again. This is a deliberate departure from vanilla, and the only one in
    // the port's behaviour.
    //
    // It walks the BSP the way R_RenderPlayerView would, and no further: the
    // planes and the sprites are never drawn, and the four NetUpdate calls it
    // makes on the way are not wanted here - they drain the event queue, which
    // is not this function's business.
    void eacpDoomRevealAutomap(void);

    // The COLORMAP row the view is remapped through before it reaches the
    // palette: 0 while playing, and the row the menu darkens its background
    // with while the menu is up (DOOM_FLAG_MENU_DARKEN_BG).
    //
    // Row 0 is the identity map, so applying it unconditionally costs one
    // lookup and changes nothing. (It is not *quite* the identity on the index
    // - it folds the palette's seven duplicate entries onto their twins - but
    // it is exactly the identity on the colour those indices resolve to.)
    //
    // The status bar needs none of this: the engine darkens its own framebuffer
    // wholesale, so the strip the renderer samples from it is already dark.
    int eacpDoomDarkenRow(void);

    // The layers the engine draws on top of the view in software and nothing
    // else reproduces: HUD messages, the PAUSE graphic, the menu, and the
    // automap's marks. Writes EACP_DOOM_SCREEN_WIDTH x EACP_DOOM_SCREEN_HEIGHT
    // RGBA pixels - palette index in red, whether the pixel darkens with the
    // view in green, coverage in alpha - and returns whether any pixel was
    // covered at all.
    //
    // They are captured as two layers, because the menu darkens the frame it
    // finds and then draws itself over it: a message, the level's name and the
    // PAUSE graphic are already on the screen by then and dim with the world,
    // while the menu itself stays bright. Green is what tells the two apart.
    //
    // The engine offers no way to draw these anywhere but over the frame it has
    // just rendered, so they are captured instead: screens[0] is pointed at
    // scratch, the overlay drawers alone are run, and the real frame is put
    // back. Coverage cannot be read off a single pass, because a pixel the menu
    // legitimately drew may hold whatever value the scratch was primed with, so
    // the drawers run twice over two differently-primed buffers and a pixel
    // counts as covered exactly where the two agree. They are pure - the skull
    // blinks on M_Ticker, not M_Drawer - so wherever anything was drawn, they
    // agree by construction.
    //
    // The menu's background darkening is left out of the capture (it would touch
    // every pixel, and so read as total coverage); eacpDoomDarkenRow applies it
    // to the GPU view instead, which is both exact and what keeps the world
    // behind the menu at full resolution.
    int eacpDoomBuildOverlay(unsigned char* outRgba);

    // Re-applies the key bindings the app asked for with doom_set_default_int.
    // Call once, after doom_init.
    //
    // DOOM has no way to rebind a key from inside the game, yet it still writes
    // every binding out to ~/.doomrc and, at startup, reads them back *over* the
    // defaults the app just set. A config left behind by an older build
    // therefore pins that build's keys for good and the app's own choice
    // silently does nothing - which is how "use" could be set to Enter here and
    // still be E in the running game. The app owns the keys, so they are applied
    // again once the config has been read. Everything the player can genuinely
    // change from the menu - mouse sensitivity, screen size, volumes - is left
    // alone and still persists.
    void eacpDoomBindKeys(void);

    // The engine's own clock, in tics - 35 a second - with the fraction: its
    // whole part says which tic the world is on, and the rest says how far into
    // that tic time has moved.
    //
    // Both answers must come from ONE reading of it. Ask for the tic and the
    // fraction separately and a tic boundary can fall between the two asks: the
    // fraction wraps back to nothing while the state it is meant to be placed
    // between is still the previous tic's, and the frame is drawn a whole tic in
    // the past - a jump backwards, and then a jump forwards to recover.
    //
    // (It counts from the epoch, not from when the game started, so it is far
    // larger than the engine's own tic number - only its steps and its fraction
    // are meaningful, and both agree with the engine's.)
    double eacpDoomTicTime(void);

    int eacpDoomIsWiping(void);

    // The screen melt, while one is running: the frame it is sliding *away*, as
    // palette indices, and how far down each column of it has moved so far.
    //
    // The frame coming *in* is neither wanted nor asked for, and that is what
    // lets the melt run with no offscreen render target. Vanilla only ever reads
    // the outgoing frame; what it composites is
    //
    //     column c, row r = the outgoing frame's row (r - offset[c]) when
    //                       r >= offset[c], and the incoming frame's row r
    //                       when it is not,
    //
    // so "the incoming frame" is just the framebuffer left as it is - the level
    // the renderer has already drawn there, at the window's resolution. Only the
    // outgoing frame has to become a texture, and it is a 320x200 software frame
    // whatever happens: the intermission or title screen it actually is.
    //
    // Writes EACP_DOOM_SCREEN_WIDTH x EACP_DOOM_SCREEN_HEIGHT indices and
    // EACP_DOOM_WIPE_COLUMNS offsets in rows, and returns whether a melt is
    // running. On the first frame of one it is not yet: the engine raises its
    // wiping flag at the end of the frame that renders the incoming screen and
    // only sets the melt up on the next, so that frame comes back with the
    // outgoing screen whole and nothing slid, which is exactly what should still
    // be on the screen.
    int eacpDoomBuildWipe(unsigned char* outStart, unsigned char* outOffsets);

    // The engine's mouse sensitivity (the options menu changes it). It scales
    // the movement it is handed by (sensitivity + 5) / 10, so a view that runs
    // ahead of the engine has to scale by the same amount or the two disagree.
    int eacpDoomMouseSensitivity(void);

    EacpDoomCamera eacpDoomGetCamera(void);

    // Every wall texture, flat and sprite the game loaded, in one id space and
    // in that order. Stable for the whole run (the WAD's graphics are loaded
    // once), so the renderer can upload each one lazily and keep it. Zero until
    // the engine has initialised.
    int eacpDoomGetTextureCount(void);
    EacpDoomTextureInfo eacpDoomGetTextureInfo(int id);

    // Fills out with the texture's pixels, row-major, top row first: one byte
    // per pixel, or four when the texture is masked (see EacpDoomTextureInfo).
    void eacpDoomGetTexturePixels(int id, unsigned char* out);

    // Fills out with EACP_DOOM_COLORMAP_ROWS rows of 256 palette indices: row r
    // maps a palette index to its appearance at light level r.
    void eacpDoomGetColormaps(unsigned char* out);

    // Builds this frame's world geometry - textured walls, floors, ceilings,
    // the sky and every thing in the level as a camera-facing billboard, read
    // fresh from the live level so moving sectors, animated textures and moving
    // monsters are always current - and groups it into per-texture draw runs.
    // Writes the vertex count through outVertexCount and returns the draw
    // count.
    //
    // `camera` is where the frame is drawn from, which between tics is not
    // quite where the engine has the player: the billboards have to square up
    // to the view actually being drawn, or they sit half edge-on and flicker as
    // it turns, and the sky has to be centred on it.
    //
    // `alpha` is how far into the current tic the frame sits (see
    // eacpDoomTicFraction). Everything that moves on the tic — the things in
    // the level, and the floors and ceilings a door or a lift is driving — is
    // placed at that point between where it was and where it is, so it glides
    // with the camera instead of stepping 35 times a second against it.
    int eacpDoomBuildGeometry(const EacpDoomCamera* camera,
                              float alpha,
                              EacpDoomVertex* vertices,
                              int maxVertices,
                              EacpDoomDraw* draws,
                              int maxDraws,
                              int* outVertexCount);

    // Builds the automap as triangles in its 320 x 168 frame: the level's walls
    // in the colours the automap picks for them, the grid, the player's arrow,
    // the things (with the map cheat on) and the crosshair. Returns the vertex
    // count.
    //
    // Vanilla walks the same lines with a Bresenham rasterizer straight into
    // the 320 x 168 frame, so its map is stepped both in space - it snaps the
    // view to whole map pixels - and in time. Neither is reproduced here: the
    // lines carry their real endpoints, and `camera` recentres the map on the
    // interpolated view rather than on the player's last tic, so panning and
    // turning glide at the display's rate the way the 3D view does. Zooming and
    // manual panning still step, being the engine's own per-tic quantities.
    int eacpDoomBuildAutomap(const EacpDoomCamera* camera,
                             EacpDoomAutomapVertex* vertices,
                             int maxVertices);

    // Remembers where every sector's floor and ceiling are, so the next frame
    // can draw them part-way to wherever they move next. Call once per tic,
    // just after the engine has run it.
    void eacpDoomSnapshotTic(void);

    // The player's weapon and muzzle flash, drawn over the world in screen
    // space. Always writes EACP_DOOM_HUD_SPRITES entries, one per slot, so a
    // caller can match a slot against the one it saw last tic and interpolate
    // between them.
    void eacpDoomGetHudSprites(EacpDoomHudSprite* out);

#ifdef __cplusplus
}
#endif
