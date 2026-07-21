// The snapshot interface between the DOOM engine's internals and the eacp
// renderer: an ordinary translation unit that includes the engine's headers.
// Nothing DOOM-typed leaks out through EngineAccess.h.

#include "EngineAccess.h"

#include <DOOM/DOOM.h>
#include <DOOM/doomtype.h>
#include <DOOM/Game/ConfigTypes.h>
#include <DOOM/Game/GameClock.h>
#include <DOOM/Game/GameDefs.h>
#include <DOOM/Game/GameFlow.h>
#include <DOOM/Game/GameSession.h>
#include <DOOM/Game/MapSpawns.h>
#include <DOOM/Game/OverlayState.h>
#include <DOOM/Game/PlayerState.h>
#include <DOOM/Game/PlayerTypes.h>
#include <DOOM/Game/RefreshFlags.h>
#include <DOOM/Game/SkyState.h>
#include <DOOM/Host/Platform.h>
#include <DOOM/Math/FixedPoint.h>
#include <DOOM/Math/TrigTables.h>
#include <DOOM/Render/BSP.h>
#include <DOOM/Render/Data.h>
#include <DOOM/Render/GraphicsData.h>
#include <DOOM/Render/Lighting.h>
#include <DOOM/Render/Main.h>
#include <DOOM/Render/Planes.h>
#include <DOOM/Render/RenderTypes.h>
#include <DOOM/Render/Things.h>
#include <DOOM/Render/Video.h>
#include <DOOM/Render/ViewWindow.h>
#include <DOOM/Sim/Info.h>
#include <DOOM/Sim/Level.h>
#include <DOOM/Sim/MapTypes.h>
#include <DOOM/Sim/SimDefs.h>
#include <DOOM/Sim/ThinkerList.h>
#include <DOOM/Sim/WeaponTypes.h>
#include <DOOM/UI/Automap.h>
#include <DOOM/UI/AutomapTypes.h>
#include <DOOM/UI/Hud.h>
#include <DOOM/UI/Menu.h>
#include <DOOM/UI/MenuSettings.h>
#include <DOOM/UI/StatusBarTypes.h>
#include <DOOM/UI/Wipe.h>
#include <DOOM/Wad/WadFile.h>

#include <algorithm>
#include <cmath>
#include <numbers>

// The engine's live palette, which no header declares - UI/Menu.cpp reaches it
// the same way. It is the natural candidate for a real interface as the engine
// gets refactored further.
extern unsigned char screen_palette[256 * 3];

namespace PureDoom::Engine
{
namespace
{
// Convex subsector cells rarely exceed a handful of corners; the cap only
// bounds the clipper's scratch space.
constexpr auto maxPolyVertices = 32;

// Larger than any DOOM map, so the initial square the BSP clips down is
// effectively unbounded.
constexpr auto mapLimit = 32768.0;

constexpr auto clipEpsilon = 1e-6;

constexpr auto flatSize = 64;

// The sky is a cylinder around the camera, far enough out that every wall in a
// DOOM level stands in front of it, and tall enough to fill the view.
constexpr auto skySegments = 64;
constexpr auto skyRadius = 12000.0f;
constexpr auto skyHeight = 9000.0f;

// The view's focal length in rows: half of the 168-row view divided by the
// tangent of half its vertical field of view. It converts a height on the sky
// cylinder into the screen row DOOM would have drawn it at.
constexpr auto skyFocal = 133.33f;

// angle_t maps the full circle onto 32 bits, so 2^31 is half a turn.
constexpr auto halfTurn = 2147483648.0;

// Whether a caller's buffer is big enough for what is about to be written into
// it. Every span this file takes is an out-parameter sized by one of the
// constants in EngineAccess.h, and this is where that is checked rather than
// assumed.
bool fits(std::span<const std::uint8_t> buffer, int bytes)
{
    return buffer.size() >= static_cast<std::size_t>(bytes);
}

double toDouble(fixed_t value)
{
    return static_cast<double>(value.raw) / static_cast<double>(FRACUNIT.raw);
}

float toFloat(fixed_t value)
{
    return static_cast<float>(value.raw) / static_cast<float>(FRACUNIT.raw);
}

fixed_t toFixed(double value)
{
    return fixed_t {static_cast<std::int32_t>(value * FRACUNIT.raw)};
}

// The engine's 32-bit angle for a heading in radians, so the view being drawn
// can index the same sine tables the engine uses.
angle_t angleFromRadians(float radians)
{
    auto turns = static_cast<double>(radians) * (halfTurn / std::numbers::pi);

    return angle_t {static_cast<std::uint32_t>(static_cast<std::int64_t>(turns))};
}

// A point in the map's own ground plane, in whole map units. Every double in
// this file that names a map coordinate is in whole units, never in fixed-point
// raw ones - mixing the two is a factor of 65536 that compiles silently.
struct Point
{
    double x = 0.0;
    double y = 0.0;
};

// The COLORMAP row a surface resolves through, and whether distance darkens it
// further.
struct Light
{
    float row = 0.0f;
    float falloff = 0.0f;
};

// Where every sector's floor and ceiling stood at the last tic, so a door or a
// lift can be drawn part-way to where it is going rather than jumping there,
// and how far into the current tic the frame being built sits.
struct TicSnapshot
{
    Vector<float> floor;
    Vector<float> ceiling;
    float alpha = 0.0f;
};

// The subsector cells recovered from the BSP, and the length of every line the
// geometry pass lays a texture along. Both are fixed for a level, so both are
// rebuilt only when a new one loads.
struct LevelCells
{
    Vector<Point> corners;
    Vector<int> start;
    Vector<int> count;
    Vector<float> lineLengths;

    const void* cachedNodes = nullptr;
    int cachedSubsectors = -1;
    int cachedEpisode = -1;
    int cachedMap = -1;
};

// Every texture the game loaded, decided once: the dimensions and whether it has
// holes are known only after decoding, and both are wanted before the renderer
// asks for a single pixel.
struct TextureTable
{
    bool ready() const { return !infos.empty(); }

    Vector<TextureInfo> infos;
};

// The per-texture vertex runs a frame is grouped into: counted on the first pass
// over the world, written on the second.
struct EmitScratch
{
    Vector<int> counts;
    Vector<int> cursors;
};

TicSnapshot snapshot;
LevelCells cells;
TextureTable textureTable;
EmitScratch scratch;

// A sector's floor or ceiling where it is *now*, part-way through whatever move
// a door or a lift has it making, rather than where the last tic left it. The
// walls that meet it are drawn from the same number, so nothing tears.
float interpolatedHeight(const Vector<float>& previous, int index, float now)
{
    if (index < 0 || index >= previous.size())
        return now;

    return std::lerp(previous[index], now, snapshot.alpha);
}

int sectorIndex(const Doom::Sector& sector)
{
    return static_cast<int>(&sector - sectors);
}

float floorHeight(const Doom::Sector& sector)
{
    return interpolatedHeight(
        snapshot.floor, sectorIndex(sector), toFloat(sector.floorheight));
}

float ceilingHeight(const Doom::Sector& sector)
{
    return interpolatedHeight(
        snapshot.ceiling, sectorIndex(sector), toFloat(sector.ceilingheight));
}

int spriteBase()
{
    auto& gfx = Doom::graphicsData();

    return gfx.numtextures + gfx.numflats;
}

Doom::Player& displayPlayer()
{
    auto& players = Doom::playerState();

    return players.players[players.displayplayer];
}

// The row the whole view is locked to, or zero for none: the invulnerability
// sphere picks the inverse map and the light-amp visor the brightest row
// (playerThink), and setupFrame then puts every wall, flat, sprite and the
// weapon through it, with the sector's brightness and the distance falloff both
// ignored.
int fixedRow()
{
    return displayPlayer().fixedcolormap;
}

// One row, whatever the distance - what the engine does with a bare colormap
// pointer rather than a light table.
Light fixedLight(int row)
{
    return {static_cast<float>(row), 0.0f};
}

// The row a surface starts at before distance darkens it further: the engine's
// light level scaled into the 32 maps, offset by the sector's brightness and the
// fake contrast walls get for their orientation.
Light sectorLight(int lightlevel, int contrast)
{
    if (fixedRow())
        return fixedLight(fixedRow());

    auto lightnum = std::clamp((lightlevel >> Doom::LIGHTSEGSHIFT)
                                   + Doom::lighting().extralight + contrast,
                               0,
                               Doom::LIGHTLEVELS - 1);

    auto row = (Doom::LIGHTLEVELS - 1 - lightnum) * 2 * Doom::NUMCOLORMAPS
               / Doom::LIGHTLEVELS;

    return {static_cast<float>(row), 1.0f};
}

// A frame the engine marks as lit - a muzzle flash, a rocket - is drawn through
// row 0 at any distance. A powerup outranks it (projectSprite tests
// fixedcolormap first), and this says both.
Light fullbrightLight()
{
    return fixedLight(fixedRow());
}

//
// Texture decoding.
//

// A decoded graphic: the palette indices, and the coverage saying which of them
// the patches actually drew. DOOM stores every graphic as runs of pixels down a
// column with the gaps between the runs left transparent, so the coverage is
// what makes a masked texture's holes come out as holes.
struct IndexImage
{
    IndexImage(int widthToUse, int heightToUse)
        : width(widthToUse)
        , height(heightToUse)
    {
        indices.resize(width * height);
        alpha.resize(width * height);
    }

    void plot(int x, int y, std::uint8_t index)
    {
        if (x < 0 || x >= width || y < 0 || y >= height)
            return;

        indices[y * width + x] = index;
        alpha[y * width + x] = 255;
    }

    bool hasHoles() const
    {
        return std::ranges::any_of(alpha, [](auto covered) { return covered == 0; });
    }

    Vector<std::uint8_t> indices;
    Vector<std::uint8_t> alpha;
    int width = 0;
    int height = 0;
};

Doom::Patch* patchAt(int lump)
{
    return static_cast<Doom::Patch*>(Doom::cacheLumpNum(lump));
}

// Composing from the patches (as generateComposite does) rather than reading the
// engine's cached columns is what keeps the gaps.
void blitPatch(IndexImage& image, const Doom::Patch& patch, int originX, int originY)
{
    const auto* base = reinterpret_cast<const byte*>(&patch);

    for (auto x = 0; x < patch.width; ++x)
    {
        const auto* column =
            reinterpret_cast<const Doom::Column*>(base + patch.columnofs[x]);

        while (column->topdelta != 0xff)
        {
            const auto* source = reinterpret_cast<const byte*>(column) + 3;

            for (auto i = 0; i < column->length; ++i)
                image.plot(originX + x, originY + column->topdelta + i, source[i]);

            column = reinterpret_cast<const Doom::Column*>(
                reinterpret_cast<const byte*>(column) + column->length + 4);
        }
    }
}

IndexImage decodeWall(int id)
{
    const auto& texture = *textures[id];
    auto image = IndexImage {texture.width, texture.height};

    for (auto i = 0; i < texture.patchcount; ++i)
    {
        const auto& piece = texture.patches[i];

        blitPatch(image, *patchAt(piece.patch), piece.originx, piece.originy);
    }

    return image;
}

IndexImage decodeSprite(int lump, const TextureInfo& info)
{
    auto image = IndexImage {info.width, info.height};

    blitPatch(image, *patchAt(lump), 0, 0);

    return image;
}

void ensureTextureTable()
{
    auto& gfx = Doom::graphicsData();

    if (textureTable.ready() || gfx.numtextures <= 0 || textures == nullptr)
        return;

    textureTable.infos.resize(gfx.numtextures + gfx.numflats + gfx.numspritelumps);

    for (auto id = 0; id < gfx.numtextures; ++id)
        textureTable.infos[id] = {
            textures[id]->width, textures[id]->height, decodeWall(id).hasHoles()};

    for (auto i = 0; i < gfx.numflats; ++i)
        textureTable.infos[gfx.numtextures + i] = {flatSize, flatSize, false};

    for (auto i = 0; i < gfx.numspritelumps; ++i)
        textureTable.infos[spriteBase() + i] = {
            spritewidth[i].toInt(), patchAt(gfx.firstspritelump + i)->height, true};
}

int spriteHeight(int lump)
{
    return textureTable.infos[spriteBase() + lump].height;
}

//
// The subsector cells, recovered from the BSP.
//

// A convex cell being carried down the BSP, and the clipper's working shape.
struct Polygon
{
    void add(const Point& corner)
    {
        if (count < maxPolyVertices)
            corners[count++] = corner;
    }

    const Point& operator[](int index) const { return corners[index]; }

    Array<Point, maxPolyVertices> corners;
    int count = 0;
};

// Sutherland-Hodgman against one half-plane. The side test matches the engine's
// own pointOnSide: a point is in front of a directed line when
// dy * (px - x) - dx * (py - y) is positive.
Polygon clipToLine(const Polygon& in,
                   const Point& origin,
                   const Point& delta,
                   bool keepFront)
{
    auto out = Polygon {};

    auto sideOf = [&](const Point& point)
    {
        auto side = delta.y * (point.x - origin.x) - delta.x * (point.y - origin.y);

        return keepFront ? side : -side;
    };

    for (auto i = 0; i < in.count; ++i)
    {
        const auto& a = in[i];
        const auto& b = in[(i + 1) % in.count];

        auto sideA = sideOf(a);
        auto sideB = sideOf(b);

        if (sideA >= -clipEpsilon)
            out.add(a);

        if ((sideA > clipEpsilon && sideB < -clipEpsilon)
            || (sideA < -clipEpsilon && sideB > clipEpsilon))
        {
            auto t = sideA / (sideA - sideB);

            out.add({a.x + t * (b.x - a.x), a.y + t * (b.y - a.y)});
        }
    }

    return out;
}

// A subsector's cell, trimmed to the sector boundary: the segs bound it on the
// sides that came from real linedefs, and the interior always lies in front of
// them.
void storeSubsector(int index, const Polygon& poly)
{
    const auto& subsector = subsectors[index];
    auto current = poly;

    for (auto i = 0; i < subsector.numlines && current.count >= 3; ++i)
    {
        const auto& seg = segs[subsector.firstline + i];

        auto origin = Point {toDouble(seg.v1->x), toDouble(seg.v1->y)};
        auto delta =
            Point {toDouble(seg.v2->x) - origin.x, toDouble(seg.v2->y) - origin.y};

        current = clipToLine(current, origin, delta, true);
    }

    if (current.count < 3)
        return;

    cells.start[index] = cells.corners.size();
    cells.count[index] = current.count;

    for (auto i = 0; i < current.count; ++i)
        cells.corners.add(current[i]);
}

// Vanilla nodes carry no polygons - only the split planes - so each subsector's
// shape is recovered by carrying a huge square down the BSP and clipping it by
// every partition on the way to the leaf.
void descend(int nodenum, const Polygon& poly)
{
    if (poly.count < 3)
        return;

    if (nodenum & Doom::NF_SUBSECTOR)
    {
        storeSubsector(nodenum & ~Doom::NF_SUBSECTOR, poly);
        return;
    }

    const auto& node = nodes[nodenum];

    auto origin = Point {toDouble(node.x), toDouble(node.y)};
    auto delta = Point {toDouble(node.dx), toDouble(node.dy)};

    descend(node.children[0], clipToLine(poly, origin, delta, true));
    descend(node.children[1], clipToLine(poly, origin, delta, false));
}

// A line's length never changes, and the geometry pass needs it for every wall
// it lays a texture along, so it is measured once when the level loads rather
// than thousands of times a frame.
void measureLines()
{
    cells.lineLengths.resize(numlines);

    for (auto i = 0; i < numlines; ++i)
    {
        auto dx = toDouble(lines[i].dx);
        auto dy = toDouble(lines[i].dy);

        cells.lineLengths[i] = static_cast<float>(std::sqrt(dx * dx + dy * dy));
    }
}

// The BSP is static for a level, so the cells are rebuilt only when a new one
// loads; per frame the geometry pass just re-reads the (moving) heights.
void ensureLevel()
{
    auto& session = Doom::gameSession();

    if (nodes == cells.cachedNodes && numsubsectors == cells.cachedSubsectors
        && session.gameepisode == cells.cachedEpisode
        && session.gamemap == cells.cachedMap)
        return;

    cells.cachedNodes = nodes;
    cells.cachedSubsectors = numsubsectors;
    cells.cachedEpisode = session.gameepisode;
    cells.cachedMap = session.gamemap;

    cells.corners.clear();
    cells.start.clear();
    cells.count.clear();

    if (numsubsectors <= 0 || numnodes <= 0 || nodes == nullptr)
        return;

    cells.start.resize(numsubsectors);
    cells.count.resize(numsubsectors);
    cells.corners.reserveAtLeast(numsubsectors * maxPolyVertices);

    measureLines();

    auto square = Polygon {};
    square.add({-mapLimit, -mapLimit});
    square.add({mapLimit, -mapLimit});
    square.add({mapLimit, mapLimit});
    square.add({-mapLimit, mapLimit});

    descend(numnodes - 1, square);
}

//
// The world, as vertices grouped by texture.
//

// The two ends of a vertical quad on the ground and the heights it spans, in the
// renderer's coordinate space.
struct QuadSpan
{
    float ax = 0.0f;
    float az = 0.0f;
    float bx = 0.0f;
    float bz = 0.0f;
    float bottom = 0.0f;
    float top = 0.0f;
};

struct QuadUv
{
    float uStart = 0.0f;
    float uEnd = 0.0f;
    float vTop = 0.0f;
    float vBottom = 0.0f;
};

// The frame's geometry, laid down twice: the first pass sizes each texture's
// run, the second writes into the run reserved for it. `vertices` is empty while
// counting.
struct Emitter
{
    bool counting() const { return vertices.empty(); }

    void vertex(int textureId,
                float x,
                float y,
                float z,
                float u,
                float v,
                const Light& light)
    {
        if (counting())
        {
            ++counts[textureId];
            return;
        }

        if (cursors[textureId] < 0)
            return;

        vertices[cursors[textureId]++] = {
            {x, y, z}, {u, v}, light.row, light.falloff};
    }

    void quad(int textureId,
              const QuadSpan& span,
              const QuadUv& uv,
              const Light& light)
    {
        vertex(
            textureId, span.ax, span.bottom, span.az, uv.uStart, uv.vBottom, light);
        vertex(textureId, span.bx, span.bottom, span.bz, uv.uEnd, uv.vBottom, light);
        vertex(textureId, span.bx, span.top, span.bz, uv.uEnd, uv.vTop, light);

        vertex(
            textureId, span.ax, span.bottom, span.az, uv.uStart, uv.vBottom, light);
        vertex(textureId, span.bx, span.top, span.bz, uv.uEnd, uv.vTop, light);
        vertex(textureId, span.ax, span.top, span.az, uv.uStart, uv.vTop, light);
    }

    std::span<WorldVertex> vertices;
    std::span<int> counts;
    std::span<int> cursors;
};

// DOOM's map plane is (x, y) with z up; the renderer's is (x, up, -y).
QuadSpan groundSpan(const Point& from, const Point& to, float bottom, float top)
{
    return {static_cast<float>(from.x),
            static_cast<float>(-from.y),
            static_cast<float>(to.x),
            static_cast<float>(-to.y),
            bottom,
            top};
}

// A wall texture as the geometry pass wants it: the translated id the animation
// is currently showing, and its size in world units.
struct WallTexture
{
    int id = 0;
    float width = 0.0f;
    float height = 0.0f;
};

WallTexture wallTexture(int index)
{
    auto id = texturetranslation[index];

    return {id,
            static_cast<float>(textures[id]->width),
            static_cast<float>(textures[id]->height)};
}

// One textured band of a linedef. `textureTop` is where the texture's own top
// edge sits in world height, which is what DOOM's pegging rules decide.
void emitWall(Emitter& emitter,
              const WallTexture& texture,
              const Doom::Side& side,
              const QuadSpan& span,
              float textureTop,
              float length,
              const Light& light)
{
    if (span.top <= span.bottom)
        return;

    auto uStart = toFloat(side.textureoffset) / texture.width;

    emitter.quad(texture.id,
                 span,
                 {uStart,
                  uStart + length / texture.width,
                  (textureTop - span.top) / texture.height,
                  (textureTop - span.bottom) / texture.height},
                 light);
}

// The software renderer's fake contrast: walls running east-west are a step
// darker, north-south a step brighter, so corners stay readable.
int wallContrast(const Doom::Line& line)
{
    if (line.v1->y == line.v2->y)
        return -1;

    if (line.v1->x == line.v2->x)
        return 1;

    return 0;
}

void emitLineSide(Emitter& emitter, const Doom::Line& line, int index, int s)
{
    auto& sky = Doom::skyState();

    if (line.sidenum[s] < 0)
        return;

    const auto& side = sides[line.sidenum[s]];
    const auto& front = *side.sector;
    const auto* back =
        line.sidenum[s ^ 1] >= 0 ? sides[line.sidenum[s ^ 1]].sector : nullptr;

    const auto& v1 = s == 0 ? *line.v1 : *line.v2;
    const auto& v2 = s == 0 ? *line.v2 : *line.v1;

    auto from = Point {toDouble(v1.x), toDouble(v1.y)};
    auto to = Point {toDouble(v2.x), toDouble(v2.y)};

    auto length = cells.lineLengths[index];
    auto frontFloor = floorHeight(front);
    auto frontCeiling = ceilingHeight(front);
    auto rowOffset = toFloat(side.rowoffset);
    auto light = sectorLight(front.lightlevel, wallContrast(line));
    auto pegBottom = (line.flags & Doom::ML_DONTPEGBOTTOM) != 0;

    if (back == nullptr)
    {
        if (side.midtexture <= 0)
            return;

        auto texture = wallTexture(side.midtexture);
        auto textureTop =
            (pegBottom ? frontFloor + texture.height : frontCeiling) + rowOffset;

        emitWall(emitter,
                 texture,
                 side,
                 groundSpan(from, to, frontFloor, frontCeiling),
                 textureTop,
                 length,
                 light);
        return;
    }

    auto backFloor = floorHeight(*back);
    auto backCeiling = ceilingHeight(*back);

    if (side.bottomtexture > 0 && backFloor > frontFloor)
    {
        auto texture = wallTexture(side.bottomtexture);
        auto textureTop = (pegBottom ? frontCeiling : backFloor) + rowOffset;

        emitWall(emitter,
                 texture,
                 side,
                 groundSpan(from, to, frontFloor, backFloor),
                 textureTop,
                 length,
                 light);
    }

    // Between two sky ceilings the step is invisible sky (the classic sky hack),
    // not an upper wall.
    auto bothSky =
        front.ceilingpic == sky.skyflatnum && back->ceilingpic == sky.skyflatnum;

    if (side.toptexture > 0 && backCeiling < frontCeiling && !bothSky)
    {
        auto texture = wallTexture(side.toptexture);
        auto pegTop = (line.flags & Doom::ML_DONTPEGTOP) != 0;
        auto textureTop =
            (pegTop ? frontCeiling : backCeiling + texture.height) + rowOffset;

        emitWall(emitter,
                 texture,
                 side,
                 groundSpan(from, to, backCeiling, frontCeiling),
                 textureTop,
                 length,
                 light);
    }

    // The middle texture of a two-sided line - a grate, a window, a hanging
    // vine. It is masked, and unlike the walls above it never tiles: DOOM draws
    // it once, clipped to the opening, which is why a too-short midtexture
    // leaves a gap rather than repeating.
    if (side.midtexture > 0)
    {
        auto texture = wallTexture(side.midtexture);

        auto openingBottom = std::max(backFloor, frontFloor);
        auto openingTop = std::min(backCeiling, frontCeiling);

        auto textureTop =
            (pegBottom ? openingBottom + texture.height : openingTop) + rowOffset;

        emitWall(emitter,
                 texture,
                 side,
                 groundSpan(from,
                            to,
                            std::max(textureTop - texture.height, openingBottom),
                            std::min(textureTop, openingTop)),
                 textureTop,
                 length,
                 light);
    }
}

// Every thing in the level - monsters, items, decorations, the player's
// corpse - as a quad facing the camera, exactly where DOOM's own sprite
// projection would put it: the sprite's left edge sits its own offset to the
// left of the thing's position along the view plane, and its top sits the
// sprite's top offset above the thing's feet.
void emitSprite(Emitter& emitter,
                const Doom::Mobj& thing,
                const Doom::Mobj& viewer,
                const Point& right)
{
    auto& gfx = Doom::graphicsData();
    auto sprite = Doom::toIndex(thing.sprite);

    if (&thing == &viewer || sprite < 0 || sprite >= gfx.numsprites)
        return;

    const auto& definition = sprites[sprite];
    auto frameIndex = static_cast<int>(thing.frame & Doom::FF_FRAMEMASK);

    if (frameIndex >= definition.numframes)
        return;

    const auto& frame = definition.spriteframes[frameIndex];

    // Eight drawings per frame, one per facing: which one shows depends on the
    // angle the thing is seen from.
    auto rotation = 0;

    if (frame.rotate)
    {
        auto seen = Doom::pointToAngle2(viewer.x, viewer.y, thing.x, thing.y);

        rotation = static_cast<int>(
            ((seen - thing.angle + (Doom::ang45 / 2u) * 9u) >> 29).raw);
    }

    auto lump = frame.lump[rotation];

    if (lump < 0 || lump >= gfx.numspritelumps)
        return;

    auto width = static_cast<float>(spritewidth[lump].toInt());
    auto height = static_cast<float>(spriteHeight(lump));

    // A thing moves once a tic, so drawing it where the tic left it makes it
    // step while the world glides past. Its momentum is how far it travelled to
    // get there, so winding that back by the part of the tic still to come puts
    // it where it would be at the moment being drawn.
    auto back = 1.0 - static_cast<double>(snapshot.alpha);
    auto offset = toDouble(spriteoffset[lump]);

    auto left =
        Point {toDouble(thing.x) - toDouble(thing.momx) * back - right.x * offset,
               toDouble(thing.y) - toDouble(thing.momy) * back - right.y * offset};

    auto feet = toFloat(thing.z) - static_cast<float>(toDouble(thing.momz) * back);
    auto top = feet + toFloat(spritetopoffset[lump]);

    auto light = (thing.frame & Doom::FF_FULLBRIGHT)
                     ? fullbrightLight()
                     : sectorLight(thing.subsector->sector->lightlevel, 0);

    auto flip = frame.flip[rotation] != 0;
    auto right_ = Point {left.x + right.x * width, left.y + right.y * width};

    emitter.quad(spriteBase() + lump,
                 groundSpan(left, right_, top - height, top),
                 {flip ? 1.0f : 0.0f, flip ? 0.0f : 1.0f, 0.0f, 1.0f},
                 light);
}

void emitSprites(Emitter& emitter, const Doom::Mobj& viewer, const Camera& camera)
{
    auto& thinkers = Doom::thinkerList();

    // The view plane's right axis, the one DOOM measures a sprite's width along,
    // so the billboards stay square-on to the camera being drawn.
    auto facing = angleFromRadians(camera.angle);
    auto right = Point {toDouble(finesine[facing.fineIndex()]),
                        -toDouble(finecosine[facing.fineIndex()])};

    for (auto* thinker = thinkers.cap.next; thinker != &thinkers.cap;
         thinker = thinker->next)
    {
        // Skip a removed-but-not-yet-freed thinker, as the engine's own scans do.
        if (thinker->kind() != Doom::ThinkerKind::Mobj || thinker->removed)
            continue;

        emitSprite(emitter, *static_cast<Doom::Mobj*>(thinker), viewer, right);
    }
}

// The sky is not geometry in DOOM: it is painted wherever a ceiling is missing,
// at a column picked by the direction the player faces. A cylinder around the
// camera reproduces that - it never moves relative to the viewer, so it has no
// parallax, and its texture repeats four times around, as the engine's does.
void emitSky(Emitter& emitter, const Camera& camera)
{
    auto& sky = Doom::skyState();

    // "Sky is allways drawn full bright, i.e. colormaps[0] is used. Because of
    // this hack, sky is not affected by INVUL inverse mapping" - drawPlanes,
    // whose words those are. Row 0 at any distance, and through any powerup.
    auto light = fixedLight(0);

    if (sky.skytexture <= 0 || sky.skytexture >= Doom::graphicsData().numtextures)
        return;

    auto texture = texturetranslation[sky.skytexture];

    // DOOM pins the sky to screen rows, with row 100 on the horizon. A screen
    // row is linear in height on the cylinder, so two rings are exact.
    auto rows = skyFocal * skyHeight / skyRadius;
    auto uv =
        QuadUv {0.0f, 0.0f, (100.0f - rows) / 128.0f, (100.0f + rows) / 128.0f};

    for (auto i = 0; i < skySegments; ++i)
    {
        auto a0 = angle_t {static_cast<std::uint32_t>(i)} << 26;
        auto a1 = angle_t {static_cast<std::uint32_t>(i + 1)} << 26;

        auto from =
            Point {camera.x + skyRadius * toDouble(finecosine[a0.fineIndex()]),
                   camera.y + skyRadius * toDouble(finesine[a0.fineIndex()])};
        auto to = Point {camera.x + skyRadius * toDouble(finecosine[a1.fineIndex()]),
                         camera.y + skyRadius * toDouble(finesine[a1.fineIndex()])};

        uv.uStart = 4.0f * static_cast<float>(i) / skySegments;
        uv.uEnd = 4.0f * static_cast<float>(i + 1) / skySegments;

        emitter.quad(
            texture,
            groundSpan(from, to, camera.z - skyHeight, camera.z + skyHeight),
            uv,
            light);
    }
}

void emitFlat(
    Emitter& emitter, int index, int flat, float height, const Light& light)
{
    auto textureId = Doom::graphicsData().numtextures + flattranslation[flat];
    auto first = cells.start[index];

    auto emitCorner = [&](int corner)
    {
        auto x = static_cast<float>(cells.corners[first + corner].x);
        auto y = static_cast<float>(cells.corners[first + corner].y);

        emitter.vertex(textureId, x, height, -y, x / flatSize, -y / flatSize, light);
    };

    for (auto i = 1; i + 1 < cells.count[index]; ++i)
    {
        emitCorner(0);
        emitCorner(i);
        emitCorner(i + 1);
    }
}

void emitSubsector(Emitter& emitter, int index)
{
    auto& sky = Doom::skyState();

    const auto* sector = subsectors[index].sector;

    if (cells.count[index] < 3 || sector == nullptr)
        return;

    auto light = sectorLight(sector->lightlevel, 0);

    // Either way round, a sky flat is a hole onto the sky rather than a surface:
    // drawPlanes paints the sky wherever it finds one, and a floor is as free to
    // carry it as a ceiling.
    if (sector->floorpic != sky.skyflatnum)
        emitFlat(emitter, index, sector->floorpic, floorHeight(*sector), light);

    if (sector->ceilingpic != sky.skyflatnum)
        emitFlat(emitter, index, sector->ceilingpic, ceilingHeight(*sector), light);
}

void emitWorld(Emitter& emitter, const Camera& camera)
{
    for (auto i = 0; i < numlines; ++i)
    {
        emitLineSide(emitter, lines[i], i, 0);
        emitLineSide(emitter, lines[i], i, 1);
    }

    for (auto i = 0; i < numsubsectors; ++i)
        emitSubsector(emitter, i);

    const auto* viewer = displayPlayer().mo;

    if (viewer == nullptr)
        return;

    emitSky(emitter, camera);
    emitSprites(emitter, *viewer, camera);
}

//
// The overlay: the software-only layers, captured off a scratch frame.
//

// One layer's pixels and the coverage that goes with them. Primed differently,
// the two passes agree only where the layer drew, so where they agree is exactly
// what it covered - which no single pass can tell, a drawn pixel being free to
// hold whatever value the buffer was primed with.
struct CapturedLayer
{
    Array<std::uint8_t, screenPixels> indices;
    Array<std::uint8_t, screenPixels> coverage;
};

Array<Array<std::uint8_t, screenPixels>, 2> overlayPasses;
CapturedLayer underLayer;
CapturedLayer menuLayer;

// What displayFrame draws over the view *before* the menu darkens the frame, in
// its order - and so what the menu darkens along with the world.
void drawUnderLayers()
{
    auto& overlay = Doom::overlayState();
    auto& view = Doom::viewWindow();

    if (Doom::gameFlow().gamestate != Doom::GameState::Level
        || !Doom::gameClock().gametic)
        return;

    if (overlay.automapactive)
        Doom::drawAutomapMarks();

    Doom::drawHud();

    if (Doom::refreshFlags().paused)
    {
        auto y = overlay.automapactive ? 4 : view.viewwindowy + 4;

        Doom::drawPatchDirect(
            view.viewwindowx + (view.scaledviewwidth - 68) / 2,
            y,
            0,
            static_cast<Doom::Patch*>(Doom::cacheLumpName("M_PAUSE")));
    }
}

void captureLayer(auto&& draw, CapturedLayer& into)
{
    auto* frame = screens[0];

    for (auto pass = 0; pass < 2; ++pass)
    {
        overlayPasses[pass].fill(pass == 0 ? 0x00 : 0xff);
        screens[0] = overlayPasses[pass].data();
        draw();
    }

    screens[0] = frame;

    for (auto i = 0; i < screenPixels; ++i)
    {
        into.indices[i] = overlayPasses[0][i];
        into.coverage[i] = overlayPasses[0][i] == overlayPasses[1][i] ? 255 : 0;
    }
}

//
// The automap, as geometry rather than as a rasterized frame.
//
// What is drawn, and in what colour, is drawAutomap's own choice, mirrored
// below: only its rasterizer (drawFline, a Bresenham walk straight into the
// 320 x 168 frame) is replaced. The shapes it draws the player and the things
// with - player_arrow, cheat_player_arrow, thintriangle_guy - and the rotation
// it puts them through are the engine's, used here as they stand.
//

struct AutomapEmitter
{
    void corner(const Point& at, const Point& direction, float side, float shade)
    {
        vertices[count++] = {
            {static_cast<float>(at.x), static_cast<float>(at.y)},
            {static_cast<float>(direction.x), static_cast<float>(direction.y)},
            side,
            shade};
    }

    // One line of the map, in frame coordinates, as the two triangles of a quad
    // the vertex shader widens.
    void frameLine(const Point& a, const Point& b, int color)
    {
        auto direction = Point {b.x - a.x, b.y - a.y};
        auto shade = static_cast<float>(color & 0xff);

        if (count + 6 > static_cast<int>(vertices.size())
            || (direction.x == 0.0 && direction.y == 0.0))
            return;

        corner(a, direction, 1.0f, shade);
        corner(b, direction, 1.0f, shade);
        corner(b, direction, -1.0f, shade);

        corner(a, direction, 1.0f, shade);
        corner(b, direction, -1.0f, shade);
        corner(a, direction, -1.0f, shade);
    }

    // CXMTOF and CYMTOF's transform, in floating point and without their
    // rounding to whole pixels: the map's y runs up and the frame's runs down.
    Point toFrame(fixed_t x, fixed_t y) const
    {
        return {f_x + (toDouble(x) - origin.x) * pixelsPerMapUnit,
                f_y + f_h - (toDouble(y) - origin.y) * pixelsPerMapUnit};
    }

    void line(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, int color)
    {
        frameLine(toFrame(x1, y1), toFrame(x2, y2), color);
    }

    // drawLineCharacter, emitting instead of rasterizing.
    void lineCharacter(std::span<const Doom::MapLine> shape,
                       fixed_t size,
                       angle_t angle,
                       int color,
                       fixed_t x,
                       fixed_t y)
    {
        for (const auto& original: shape)
        {
            auto l = original;

            if (size)
            {
                l.a.x = FixedMul(size, l.a.x);
                l.a.y = FixedMul(size, l.a.y);
                l.b.x = FixedMul(size, l.b.x);
                l.b.y = FixedMul(size, l.b.y);
            }

            if (angle)
            {
                Doom::rotateAutomapPoint(l.a.x, l.a.y, angle);
                Doom::rotateAutomapPoint(l.b.x, l.b.y, angle);
            }

            line(l.a.x + x, l.a.y + y, l.b.x + x, l.b.y + y, color);
        }
    }

    std::span<AutomapVertex> vertices;
    int count = 0;

    // The map point the frame's lower-left corner sits on, and how many frame
    // pixels one whole map unit spans. Both are in whole map units, which is
    // what toFrame's own arithmetic is in - see Point.
    Point origin;
    double pixelsPerMapUnit = 0.0;
};

void automapWalls(AutomapEmitter& emitter)
{
    for (auto i = 0; i < numlines; i++)
    {
        const auto& line = lines[i];

        auto draw = [&](int color)
        { emitter.line(line.v1->x, line.v1->y, line.v2->x, line.v2->y, color); };

        if (cheating || (line.flags & Doom::ML_MAPPED))
        {
            if ((line.flags & LINE_NEVERSEE) && !cheating)
                continue;

            if (!line.backsector)
                draw(WALLCOLORS + lightlev);
            else if (line.special == 39)
                draw(WALLCOLORS + WALLRANGE / 2);
            else if (line.flags & Doom::ML_SECRET)
                draw(cheating ? SECRETWALLCOLORS + lightlev : WALLCOLORS + lightlev);
            else if (line.backsector->floorheight != line.frontsector->floorheight)
                draw(FDWALLCOLORS + lightlev);
            else if (line.backsector->ceilingheight
                     != line.frontsector->ceilingheight)
                draw(CDWALLCOLORS + lightlev);
            else if (cheating)
                draw(TSWALLCOLORS + lightlev);
        }
        else if (am_plr != nullptr
                 && am_plr->powers[Doom::toIndex(Doom::PowerType::AllMap)])
        {
            if (!(line.flags & LINE_NEVERSEE))
                draw(GRAYS + 3);
        }
    }
}

void automapGrid(AutomapEmitter& emitter, int color)
{
    auto block = Doom::Fixed::fromInt(Doom::MAPBLOCKUNITS);
    auto originX = toFixed(emitter.origin.x);
    auto originY = toFixed(emitter.origin.y);

    // The first grid line at or past `from`, the grid being pinned to the
    // blockmap's own origin rather than to the map's.
    auto firstLine = [&](fixed_t from, fixed_t blockmapOrigin)
    {
        auto past = fixed_t {(from - blockmapOrigin).raw % block.raw};

        return past ? from + block - past : from;
    };

    for (auto x = firstLine(originX, bmaporgx); x < originX + m_w; x += block)
        emitter.line(x, originY, x, originY + m_h, color);

    for (auto y = firstLine(originY, bmaporgy); y < originY + m_h; y += block)
        emitter.line(originX, y, originX + m_w, y, color);
}

// Drawn from the view rather than from the player: the arrow is the one thing on
// the map that turns, and turning it once a tic against a map that glides is
// what would be seen.
void automapPlayer(AutomapEmitter& emitter, const Camera& camera)
{
    if (am_plr == nullptr || am_plr->mo == nullptr)
        return;

    auto x = toFixed(camera.x);
    auto y = toFixed(camera.y);
    auto angle = angleFromRadians(camera.angle);
    auto unscaled = fixed_t {};

    if (cheating)
        emitter.lineCharacter(cheat_player_arrow, unscaled, angle, WHITE, x, y);
    else
        emitter.lineCharacter(player_arrow, unscaled, angle, WHITE, x, y);
}

void automapThings(AutomapEmitter& emitter, int color)
{
    for (auto i = 0; i < numsectors; i++)
        for (auto* thing = sectors[i].thinglist; thing != nullptr;
             thing = thing->snext)
            emitter.lineCharacter(thintriangle_guy,
                                  Doom::Fixed::fromInt(16),
                                  thing->angle,
                                  color + lightlev,
                                  thing->x,
                                  thing->y);
}

// drawCrosshair pokes the frame's middle pixel; a line a pixel long over it is
// the same dot, and widens with everything else.
void automapCrosshair(AutomapEmitter& emitter, int color)
{
    auto x = f_w * 0.5;
    auto y = f_h * 0.5;

    emitter.frameLine({x - 0.5, y}, {x + 0.5, y}, color);
}

//
// The weapon and its muzzle flash.
//

// drawPSprite lights the weapon at the *nearest* entry of the scale table - it
// is right up against the camera - and that entry is this many rows brighter
// than the sector's start map. Reading the start map alone lights the weapon as
// if it stood infinitely far away, which is nearly black in a dim room and
// visibly dark in almost any room: DOOM's weapon is fullbright in every sector
// above light level 240 and close to it well below that.
float weaponBrightening()
{
    auto& view = Doom::viewWindow();

    auto width = view.viewwidth << view.detailshift;

    if (width <= 0)
        return 0.0f;

    return static_cast<float>((Doom::MAXLIGHTSCALE - 1) * Doom::SCREENWIDTH / width
                              / Doom::DISTMAP);
}

// drawPSprite's choice of colormap, in its own order: a powerup first, then a
// lit frame, then the sector.
float weaponLight(bool fullbright)
{
    if (fixedRow())
        return static_cast<float>(fixedRow());

    if (fullbright)
        return 0.0f;

    auto lightlevel = displayPlayer().mo->subsector->sector->lightlevel;

    return std::max(sectorLight(lightlevel, 0).row - weaponBrightening(), 0.0f);
}

// The engine places the weapon in a 320x200 space centred on row 100
// (BASEYCENTER), and drawPSprite lands that centre on the middle row of whatever
// the view is: row 84 with the status bar up, row 100 without it.
float weaponRowShift()
{
    return 100.0f - viewRows() * 0.5f;
}
} // namespace

//
// The interface itself.
//

void snapshotTic()
{
    if (Doom::gameFlow().gamestate != Doom::GameState::Level || sectors == nullptr
        || numsectors <= 0)
        return;

    snapshot.floor.resize(numsectors);
    snapshot.ceiling.resize(numsectors);

    for (auto i = 0; i < numsectors; ++i)
    {
        snapshot.floor[i] = toFloat(sectors[i].floorheight);
        snapshot.ceiling[i] = toFloat(sectors[i].ceilingheight);
    }
}

bool viewActive()
{
    return Doom::gameFlow().gamestate == Doom::GameState::Level
           && Doom::gameClock().gametic != 0;
}

bool automapActive()
{
    return Doom::overlayState().automapactive;
}

bool statusBarVisible()
{
    // drawStatusBar's own st_statusbaron, which is private to it: displayFrame
    // asks for a bar-less frame once the view fills all 200 rows, and the
    // automap keeps the bar whatever the screen size says.
    return Doom::viewWindow().viewheight != Doom::SCREENHEIGHT
           || Doom::overlayState().automapactive;
}

float viewRows()
{
    return statusBarVisible() ? static_cast<float>(Doom::ST_Y)
                              : static_cast<float>(Doom::SCREENHEIGHT);
}

void revealAutomap()
{
    auto& player = displayPlayer();

    if (Doom::gameFlow().gamestate != Doom::GameState::Level
        || !Doom::gameClock().gametic || !Doom::overlayState().automapactive
        || player.mo == nullptr)
        return;

    Doom::setupFrame(player);
    Doom::clearClipSegs();
    Doom::clearDrawSegs();
    Doom::clearPlanes();
    Doom::clearSprites();

    // Marking a line is storeWallRange's doing, and it does it as it draws the
    // wall - so the walls land in the frame the automap has just drawn itself
    // into (the column drawers write through ylookup, which was pointed at
    // screens[0] when the view size was set, and does not follow it anywhere).
    // Drawing the map again puts it back. The GPU path does not read that frame
    // for anything but the status bar, which the view never reaches; the
    // software one reads all of it.
    Doom::renderBSPNode(numnodes - 1);
    Doom::drawAutomap();
}

int darkenRow()
{
    // drawMenu only reaches its darkening once it is actually showing a menu: a
    // confirmation prompt draws its text and returns before then.
    if (Doom::overlayState().menuactive && !messageToPrint
        && (doom_flags & Doom::DOOM_FLAG_MENU_DARKEN_BG))
        return menuDarkenRow;

    return 0;
}

bool buildOverlay(std::span<std::uint8_t> outRgba)
{
    if (!fits(outRgba, screenPixels * 4))
        return false;

    auto flags = doom_flags;

    // Left to the GPU view, which can darken at full resolution and exactly (see
    // darkenRow). Were it left on, it would write to all 64000 pixels and the
    // whole screen would come back as covered.
    doom_flags &= ~Doom::DOOM_FLAG_MENU_DARKEN_BG;

    captureLayer(drawUnderLayers, underLayer);
    captureLayer(Doom::drawMenu, menuLayer);

    doom_flags = flags;

    auto covered = false;

    for (auto i = 0; i < screenPixels; ++i)
    {
        auto menu = menuLayer.coverage[i] != 0;
        auto under = underLayer.coverage[i] != 0;

        outRgba[i * 4 + 0] = menu ? menuLayer.indices[i] : underLayer.indices[i];

        // The menu darkens what was already on the screen and then draws itself
        // over it, so a message or the PAUSE graphic dims with the world it sits
        // on while the menu itself stays bright.
        outRgba[i * 4 + 1] = (under && !menu) ? 255 : 0;
        outRgba[i * 4 + 2] = 0;
        outRgba[i * 4 + 3] = (menu || under) ? 255 : 0;

        covered = covered || menu || under;
    }

    return covered;
}

void bindKeys()
{
    for (auto i = 0; i < numdefaults; ++i)
    {
        auto& entry = defaults[i];

        if (entry.defaultvalue != Doom::STRING_VALUE
            && entry.name.starts_with("key_"))
            *entry.location = entry.defaultvalue;
    }
}

double ticTime()
{
    auto sec = 0;
    auto usec = 0;

    doom_gettime(&sec, &usec);

    // currentTic's own expression, kept fractional instead of truncated, so this
    // steps from one tic to the next at exactly the moment the engine does. It
    // omits the engine's private start-of-run offset, which only shifts the
    // count by a whole number of tics and so changes neither the steps nor the
    // fraction.
    return static_cast<double>(sec) * Doom::TICRATE
           + static_cast<double>(usec) * Doom::TICRATE / 1000000.0;
}

bool isWiping()
{
    return Doom::gameFlow().is_wiping_screen;
}

bool buildWipe(std::span<std::uint8_t> outStart, std::span<std::uint8_t> outOffsets)
{
    const auto* start = wipe_scr_start;

    if (!Doom::gameFlow().is_wiping_screen || start == nullptr
        || !fits(outStart, screenPixels) || !fits(outOffsets, wipeColumns))
        return false;

    // wipe_melt_running is the melt's own "I have been set up" flag, and the only
    // one to test: exitMelt frees the column table but leaves the pointer to it
    // alone, so between melts it is non-null and dangling. Until the melt is set
    // up, the outgoing screen is still row-major and nothing has slid.
    if (!wipe_melt_running)
    {
        std::copy_n(start, screenPixels, outStart.begin());
        std::fill_n(outOffsets.begin(), wipeColumns, std::uint8_t {0});

        return true;
    }

    for (auto column = 0; column < wipeColumns; ++column)
    {
        // A column that has not started moving sits at a negative offset; it has
        // slid nothing, which is what zero says.
        auto slid = std::clamp(wipe_melt_offsets[column], 0, screenHeight);

        outOffsets[column] = static_cast<std::uint8_t>(slid);

        // initMelt leaves the outgoing screen column-major - a column of
        // two-pixel shorts - because that is how the melt walks it. A texture
        // wants it back the way round it was. Copying the two bytes rather than
        // the short keeps this independent of byte order.
        for (auto row = 0; row < screenHeight; ++row)
        {
            const auto* source = start + (column * screenHeight + row) * 2;
            auto* destination = outStart.data() + row * screenWidth + column * 2;

            destination[0] = source[0];
            destination[1] = source[1];
        }
    }

    return true;
}

int mouseSensitivity()
{
    return Doom::menuSettings().mouseSensitivity;
}

Camera camera()
{
    const auto& player = displayPlayer();

    if (player.mo == nullptr)
        return {};

    return {toFloat(player.mo->x),
            toFloat(player.mo->y),
            toFloat(player.viewz),
            static_cast<float>(static_cast<double>(player.mo->angle.raw)
                               * (std::numbers::pi / halfTurn))};
}

void readPalette(std::span<std::uint8_t> outRgba)
{
    if (!fits(outRgba, 256 * 4))
        return;

    for (auto i = 0; i < 256; ++i)
    {
        outRgba[i * 4 + 0] = screen_palette[i * 3 + 0];
        outRgba[i * 4 + 1] = screen_palette[i * 3 + 1];
        outRgba[i * 4 + 2] = screen_palette[i * 3 + 2];
        outRgba[i * 4 + 3] = 255;
    }
}

int textureCount()
{
    auto& gfx = Doom::graphicsData();

    if (gfx.numtextures <= 0 || textures == nullptr)
        return 0;

    return gfx.numtextures + gfx.numflats + gfx.numspritelumps;
}

TextureInfo textureInfo(int id)
{
    ensureTextureTable();

    if (!textureTable.ready() || id < 0 || id >= textureTable.infos.size())
        return {};

    return textureTable.infos[id];
}

void readTexturePixels(int id, std::span<std::uint8_t> out)
{
    auto& gfx = Doom::graphicsData();

    auto info = textureInfo(id);
    auto pixels = info.width * info.height;

    if (pixels <= 0 || !fits(out, pixels * (info.masked ? 4 : 1)))
        return;

    if (id >= gfx.numtextures && id < spriteBase())
    {
        const auto* flat = static_cast<const byte*>(
            Doom::cacheLumpNum(gfx.firstflat + id - gfx.numtextures));

        std::copy_n(flat, flatSize * flatSize, out.begin());
        return;
    }

    auto image = id < gfx.numtextures
                     ? decodeWall(id)
                     : decodeSprite(gfx.firstspritelump + (id - spriteBase()), info);

    // A masked texture carries its coverage in alpha, so the shader can throw the
    // empty pixels away; everything else is a bare index.
    if (!info.masked)
    {
        std::copy_n(image.indices.data(), pixels, out.begin());
        return;
    }

    for (auto i = 0; i < pixels; ++i)
    {
        out[i * 4 + 0] = image.indices[i];
        out[i * 4 + 1] = 0;
        out[i * 4 + 2] = 0;
        out[i * 4 + 3] = image.alpha[i];
    }
}

void readColormaps(std::span<std::uint8_t> out)
{
    if (colormaps == nullptr || !fits(out, 256 * colormapRows))
        return;

    std::copy_n(colormaps, 256 * colormapRows, out.begin());
}

WorldGeometry
    buildGeometry(const Camera& camera, float alpha, const GeometryBuffers& into)
{
    auto count = textureCount();

    snapshot.alpha = std::clamp(alpha, 0.0f, 1.0f);
    ensureTextureTable();

    if (Doom::gameFlow().gamestate != Doom::GameState::Level || into.vertices.empty()
        || into.draws.empty() || count <= 0 || lines == nullptr
        || !textureTable.ready())
        return {};

    ensureLevel();

    if (cells.corners.empty())
        return {};

    scratch.counts.assign(count, 0);
    scratch.cursors.assign(count, 0);

    auto emitter = Emitter {};
    emitter.counts = {scratch.counts.data(), static_cast<std::size_t>(count)};
    emitter.cursors = {scratch.cursors.data(), static_cast<std::size_t>(count)};

    emitWorld(emitter, camera);

    // Each texture's vertices become one contiguous run, so the frame draws once
    // per texture with no state changes in between.
    auto total = 0;
    auto drawCount = 0;

    for (auto i = 0; i < count; ++i)
    {
        auto vertexCount = scratch.counts[i];

        if (vertexCount <= 0 || drawCount >= static_cast<int>(into.draws.size())
            || total + vertexCount > static_cast<int>(into.vertices.size()))
        {
            scratch.cursors[i] = -1;
            continue;
        }

        scratch.cursors[i] = total;
        into.draws[drawCount++] = {i, total, vertexCount};
        total += vertexCount;
    }

    emitter.vertices = into.vertices;
    emitWorld(emitter, camera);

    return {into.vertices.first(static_cast<std::size_t>(total)),
            into.draws.first(static_cast<std::size_t>(drawCount))};
}

std::span<const AutomapVertex> buildAutomap(const Camera& camera,
                                            std::span<AutomapVertex> into)
{
    if (!Doom::overlayState().automapactive
        || Doom::gameFlow().gamestate != Doom::GameState::Level || into.empty()
        || lines == nullptr)
        return {};

    auto emitter = AutomapEmitter {};
    emitter.vertices = into;

    // MTOF, as a plain number: the engine's own is FixedMul(x, scale_mtof) shifted
    // back down twice, which is x_raw * scale_mtof_raw / 2^32 - so one whole map
    // unit spans scale_mtof whole frame pixels.
    emitter.pixelsPerMapUnit = toDouble(scale_mtof);

    // Vanilla recentres on the player once a tic and snaps the map to whole frame
    // pixels as it does it (doFollowPlayer's FTOM(MTOF(x))). Following the
    // interpolated view instead, and not rounding, is what makes the map glide
    // rather than crawl. Panned by hand, it is the engine's window that moves,
    // and that still steps.
    if (followplayer && am_plr != nullptr && am_plr->mo != nullptr)
        emitter.origin = {camera.x - toDouble(m_w) / 2.0,
                          camera.y - toDouble(m_h) / 2.0};
    else
        emitter.origin = {toDouble(m_x), toDouble(m_y)};

    if (grid)
        automapGrid(emitter, GRIDCOLORS);

    automapWalls(emitter);
    automapPlayer(emitter, camera);

    if (cheating == 2)
        automapThings(emitter, THINGCOLORS);

    automapCrosshair(emitter, XHAIRCOLORS);

    return into.first(static_cast<std::size_t>(emitter.count));
}

Array<HudSprite, hudSpriteCount> hudSprites()
{
    auto& gfx = Doom::graphicsData();

    auto out = Array<HudSprite, hudSpriteCount> {};
    const auto& player = displayPlayer();

    ensureTextureTable();

    if (!textureTable.ready() || Doom::gameFlow().gamestate != Doom::GameState::Level
        || player.mo == nullptr)
        return out;

    for (auto i = 0; i < Doom::numPSprites && i < hudSpriteCount; ++i)
    {
        const auto& weapon = player.psprites[i];
        const auto* state = weapon.state;

        if (state == nullptr || Doom::toIndex(state->sprite) < 0
            || Doom::toIndex(state->sprite) >= gfx.numsprites)
            continue;

        const auto& definition = sprites[Doom::toIndex(state->sprite)];
        auto frameIndex = static_cast<int>(state->frame & Doom::FF_FRAMEMASK);

        if (frameIndex >= definition.numframes)
            continue;

        const auto& frame = definition.spriteframes[frameIndex];
        auto lump = frame.lump[0];

        if (lump < 0 || lump >= gfx.numspritelumps)
            continue;

        out[i] = {spriteBase() + lump,
                  toFloat(weapon.sx) - toFloat(spriteoffset[lump]),
                  toFloat(weapon.sy) - toFloat(spritetopoffset[lump])
                      - weaponRowShift(),
                  static_cast<float>(spritewidth[lump].toInt()),
                  static_cast<float>(spriteHeight(lump)),
                  weaponLight((state->frame & Doom::FF_FULLBRIGHT) != 0),
                  frame.flip[0] != 0};
    }

    return out;
}
} // namespace PureDoom::Engine
