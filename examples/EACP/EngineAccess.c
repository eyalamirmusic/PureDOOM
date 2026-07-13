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

// The sky is a cylinder around the camera, far enough out that every wall in a
// DOOM level stands in front of it, and tall enough to fill the view.
#define EACP_SKY_SEGMENTS 64
#define EACP_SKY_RADIUS 12000.0f
#define EACP_SKY_HEIGHT 9000.0f

// The view's focal length in rows: half of the 168-row view divided by the
// tangent of half its vertical field of view. It converts a height on the sky
// cylinder into the screen row DOOM would have drawn it at.
#define EACP_SKY_FOCAL 133.33f

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

// A line's length never changes, and the geometry pass needs it for every wall
// it lays a texture along, so it is measured once when the level loads rather
// than thousands of times a frame.
static float* eacpLineLengths = 0;

static int* eacpTextureCounts = 0;
static int* eacpTextureCursors = 0;
static int eacpScratchTextures = 0;

static void* eacpCachedNodes = 0;
static int eacpCachedSubsectors = -1;
static int eacpCachedEpisode = -1;
static int eacpCachedMap = -1;

static int eacpTexturesReady = 0;
static unsigned char* eacpWallMasked = 0;
static short* eacpSpriteHeights = 0;

// Where every sector's floor and ceiling stood at the last tic, so a door or a
// lift can be drawn part-way to where it is going rather than jumping there.
static float* eacpPreviousFloor = 0;
static float* eacpPreviousCeiling = 0;
static int eacpSnapshotSectors = 0;

// How far into the current tic the frame being built sits.
static float eacpAlpha = 0.0f;

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

static float eacpMix(float from, float to, float amount)
{
    return from + (to - from) * amount;
}

// A sector's floor and ceiling where they are *now*, part-way through whatever
// move a door or a lift has them making, rather than where the last tic left
// them. The walls that meet them are drawn from the same numbers, so nothing
// tears.
static float eacpFloorHeight(sector_t* sector)
{
    float now = eacpFixedToFloat(sector->floorheight);
    int index = (int) (sector - sectors);

    if (eacpPreviousFloor == 0 || index < 0 || index >= eacpSnapshotSectors)
        return now;

    return eacpMix(eacpPreviousFloor[index], now, eacpAlpha);
}

static float eacpCeilingHeight(sector_t* sector)
{
    float now = eacpFixedToFloat(sector->ceilingheight);
    int index = (int) (sector - sectors);

    if (eacpPreviousCeiling == 0 || index < 0 || index >= eacpSnapshotSectors)
        return now;

    return eacpMix(eacpPreviousCeiling[index], now, eacpAlpha);
}

void eacpDoomSnapshotTic(void)
{
    int i;

    if (gamestate != GS_LEVEL || sectors == 0 || numsectors <= 0)
        return;

    if (eacpSnapshotSectors != numsectors)
    {
        doom_free(eacpPreviousFloor);
        doom_free(eacpPreviousCeiling);

        eacpPreviousFloor = (float*) doom_malloc(numsectors * (int) sizeof(float));
        eacpPreviousCeiling =
            (float*) doom_malloc(numsectors * (int) sizeof(float));
        eacpSnapshotSectors = numsectors;

        if (eacpPreviousFloor == 0 || eacpPreviousCeiling == 0)
        {
            eacpSnapshotSectors = 0;
            return;
        }
    }

    for (i = 0; i < numsectors; ++i)
    {
        eacpPreviousFloor[i] = eacpFixedToFloat(sectors[i].floorheight);
        eacpPreviousCeiling[i] = eacpFixedToFloat(sectors[i].ceilingheight);
    }
}

int eacpDoomWorldVisible(void)
{
    return gamestate == GS_LEVEL && !menuactive && !automapactive
           && !is_wiping_screen;
}

void eacpDoomBindKeys(void)
{
    int count = (int)(sizeof(defaults) / sizeof(defaults[0]));
    int i;

    for (i = 0; i < count; i++)
    {
        default_t* entry = &defaults[i];

        if (entry->defaultvalue != STRING_VALUE
            && doom_strncmp(entry->name, "key_", 4) == 0)
        {
            *entry->location = entry->defaultvalue;
        }
    }
}

double eacpDoomTicTime(void)
{
    int sec, usec;

    doom_gettime(&sec, &usec);

    // I_GetTime's own expression, kept fractional instead of truncated, so this
    // steps from one tic to the next at exactly the moment the engine does. It
    // omits the engine's private start-of-run offset, which only shifts the
    // count by a whole number of tics and so changes neither the steps nor the
    // fraction.
    return (double) sec * TICRATE + (double) usec * TICRATE / 1000000.0;
}

int eacpDoomIsWiping(void)
{
    return is_wiping_screen ? 1 : 0;
}

int eacpDoomMouseSensitivity(void)
{
    return mouseSensitivity;
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

static int eacpSpriteBase(void)
{
    return numtextures + numflats;
}

// Draws a patch's posts into an index image and its coverage into an alpha
// image. This is how DOOM stores every graphic: runs of pixels down a column,
// with the gaps between the runs left transparent. Composing from the patches
// (as R_GenerateComposite does) rather than reading the engine's cached columns
// is what makes a masked texture's holes come out as holes.
static void eacpBlitPatch(patch_t* patch,
                          int originX,
                          int originY,
                          unsigned char* indices,
                          unsigned char* alpha,
                          int width,
                          int height)
{
    int x;

    for (x = 0; x < patch->width; ++x)
    {
        int destX = originX + x;
        column_t* column;

        if (destX < 0 || destX >= width)
            continue;

        column = (column_t*) ((byte*) patch + patch->columnofs[x]);

        while (column->topdelta != 0xff)
        {
            byte* source = (byte*) column + 3;
            int count = column->length;
            int i;

            for (i = 0; i < count; ++i)
            {
                int destY = originY + column->topdelta + i;

                if (destY < 0 || destY >= height)
                    continue;

                indices[destY * width + destX] = source[i];
                alpha[destY * width + destX] = 255;
            }

            column = (column_t*) ((byte*) column + column->length + 4);
        }
    }
}

static void eacpDecodeWall(int id,
                           unsigned char* indices,
                           unsigned char* alpha,
                           int width,
                           int height)
{
    texture_t* texture = textures[id];
    int i;

    doom_memset(indices, 0, width * height);
    doom_memset(alpha, 0, width * height);

    for (i = 0; i < texture->patchcount; ++i)
    {
        texpatch_t* piece = &texture->patches[i];
        patch_t* patch = (patch_t*) W_CacheLumpNum(piece->patch, PU_CACHE);

        eacpBlitPatch(
            patch, piece->originx, piece->originy, indices, alpha, width, height);
    }
}

static int eacpWallIsMasked(int id)
{
    int width = textures[id]->width;
    int height = textures[id]->height;
    int count = width * height;
    int masked = 0;
    int i;

    unsigned char* indices = (unsigned char*) doom_malloc(count);
    unsigned char* alpha = (unsigned char*) doom_malloc(count);

    if (indices != 0 && alpha != 0)
    {
        eacpDecodeWall(id, indices, alpha, width, height);

        for (i = 0; i < count; ++i)
            if (alpha[i] == 0)
            {
                masked = 1;
                break;
            }
    }

    doom_free(indices);
    doom_free(alpha);

    return masked;
}

// Which wall textures have holes, and how tall each sprite is: both are known
// only after decoding, and both are wanted before the renderer asks for a
// single pixel.
static void eacpEnsureTextureData(void)
{
    int i;

    if (eacpTexturesReady || numtextures <= 0 || textures == 0)
        return;

    eacpWallMasked = (unsigned char*) doom_malloc(numtextures);
    eacpSpriteHeights = (short*) doom_malloc(numspritelumps * (int) sizeof(short));

    if (eacpWallMasked == 0 || eacpSpriteHeights == 0)
        return;

    for (i = 0; i < numtextures; ++i)
        eacpWallMasked[i] = (unsigned char) eacpWallIsMasked(i);

    for (i = 0; i < numspritelumps; ++i)
    {
        patch_t* patch = (patch_t*) W_CacheLumpNum(firstspritelump + i, PU_CACHE);
        eacpSpriteHeights[i] = patch->height;
    }

    eacpTexturesReady = 1;
}

int eacpDoomGetTextureCount(void)
{
    if (numtextures <= 0 || textures == 0)
        return 0;

    return numtextures + numflats + numspritelumps;
}

EacpDoomTextureInfo eacpDoomGetTextureInfo(int id)
{
    EacpDoomTextureInfo info;
    info.width = 0;
    info.height = 0;
    info.masked = 0;

    eacpEnsureTextureData();

    if (!eacpTexturesReady || id < 0 || id >= eacpDoomGetTextureCount())
        return info;

    if (id < numtextures)
    {
        info.width = textures[id]->width;
        info.height = textures[id]->height;
        info.masked = eacpWallMasked[id];
    }
    else if (id < eacpSpriteBase())
    {
        info.width = EACP_FLAT_SIZE;
        info.height = EACP_FLAT_SIZE;
    }
    else
    {
        int lump = id - eacpSpriteBase();

        info.width = spritewidth[lump] >> FRACBITS;
        info.height = eacpSpriteHeights[lump];
        info.masked = 1;
    }

    return info;
}

void eacpDoomGetTexturePixels(int id, unsigned char* out)
{
    EacpDoomTextureInfo info = eacpDoomGetTextureInfo(id);
    int count = info.width * info.height;
    unsigned char* indices;
    unsigned char* alpha;
    int i;

    if (out == 0 || count <= 0)
        return;

    if (id >= numtextures && id < eacpSpriteBase())
    {
        byte* flat =
            (byte*) W_CacheLumpNum(firstflat + (id - numtextures), PU_CACHE);
        doom_memcpy(out, flat, EACP_FLAT_SIZE * EACP_FLAT_SIZE);
        return;
    }

    indices = (unsigned char*) doom_malloc(count);
    alpha = (unsigned char*) doom_malloc(count);

    if (indices == 0 || alpha == 0)
    {
        doom_free(indices);
        doom_free(alpha);
        return;
    }

    if (id < numtextures)
    {
        eacpDecodeWall(id, indices, alpha, info.width, info.height);
    }
    else
    {
        patch_t* patch = (patch_t*) W_CacheLumpNum(
            firstspritelump + (id - eacpSpriteBase()), PU_CACHE);

        doom_memset(indices, 0, count);
        doom_memset(alpha, 0, count);
        eacpBlitPatch(patch, 0, 0, indices, alpha, info.width, info.height);
    }

    // A masked texture carries its coverage in alpha, so the shader can throw
    // the empty pixels away; everything else is a bare index.
    if (info.masked)
    {
        for (i = 0; i < count; ++i)
        {
            out[i * 4 + 0] = indices[i];
            out[i * 4 + 1] = 0;
            out[i * 4 + 2] = 0;
            out[i * 4 + 3] = alpha[i];
        }
    }
    else
    {
        for (i = 0; i < count; ++i)
            out[i] = indices[i];
    }

    doom_free(indices);
    doom_free(alpha);
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

static void eacpMeasureLines(void)
{
    int i;

    doom_free(eacpLineLengths);
    eacpLineLengths = (float*) doom_malloc(numlines * (int) sizeof(float));

    if (eacpLineLengths == 0)
        return;

    for (i = 0; i < numlines; ++i)
    {
        double dx = eacpFixedToDouble(lines[i].dx);
        double dy = eacpFixedToDouble(lines[i].dy);

        eacpLineLengths[i] = (float) eacpSqrt(dx * dx + dy * dy);
    }
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

    eacpMeasureLines();

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

static void eacpEmitLineSide(EacpEmitter* em, line_t* line, int index, int s)
{
    side_t* side;
    sector_t* front;
    sector_t* back;
    vertex_t* v1;
    vertex_t* v2;
    double x1, y1, x2, y2;
    float length;
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
    length = eacpLineLengths[index];

    frontFloor = eacpFloorHeight(front);
    frontCeiling = eacpCeilingHeight(front);
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
        uEnd = uStart + length / textureWidth;

        eacpEmitWallQuad(em, texture, x1, y1, x2, y2, frontFloor, frontCeiling,
                         textureTop, uStart, uEnd, textureHeight, light);
        return;
    }

    {
        float backFloor = eacpFloorHeight(back);
        float backCeiling = eacpCeilingHeight(back);

        if (side->bottomtexture > 0 && backFloor > frontFloor)
        {
            int texture = texturetranslation[side->bottomtexture];
            float textureWidth = (float) textures[texture]->width;
            float textureHeight = (float) textures[texture]->height;

            float textureTop =
                (line->flags & ML_DONTPEGBOTTOM) ? frontCeiling : backFloor;
            textureTop += rowOffset;

            uStart = eacpFixedToFloat(side->textureoffset) / textureWidth;
            uEnd = uStart + length / textureWidth;

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
            uEnd = uStart + length / textureWidth;

            eacpEmitWallQuad(em, texture, x1, y1, x2, y2, backCeiling,
                             frontCeiling, textureTop, uStart, uEnd,
                             textureHeight, light);
        }

        // The middle texture of a two-sided line - a grate, a window, a hanging
        // vine. It is masked, and unlike the walls above it never tiles: DOOM
        // draws it once, clipped to the opening, which is why a too-short
        // midtexture leaves a gap rather than repeating.
        if (side->midtexture > 0)
        {
            int texture = texturetranslation[side->midtexture];
            float textureWidth = (float) textures[texture]->width;
            float textureHeight = (float) textures[texture]->height;

            float openingBottom = backFloor > frontFloor ? backFloor : frontFloor;
            float openingTop =
                backCeiling < frontCeiling ? backCeiling : frontCeiling;

            float textureTop = (line->flags & ML_DONTPEGBOTTOM)
                                   ? openingBottom + textureHeight
                                   : openingTop;
            float top;
            float bottom;

            textureTop += rowOffset;

            top = textureTop < openingTop ? textureTop : openingTop;
            bottom = textureTop - textureHeight;

            if (bottom < openingBottom)
                bottom = openingBottom;

            uStart = eacpFixedToFloat(side->textureoffset) / textureWidth;
            uEnd = uStart + length / textureWidth;

            eacpEmitWallQuad(em, texture, x1, y1, x2, y2, bottom, top, textureTop,
                             uStart, uEnd, textureHeight, light);
        }
    }
}

// Every thing in the level - monsters, items, decorations, the player's
// corpse - as a quad facing the camera, exactly where DOOM's own sprite
// projection would put it: the sprite's left edge sits its own offset to the
// left of the thing's position along the view plane, and its top sits the
// sprite's top offset above the thing's feet.
static void eacpEmitSprite(EacpEmitter* em,
                           mobj_t* thing,
                           mobj_t* viewer,
                           double rightX,
                           double rightY)
{
    spritedef_t* definition;
    spriteframe_t* frame;
    int rotation = 0;
    int lump;
    int flip;
    float light;
    double leftX, leftY;
    float width, height, top, bottom;
    float u0, u1;

    if (thing == viewer || thing->sprite < 0 || thing->sprite >= numsprites)
        return;

    definition = &sprites[thing->sprite];

    if ((int) (thing->frame & FF_FRAMEMASK) >= definition->numframes)
        return;

    frame = &definition->spriteframes[thing->frame & FF_FRAMEMASK];

    // Eight drawings per frame, one per facing: which one shows depends on the
    // angle the thing is seen from.
    if (frame->rotate)
    {
        angle_t seen = R_PointToAngle2(viewer->x, viewer->y, thing->x, thing->y);
        rotation =
            (seen - thing->angle + (unsigned) (ANG45 / 2) * 9) >> 29;
    }

    lump = frame->lump[rotation];
    flip = frame->flip[rotation];

    if (lump < 0 || lump >= numspritelumps)
        return;

    width = (float) (spritewidth[lump] >> FRACBITS);
    height = (float) eacpSpriteHeights[lump];

    // A thing moves once a tic, so drawing it where the tic left it makes it
    // step while the world glides past. Its momentum is how far it travelled to
    // get there, so winding that back by the part of the tic still to come puts
    // it where it would be at the moment being drawn.
    {
        double back = 1.0 - (double) eacpAlpha;

        double thingX =
            eacpFixedToDouble(thing->x) - eacpFixedToDouble(thing->momx) * back;
        double thingY =
            eacpFixedToDouble(thing->y) - eacpFixedToDouble(thing->momy) * back;
        float thingZ = eacpFixedToFloat(thing->z)
                       - (float) (eacpFixedToDouble(thing->momz) * back);

        leftX = thingX - rightX * eacpFixedToDouble(spriteoffset[lump]);
        leftY = thingY - rightY * eacpFixedToDouble(spriteoffset[lump]);

        top = thingZ + eacpFixedToFloat(spritetopoffset[lump]);
        bottom = top - height;
    }

    // A lit frame (a muzzle flash, a rocket) ignores the sector's light.
    light = (thing->frame & FF_FULLBRIGHT)
                ? 0.0f
                : eacpStartMap(thing->subsector->sector->lightlevel, 0);

    u0 = flip ? 1.0f : 0.0f;
    u1 = flip ? 0.0f : 1.0f;

    {
        int texture = eacpSpriteBase() + lump;

        float ax = (float) leftX;
        float az = (float) -leftY;
        float bx = (float) (leftX + rightX * width);
        float bz = (float) -(leftY + rightY * width);

        eacpEmitVertex(em, texture, ax, bottom, az, u0, 1.0f, light);
        eacpEmitVertex(em, texture, bx, bottom, bz, u1, 1.0f, light);
        eacpEmitVertex(em, texture, bx, top, bz, u1, 0.0f, light);

        eacpEmitVertex(em, texture, ax, bottom, az, u0, 1.0f, light);
        eacpEmitVertex(em, texture, bx, top, bz, u1, 0.0f, light);
        eacpEmitVertex(em, texture, ax, top, az, u0, 0.0f, light);
    }
}

// The engine's 32-bit angle for a heading in radians, so the view being drawn
// can index the same sine tables the engine uses.
static angle_t eacpAngleFromRadians(float radians)
{
    return (angle_t) (unsigned) (long long) ((double) radians
                                             * (2147483648.0 / 3.14159265358979));
}

static void eacpEmitSprites(EacpEmitter* em,
                            mobj_t* viewer,
                            const EacpDoomCamera* camera)
{
    thinker_t* thinker;

    // The view plane's right axis, the one DOOM measures a sprite's width
    // along, so the billboards stay square-on to the camera being drawn.
    angle_t facing = eacpAngleFromRadians(camera->angle);

    double rightX = eacpFixedToDouble(finesine[facing >> ANGLETOFINESHIFT]);
    double rightY = -eacpFixedToDouble(finecosine[facing >> ANGLETOFINESHIFT]);

    for (thinker = thinkercap.next; thinker != &thinkercap;
         thinker = thinker->next)
    {
        if (thinker->function.acp1 != (actionf_p1) P_MobjThinker)
            continue;

        eacpEmitSprite(em, (mobj_t*) thinker, viewer, rightX, rightY);
    }
}

// The sky is not geometry in DOOM: it is painted wherever a ceiling is missing,
// at a column picked by the direction the player faces. A cylinder around the
// camera reproduces that - it never moves relative to the viewer, so it has no
// parallax, and its texture repeats four times around, as the engine's does.
static void eacpEmitSky(EacpEmitter* em, const EacpDoomCamera* camera)
{
    int texture;
    int i;
    double camX, camY;
    float camZ;
    float vTop, vBottom;

    if (skytexture <= 0 || skytexture >= numtextures)
        return;

    texture = texturetranslation[skytexture];

    camX = camera->x;
    camY = camera->y;
    camZ = camera->z;

    // DOOM pins the sky to screen rows, with row 100 on the horizon. A screen
    // row is linear in height on the cylinder, so two rings are exact.
    vTop = (100.0f - EACP_SKY_FOCAL * EACP_SKY_HEIGHT / EACP_SKY_RADIUS) / 128.0f;
    vBottom =
        (100.0f + EACP_SKY_FOCAL * EACP_SKY_HEIGHT / EACP_SKY_RADIUS) / 128.0f;

    for (i = 0; i < EACP_SKY_SEGMENTS; ++i)
    {
        angle_t a0 = (angle_t) i << 26;
        angle_t a1 = (angle_t) (i + 1) << 26;

        double x0 = camX
                    + EACP_SKY_RADIUS
                          * eacpFixedToDouble(finecosine[a0 >> ANGLETOFINESHIFT]);
        double y0 = camY
                    + EACP_SKY_RADIUS
                          * eacpFixedToDouble(finesine[a0 >> ANGLETOFINESHIFT]);
        double x1 = camX
                    + EACP_SKY_RADIUS
                          * eacpFixedToDouble(finecosine[a1 >> ANGLETOFINESHIFT]);
        double y1 = camY
                    + EACP_SKY_RADIUS
                          * eacpFixedToDouble(finesine[a1 >> ANGLETOFINESHIFT]);

        float u0 = 4.0f * (float) i / (float) EACP_SKY_SEGMENTS;
        float u1 = 4.0f * (float) (i + 1) / (float) EACP_SKY_SEGMENTS;

        float top = camZ + EACP_SKY_HEIGHT;
        float bottom = camZ - EACP_SKY_HEIGHT;

        float ax = (float) x0;
        float az = (float) -y0;
        float bx = (float) x1;
        float bz = (float) -y1;

        // Light 0: the sky is always at its brightest, at any distance.
        eacpEmitVertex(em, texture, ax, bottom, az, u0, vBottom, 0.0f);
        eacpEmitVertex(em, texture, bx, bottom, bz, u1, vBottom, 0.0f);
        eacpEmitVertex(em, texture, bx, top, bz, u1, vTop, 0.0f);

        eacpEmitVertex(em, texture, ax, bottom, az, u0, vBottom, 0.0f);
        eacpEmitVertex(em, texture, bx, top, bz, u1, vTop, 0.0f);
        eacpEmitVertex(em, texture, ax, top, az, u0, vTop, 0.0f);
    }
}

void eacpDoomGetHudSprites(EacpDoomHudSprite* out)
{
    player_t* player = &players[displayplayer];
    int i;

    if (out == 0)
        return;

    for (i = 0; i < EACP_DOOM_HUD_SPRITES; ++i)
        out[i].textureId = -1;

    eacpEnsureTextureData();

    if (!eacpTexturesReady || gamestate != GS_LEVEL || player->mo == 0)
        return;

    for (i = 0; i < NUMPSPRITES && i < EACP_DOOM_HUD_SPRITES; ++i)
    {
        pspdef_t* weapon = &player->psprites[i];
        state_t* state = weapon->state;
        spritedef_t* definition;
        spriteframe_t* frame;
        int lump;

        if (state == 0 || state->sprite < 0 || state->sprite >= numsprites)
            continue;

        definition = &sprites[state->sprite];

        if ((int) (state->frame & FF_FRAMEMASK) >= definition->numframes)
            continue;

        frame = &definition->spriteframes[state->frame & FF_FRAMEMASK];
        lump = frame->lump[0];

        if (lump < 0 || lump >= numspritelumps)
            continue;

        out[i].textureId = eacpSpriteBase() + lump;
        out[i].width = (float) (spritewidth[lump] >> FRACBITS);
        out[i].height = (float) eacpSpriteHeights[lump];

        // The engine positions the weapon in a 320x200 space centred on row
        // 100, while the view it lands in is 168 rows tall and centred on row
        // 84 - hence the 16-row shift.
        out[i].x =
            eacpFixedToFloat(weapon->sx) - eacpFixedToFloat(spriteoffset[lump]);
        out[i].y = eacpFixedToFloat(weapon->sy)
                   - eacpFixedToFloat(spritetopoffset[lump]) - 16.0f;

        out[i].light =
            (state->frame & FF_FULLBRIGHT)
                ? 0.0f
                : eacpStartMap(player->mo->subsector->sector->lightlevel, 0);

        out[i].flip = frame->flip[0];
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
                 eacpFloorHeight(sector), light);

    // A sky ceiling is a hole onto the sky, not a surface.
    if (sector->ceilingpic != skyflatnum)
        eacpEmitFlat(em, index, sector->ceilingpic,
                     eacpCeilingHeight(sector), light);
}

static void eacpEmitWorld(EacpEmitter* em, const EacpDoomCamera* camera)
{
    player_t* player = &players[displayplayer];
    int i;

    for (i = 0; i < numlines; ++i)
    {
        eacpEmitLineSide(em, &lines[i], i, 0);
        eacpEmitLineSide(em, &lines[i], i, 1);
    }

    for (i = 0; i < numsubsectors; ++i)
        eacpEmitSubsector(em, i);

    if (player->mo == 0)
        return;

    eacpEmitSky(em, camera);
    eacpEmitSprites(em, player->mo, camera);
}

int eacpDoomBuildGeometry(const EacpDoomCamera* camera,
                          float alpha,
                          EacpDoomVertex* vertices,
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

    eacpAlpha = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);
    eacpEnsureTextureData();

    if (gamestate != GS_LEVEL || vertices == 0 || draws == 0 || camera == 0
        || textureCount <= 0 || lines == 0 || !eacpTexturesReady)
        return 0;

    eacpEnsureLevel();

    if (eacpPolyTotal <= 0 || !eacpEnsureScratch(textureCount))
        return 0;

    for (i = 0; i < textureCount; ++i)
        eacpTextureCounts[i] = 0;

    em.counts = eacpTextureCounts;
    em.cursors = eacpTextureCursors;
    em.vertices = 0;
    eacpEmitWorld(&em, camera);

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
    eacpEmitWorld(&em, camera);

    if (outVertexCount != 0)
        *outVertexCount = total;

    return drawCount;
}
