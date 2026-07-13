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
// origin.
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

    // The engine's own clock, in tics (it runs at 35 of them a second).
    //
    // This matters because doom_update() BLOCKS until a tic is due: TryRunTics
    // ends in a spin loop waiting for one, and always asks for at least one. On
    // a display refresh with no tic to run it would sit there for most of a
    // tic, which on the display link's thread means the whole app - input
    // included - stalls with it. Call doom_update() only when this value has
    // moved, or while a screen wipe is running.
    int eacpDoomTicCount(void);
    int eacpDoomIsWiping(void);

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
    int eacpDoomBuildGeometry(EacpDoomVertex* vertices,
                              int maxVertices,
                              EacpDoomDraw* draws,
                              int maxDraws,
                              int* outVertexCount);

    // The player's weapon and muzzle flash, drawn over the world in screen
    // space. Returns the number written (at most 2).
    int eacpDoomGetHudSprites(EacpDoomHudSprite* out, int maxSprites);

#ifdef __cplusplus
}
#endif
