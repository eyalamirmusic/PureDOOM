#pragma once

// Plain-C snapshot interface between the DOOM engine internals and the eacp
// renderer. Implemented in EngineAccess.c, which DoomImpl.c includes at the
// end of the engine translation unit so the implementation can reach the
// engine's (non-static) types and globals; nothing DOOM-typed leaks out.

// The COLORMAP rows the renderer uses: 32 progressively darker remaps of the
// palette. (The lump carries two more - the invulnerability inverse map and a
// spare - which the light calculation never selects.)
#define EACP_DOOM_COLORMAP_ROWS 32

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

#ifdef __cplusplus
extern "C"
{
#endif

    // True while gameplay (or a demo) renders the full 3D view with nothing
    // software-only on top - no menu, automap or screen-melt wipe - i.e. when
    // the GPU world view can stand in for the software one.
    int eacpDoomWorldVisible(void);

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
