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
} EacpDoomTextureInfo;

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

    EacpDoomCamera eacpDoomGetCamera(void);

    // Every wall texture and flat the game loaded, in one id space: ids below
    // the wall-texture count are composed wall textures, the rest are flats.
    // Stable for the whole run (the WAD's texture set is loaded once), so the
    // renderer uploads them once. Zero until the engine has initialised.
    int eacpDoomGetTextureCount(void);
    EacpDoomTextureInfo eacpDoomGetTextureInfo(int id);

    // Fills out with width * height palette indices, row-major, top row first.
    void eacpDoomGetTexturePixels(int id, unsigned char* out);

    // Fills out with EACP_DOOM_COLORMAP_ROWS rows of 256 palette indices: row r
    // maps a palette index to its appearance at light level r.
    void eacpDoomGetColormaps(unsigned char* out);

    // Builds this frame's world geometry - textured walls, floors and ceilings
    // read fresh from the live level, so moving sectors and animated textures
    // are always current - and groups it into per-texture draw runs. Writes the
    // vertex count through outVertexCount and returns the draw count.
    int eacpDoomBuildGeometry(EacpDoomVertex* vertices,
                              int maxVertices,
                              EacpDoomDraw* draws,
                              int maxDraws,
                              int* outVertexCount);

#ifdef __cplusplus
}
#endif
