#pragma once

// Plain-C snapshot interface between the DOOM engine internals and the eacp
// renderer. Implemented in EngineAccess.c, which DoomImpl.c includes at the
// end of the engine translation unit so the implementation can reach the
// engine's (non-static) types and globals; nothing DOOM-typed leaks out.

typedef struct
{
    float x1, y1, x2, y2;
    float bottom, top;
    float light;
} EacpDoomWall;

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

    // Fills out with up to maxWalls solid wall quads (one-sided middles plus the
    // upper and lower steps of two-sided lines) from the current level, read
    // fresh each call so moving floors, ceilings and doors are always current.
    // Returns the number written.
    int eacpDoomGetWalls(EacpDoomWall* out, int maxWalls);

#ifdef __cplusplus
}
#endif
