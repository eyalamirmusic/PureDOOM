// Included at the end of DoomImpl.c, inside the engine translation unit, so
// the functions below can use the engine's types and globals directly. Never
// compiled standalone.

#include "EngineAccess.h"

// Convex subsector cells rarely exceed a handful of corners; the cap only
// bounds the clipper's scratch space.
#define EACP_MAX_POLY_VERTICES 32

// Larger than any DOOM map, so the initial square the BSP clips down is
// effectively unbounded.
#define EACP_MAP_LIMIT 32768.0

#define EACP_CLIP_EPSILON 1e-6

#define EACP_FLAT_SIZE 64

typedef struct
{
    double x, y;
} EacpPoint;

typedef struct
{
    // Null while counting: the first pass sizes each texture's run, the second
    // writes into it.
    EacpDoomVertex* vertices;
    int* counts;
    int* cursors;
} EacpEmitter;

static float* eacpPolyVertices = 0;
static int* eacpPolyStart = 0;
static int* eacpPolyCount = 0;
static int eacpPolyTotal = 0;

static int* eacpTextureCounts = 0;
static int* eacpTextureCursors = 0;
static int eacpScratchTextures = 0;

static void* eacpCachedNodes = 0;
static int eacpCachedSubsectors = -1;
static int eacpCachedEpisode = -1;
static int eacpCachedMap = -1;

static double eacpFixedToDouble(fixed_t value)
{
    return (double) value / (double) FRACUNIT;
}

static float eacpFixedToFloat(fixed_t value)
{
    return (float) value / (float) FRACUNIT;
}

// The engine deliberately links no libm, so the one square root the texture
// mapping needs (a wall's length, for its horizontal texture coordinate)
// comes from Newton's method rather than an include.
static double eacpSqrt(double value)
{
    double guess;
    int i;

    if (value <= 0.0)
        return 0.0;

    guess = value;

    for (i = 0; i < 24; ++i)
        guess = 0.5 * (guess + value / guess);

    return guess;
}

int eacpDoomWorldVisible(void)
{
    return gamestate == GS_LEVEL && !menuactive && !automapactive
           && !is_wiping_screen;
}

EacpDoomCamera eacpDoomGetCamera(void)
{
    EacpDoomCamera camera = {0, 0, 0, 0};
    player_t* player = &players[displayplayer];

    if (player->mo == 0)
        return camera;

    camera.x = eacpFixedToFloat(player->mo->x);
    camera.y = eacpFixedToFloat(player->mo->y);
    camera.z = eacpFixedToFloat(player->viewz);

    // angle_t maps the full circle onto 32 bits; 2^31 is half a turn.
    camera.angle =
        (float) ((double) player->mo->angle * (3.14159265358979 / 2147483648.0));
    return camera;
}

int eacpDoomGetTextureCount(void)
{
    if (numtextures <= 0 || textures == 0)
        return 0;

    return numtextures + numflats;
}

EacpDoomTextureInfo eacpDoomGetTextureInfo(int id)
{
    EacpDoomTextureInfo info;
    info.width = 0;
    info.height = 0;

    if (id < 0 || id >= eacpDoomGetTextureCount())
        return info;

    if (id < numtextures)
    {
        info.width = textures[id]->width;
        info.height = textures[id]->height;
    }
    else
    {
        info.width = EACP_FLAT_SIZE;
        info.height = EACP_FLAT_SIZE;
    }

    return info;
}

void eacpDoomGetTexturePixels(int id, unsigned char* out)
{
    if (out == 0 || id < 0 || id >= eacpDoomGetTextureCount())
        return;

    if (id < numtextures)
    {
        // The engine stores composed textures column-major (and composes them
        // on demand); transpose each column into the row-major image a GPU
        // texture wants.
        int width = textures[id]->width;
        int height = textures[id]->height;
        int x, y;

        for (x = 0; x < width; ++x)
        {
            byte* column = R_GetColumn(id, x);

            for (y = 0; y < height; ++y)
                out[y * width + x] = column[y];
        }
    }
    else
    {
        byte* flat = (byte*) W_CacheLumpNum(firstflat + (id - numtextures),
                                            PU_CACHE);
        doom_memcpy(out, flat, EACP_FLAT_SIZE * EACP_FLAT_SIZE);
    }
}

void eacpDoomGetColormaps(unsigned char* out)
{
    if (out == 0 || colormaps == 0)
        return;

    doom_memcpy(out, colormaps, 256 * EACP_DOOM_COLORMAP_ROWS);
}

// The COLORMAP row a surface starts at, before distance darkens it further:
// the engine's light level scaled into the 32 maps, offset by the sector's
// brightness and the fake contrast walls get for their orientation.
static float eacpStartMap(int lightlevel, int contrast)
{
    int lightnum = (lightlevel >> LIGHTSEGSHIFT) + extralight + contrast;

    if (lightnum < 0)
        lightnum = 0;

    if (lightnum >= LIGHTLEVELS)
        lightnum = LIGHTLEVELS - 1;

    return (float) ((LIGHTLEVELS - 1 - lightnum) * 2 * NUMCOLORMAPS
                    / LIGHTLEVELS);
}

// Sutherland-Hodgman against one half-plane. The side test matches the
// engine's own R_PointOnSide: a point is in front of a directed line when
// dy * (px - x) - dx * (py - y) is positive.
static void eacpClipToLine(const EacpPoint* in,
                           int inCount,
                           double x,
                           double y,
                           double dx,
                           double dy,
                           int keepFront,
                           EacpPoint* out,
                           int* outCount)
{
    int i;
    int count = 0;

    for (i = 0; i < inCount; ++i)
    {
        const EacpPoint* a = &in[i];
        const EacpPoint* b = &in[(i + 1) % inCount];

        double sideA = dy * (a->x - x) - dx * (a->y - y);
        double sideB = dy * (b->x - x) - dx * (b->y - y);

        if (!keepFront)
        {
            sideA = -sideA;
            sideB = -sideB;
        }

        if (sideA >= -EACP_CLIP_EPSILON && count < EACP_MAX_POLY_VERTICES)
            out[count++] = *a;

        if (((sideA > EACP_CLIP_EPSILON) && (sideB < -EACP_CLIP_EPSILON))
            || ((sideA < -EACP_CLIP_EPSILON) && (sideB > EACP_CLIP_EPSILON)))
        {
            double t = sideA / (sideA - sideB);

            if (count < EACP_MAX_POLY_VERTICES)
            {
                out[count].x = a->x + t * (b->x - a->x);
                out[count].y = a->y + t * (b->y - a->y);
                ++count;
            }
        }
    }

    *outCount = count;
}

// A subsector's cell, trimmed to the sector boundary: the segs bound it on the
// sides that came from real linedefs, and the interior always lies in front of
// them.
static void eacpStoreSubsector(int index, const EacpPoint* poly, int count)
{
    EacpPoint current[EACP_MAX_POLY_VERTICES];
    EacpPoint clipped[EACP_MAX_POLY_VERTICES];
    int currentCount = count;
    int clippedCount;
    int i;

    for (i = 0; i < count; ++i)
        current[i] = poly[i];

    for (i = 0; i < subsectors[index].numlines && currentCount >= 3; ++i)
    {
        seg_t* seg = &segs[subsectors[index].firstline + i];

        double x = eacpFixedToDouble(seg->v1->x);
        double y = eacpFixedToDouble(seg->v1->y);
        double dx = eacpFixedToDouble(seg->v2->x) - x;
        double dy = eacpFixedToDouble(seg->v2->y) - y;

        eacpClipToLine(current, currentCount, x, y, dx, dy, 1, clipped,
                       &clippedCount);

        for (currentCount = 0; currentCount < clippedCount; ++currentCount)
            current[currentCount] = clipped[currentCount];
    }

    if (currentCount < 3)
        return;

    eacpPolyStart[index] = eacpPolyTotal;
    eacpPolyCount[index] = currentCount;

    for (i = 0; i < currentCount; ++i)
    {
        eacpPolyVertices[(eacpPolyTotal + i) * 2 + 0] = (float) current[i].x;
        eacpPolyVertices[(eacpPolyTotal + i) * 2 + 1] = (float) current[i].y;
    }

    eacpPolyTotal += currentCount;
}

// Vanilla nodes carry no polygons - only the split planes - so each
// subsector's shape is recovered by carrying a huge square down the BSP and
// clipping it by every partition on the way to the leaf.
static void eacpDescend(int nodenum, const EacpPoint* poly, int count)
{
    EacpPoint clipped[EACP_MAX_POLY_VERTICES];
    int clippedCount;
    node_t* node;
    double x, y, dx, dy;

    if (count < 3)
        return;

    if (nodenum & NF_SUBSECTOR)
    {
        eacpStoreSubsector(nodenum & ~NF_SUBSECTOR, poly, count);
        return;
    }

    node = &nodes[nodenum];
    x = eacpFixedToDouble(node->x);
    y = eacpFixedToDouble(node->y);
    dx = eacpFixedToDouble(node->dx);
    dy = eacpFixedToDouble(node->dy);

    eacpClipToLine(poly, count, x, y, dx, dy, 1, clipped, &clippedCount);
    eacpDescend(node->children[0], clipped, clippedCount);

    eacpClipToLine(poly, count, x, y, dx, dy, 0, clipped, &clippedCount);
    eacpDescend(node->children[1], clipped, clippedCount);
}

static int eacpEnsurePolyStorage(void)
{
    doom_free(eacpPolyVertices);
    doom_free(eacpPolyStart);
    doom_free(eacpPolyCount);

    eacpPolyVertices = (float*) doom_malloc(numsubsectors * EACP_MAX_POLY_VERTICES
                                            * 2 * (int) sizeof(float));
    eacpPolyStart = (int*) doom_malloc(numsubsectors * (int) sizeof(int));
    eacpPolyCount = (int*) doom_malloc(numsubsectors * (int) sizeof(int));

    return eacpPolyVertices != 0 && eacpPolyStart != 0 && eacpPolyCount != 0;
}

// The BSP is static for a level, so the cells are rebuilt only when a new one
// loads; per-frame the geometry pass just re-reads the (moving) heights.
static void eacpEnsureLevel(void)
{
    EacpPoint square[4];
    int i;

    if (nodes == eacpCachedNodes && numsubsectors == eacpCachedSubsectors
        && gameepisode == eacpCachedEpisode && gamemap == eacpCachedMap)
        return;

    eacpCachedNodes = nodes;
    eacpCachedSubsectors = numsubsectors;
    eacpCachedEpisode = gameepisode;
    eacpCachedMap = gamemap;

    eacpPolyTotal = 0;

    if (numsubsectors <= 0 || numnodes <= 0 || nodes == 0)
        return;

    if (!eacpEnsurePolyStorage())
        return;

    for (i = 0; i < numsubsectors; ++i)
    {
        eacpPolyStart[i] = 0;
        eacpPolyCount[i] = 0;
    }

    square[0].x = -EACP_MAP_LIMIT;
    square[0].y = -EACP_MAP_LIMIT;
    square[1].x = EACP_MAP_LIMIT;
    square[1].y = -EACP_MAP_LIMIT;
    square[2].x = EACP_MAP_LIMIT;
    square[2].y = EACP_MAP_LIMIT;
    square[3].x = -EACP_MAP_LIMIT;
    square[3].y = EACP_MAP_LIMIT;

    eacpDescend(numnodes - 1, square, 4);
}

static int eacpEnsureScratch(int textureCount)
{
    if (eacpScratchTextures == textureCount && eacpTextureCounts != 0)
        return 1;

    doom_free(eacpTextureCounts);
    doom_free(eacpTextureCursors);

    eacpTextureCounts = (int*) doom_malloc(textureCount * (int) sizeof(int));
    eacpTextureCursors = (int*) doom_malloc(textureCount * (int) sizeof(int));
    eacpScratchTextures = textureCount;

    return eacpTextureCounts != 0 && eacpTextureCursors != 0;
}

static void eacpEmitVertex(EacpEmitter* em,
                           int textureId,
                           float x,
                           float y,
                           float z,
                           float u,
                           float v,
                           float light)
{
    EacpDoomVertex* out;

    if (em->vertices == 0)
    {
        em->counts[textureId]++;
        return;
    }

    if (em->cursors[textureId] < 0)
        return;

    out = &em->vertices[em->cursors[textureId]++];
    out->position[0] = x;
    out->position[1] = y;
    out->position[2] = z;
    out->uv[0] = u;
    out->uv[1] = v;
    out->light = light;
}

// DOOM's map plane is (x, y) with z up; the renderer's is (x, up, -y).
static void eacpEmitWallQuad(EacpEmitter* em,
                             int textureId,
                             double x1,
                             double y1,
                             double x2,
                             double y2,
                             float bottom,
                             float top,
                             float textureTop,
                             float uStart,
                             float uEnd,
                             float textureHeight,
                             float light)
{
    float u1 = uStart;
    float u2 = uEnd;
    float vTop = (textureTop - top) / textureHeight;
    float vBottom = (textureTop - bottom) / textureHeight;

    float ax = (float) x1;
    float az = (float) -y1;
    float bx = (float) x2;
    float bz = (float) -y2;

    if (top <= bottom)
        return;

    eacpEmitVertex(em, textureId, ax, bottom, az, u1, vBottom, light);
    eacpEmitVertex(em, textureId, bx, bottom, bz, u2, vBottom, light);
    eacpEmitVertex(em, textureId, bx, top, bz, u2, vTop, light);

    eacpEmitVertex(em, textureId, ax, bottom, az, u1, vBottom, light);
    eacpEmitVertex(em, textureId, bx, top, bz, u2, vTop, light);
    eacpEmitVertex(em, textureId, ax, top, az, u1, vTop, light);
}

static void eacpEmitLineSide(EacpEmitter* em, line_t* line, int s)
{
    side_t* side;
    sector_t* front;
    sector_t* back;
    vertex_t* v1;
    vertex_t* v2;
    double x1, y1, x2, y2, dx, dy, length;
    float frontFloor, frontCeiling;
    float uStart, uEnd, rowOffset, light;
    int contrast;

    if (line->sidenum[s] < 0)
        return;

    side = &sides[line->sidenum[s]];
    front = side->sector;
    back = line->sidenum[s ^ 1] >= 0 ? sides[line->sidenum[s ^ 1]].sector : 0;

    v1 = s == 0 ? line->v1 : line->v2;
    v2 = s == 0 ? line->v2 : line->v1;

    x1 = eacpFixedToDouble(v1->x);
    y1 = eacpFixedToDouble(v1->y);
    x2 = eacpFixedToDouble(v2->x);
    y2 = eacpFixedToDouble(v2->y);
    dx = x2 - x1;
    dy = y2 - y1;
    length = eacpSqrt(dx * dx + dy * dy);

    frontFloor = eacpFixedToFloat(front->floorheight);
    frontCeiling = eacpFixedToFloat(front->ceilingheight);
    rowOffset = eacpFixedToFloat(side->rowoffset);

    // The software renderer's fake contrast: walls running east-west are a
    // step darker, north-south a step brighter, so corners stay readable.
    contrast = 0;

    if (line->v1->y == line->v2->y)
        contrast = -1;
    else if (line->v1->x == line->v2->x)
        contrast = 1;

    light = eacpStartMap(front->lightlevel, contrast);

    if (back == 0)
    {
        int texture = texturetranslation[side->midtexture];
        float textureWidth, textureHeight, textureTop;

        if (side->midtexture <= 0)
            return;

        textureWidth = (float) textures[texture]->width;
        textureHeight = (float) textures[texture]->height;

        textureTop = (line->flags & ML_DONTPEGBOTTOM)
                         ? frontFloor + textureHeight
                         : frontCeiling;
        textureTop += rowOffset;

        uStart = eacpFixedToFloat(side->textureoffset) / textureWidth;
        uEnd = uStart + (float) length / textureWidth;

        eacpEmitWallQuad(em, texture, x1, y1, x2, y2, frontFloor, frontCeiling,
                         textureTop, uStart, uEnd, textureHeight, light);
        return;
    }

    {
        float backFloor = eacpFixedToFloat(back->floorheight);
        float backCeiling = eacpFixedToFloat(back->ceilingheight);

        if (side->bottomtexture > 0 && backFloor > frontFloor)
        {
            int texture = texturetranslation[side->bottomtexture];
            float textureWidth = (float) textures[texture]->width;
            float textureHeight = (float) textures[texture]->height;

            float textureTop =
                (line->flags & ML_DONTPEGBOTTOM) ? frontCeiling : backFloor;
            textureTop += rowOffset;

            uStart = eacpFixedToFloat(side->textureoffset) / textureWidth;
            uEnd = uStart + (float) length / textureWidth;

            eacpEmitWallQuad(em, texture, x1, y1, x2, y2, frontFloor, backFloor,
                             textureTop, uStart, uEnd, textureHeight, light);
        }

        // Between two sky ceilings the step is invisible sky (the classic sky
        // hack), not an upper wall.
        if (side->toptexture > 0 && backCeiling < frontCeiling
            && !(front->ceilingpic == skyflatnum
                 && back->ceilingpic == skyflatnum))
        {
            int texture = texturetranslation[side->toptexture];
            float textureWidth = (float) textures[texture]->width;
            float textureHeight = (float) textures[texture]->height;

            float textureTop = (line->flags & ML_DONTPEGTOP)
                                   ? frontCeiling
                                   : backCeiling + textureHeight;
            textureTop += rowOffset;

            uStart = eacpFixedToFloat(side->textureoffset) / textureWidth;
            uEnd = uStart + (float) length / textureWidth;

            eacpEmitWallQuad(em, texture, x1, y1, x2, y2, backCeiling,
                             frontCeiling, textureTop, uStart, uEnd,
                             textureHeight, light);
        }
    }
}

static void eacpEmitFlat(EacpEmitter* em,
                         int index,
                         int flat,
                         float height,
                         float light)
{
    int textureId = numtextures + flattranslation[flat];
    const float* poly = &eacpPolyVertices[eacpPolyStart[index] * 2];
    int count = eacpPolyCount[index];
    int i;

    for (i = 1; i + 1 < count; ++i)
    {
        int corners[3];
        int c;

        corners[0] = 0;
        corners[1] = i;
        corners[2] = i + 1;

        for (c = 0; c < 3; ++c)
        {
            float x = poly[corners[c] * 2 + 0];
            float y = poly[corners[c] * 2 + 1];

            eacpEmitVertex(em, textureId, x, height, -y,
                           x / (float) EACP_FLAT_SIZE,
                           -y / (float) EACP_FLAT_SIZE, light);
        }
    }
}

static void eacpEmitSubsector(EacpEmitter* em, int index)
{
    sector_t* sector = subsectors[index].sector;
    float light;

    if (eacpPolyCount[index] < 3 || sector == 0)
        return;

    light = eacpStartMap(sector->lightlevel, 0);

    eacpEmitFlat(em, index, sector->floorpic,
                 eacpFixedToFloat(sector->floorheight), light);

    // A sky ceiling is a hole onto the sky, not a surface.
    if (sector->ceilingpic != skyflatnum)
        eacpEmitFlat(em, index, sector->ceilingpic,
                     eacpFixedToFloat(sector->ceilingheight), light);
}

static void eacpEmitWorld(EacpEmitter* em)
{
    int i;

    for (i = 0; i < numlines; ++i)
    {
        eacpEmitLineSide(em, &lines[i], 0);
        eacpEmitLineSide(em, &lines[i], 1);
    }

    for (i = 0; i < numsubsectors; ++i)
        eacpEmitSubsector(em, i);
}

int eacpDoomBuildGeometry(EacpDoomVertex* vertices,
                          int maxVertices,
                          EacpDoomDraw* draws,
                          int maxDraws,
                          int* outVertexCount)
{
    EacpEmitter em;
    int textureCount = eacpDoomGetTextureCount();
    int total = 0;
    int drawCount = 0;
    int i;

    if (outVertexCount != 0)
        *outVertexCount = 0;

    if (gamestate != GS_LEVEL || vertices == 0 || draws == 0 || textureCount <= 0
        || lines == 0)
        return 0;

    eacpEnsureLevel();

    if (eacpPolyTotal <= 0 || !eacpEnsureScratch(textureCount))
        return 0;

    for (i = 0; i < textureCount; ++i)
        eacpTextureCounts[i] = 0;

    em.counts = eacpTextureCounts;
    em.cursors = eacpTextureCursors;
    em.vertices = 0;
    eacpEmitWorld(&em);

    // Each texture's vertices become one contiguous run, so the frame draws
    // once per texture with no state changes in between.
    for (i = 0; i < textureCount; ++i)
    {
        int count = eacpTextureCounts[i];

        if (count <= 0 || drawCount >= maxDraws || total + count > maxVertices)
        {
            eacpTextureCursors[i] = -1;
            continue;
        }

        eacpTextureCursors[i] = total;

        draws[drawCount].textureId = i;
        draws[drawCount].firstVertex = total;
        draws[drawCount].vertexCount = count;
        ++drawCount;

        total += count;
    }

    em.vertices = vertices;
    eacpEmitWorld(&em);

    if (outVertexCount != 0)
        *outVertexCount = total;

    return drawCount;
}
