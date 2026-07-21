// The port's world-geometry builder, tested headlessly.
//
// examples/EACP is the one part of this repository with no gate on it: the app
// is not built by CI, EngineAccess.cpp includes the engine's headers directly,
// and nothing has ever run Engine::buildGeometry outside a live GPU frame. That
// blind spot is exactly where the first Windows build's missing floors lived -
// the software renderer's frame goldens were green throughout, because they do
// not exercise this code at all.
//
// EngineAccess.cpp needs no GPU (it includes only DOOM headers and the container
// vocabulary), so the builder can be driven from a test binary and its output
// inspected as data. That is what this file does: load E1M1, ask for one frame of
// geometry, and assert that the three kinds of surface DOOM's world is made of
// are all present.
//
// The classifier deliberately reads the *vertices* rather than the texture ids.
// A floor or a ceiling is the only geometry whose triangle is horizontal - all
// three corners at one height - because walls are vertical quads and things are
// camera-facing billboards. So "did any floor come out?" is answerable from the
// buffer alone, with no reference to the id space, no knowledge of which lump is
// a flat, and nothing to keep in step with the emitter if it is rewritten.

#include "../Common.h"

#include <EngineAccess.h>

#include <DOOM/Render/GraphicsData.h>

#include <cmath>
#include <span>
#include <vector>

using namespace nano;
using namespace PureDoom;

namespace
{
constexpr int e1 = 1;
constexpr int m1 = 1;
constexpr int skillMedium = 2;

// The app's own ceilings (PureDoom::maxVertices / maxDraws), so this asks for a
// frame the same shape the renderer does.
constexpr int maxVertices = 262144;
constexpr int maxDraws = 2048;

struct Surfaces
{
    int horizontal = 0;
    int vertical = 0;
};

// The scratch a frame is built into, and the frame built into it. Held together
// so each case can ask for one line and then read the result as data.
struct Frame
{
    Frame()
    {
        vertices.resize(maxVertices);
        draws.resize(maxDraws);
    }

    Engine::WorldGeometry build()
    {
        return Engine::buildGeometry(Engine::camera(), 0.0f, {vertices, draws});
    }

    std::vector<Engine::WorldVertex> vertices;
    std::vector<Engine::TextureDraw> draws;
};

// A triangle counts as horizontal when its three corners share a height. The
// emitter writes floors and ceilings at a single `height` per subsector, so this
// is exact rather than approximate - but it is compared with a tolerance anyway,
// since the values arrive as floats and a future emitter is free to interpolate
// them.
Surfaces classify(std::span<const Engine::WorldVertex> vertices)
{
    auto result = Surfaces {};

    for (auto i = 0u; i + 2 < vertices.size(); i += 3)
    {
        auto a = vertices[i].position[1];
        auto b = vertices[i + 1].position[1];
        auto c = vertices[i + 2].position[1];

        auto flat = std::fabs(a - b) < 0.001f && std::fabs(a - c) < 0.001f;

        if (flat)
            result.horizontal++;
        else
            result.vertical++;
    }

    return result;
}

// The horizontal triangles inside one draw's run - the floors and ceilings drawn
// with that draw's texture.
int horizontalTrianglesIn(std::span<const Engine::WorldVertex> vertices,
                          const Engine::TextureDraw& draw)
{
    auto count = 0;
    auto end = draw.firstVertex + draw.vertexCount;

    for (auto v = draw.firstVertex; v + 2 < end; v += 3)
    {
        auto a = vertices[v].position[1];
        auto b = vertices[v + 1].position[1];
        auto c = vertices[v + 2].position[1];

        if (std::fabs(a - b) < 0.001f && std::fabs(a - c) < 0.001f)
            count++;
    }

    return count;
}

// One frame of world geometry from a freshly loaded E1M1, at the player's own
// spawn - which is where every demo starts, and a spot with floor, ceiling and
// walls all in view.
auto tWorldGeometryHasFloors = test("Port/worldGeometryHasFloors") = []
{
    check(doomSimBoot() != 0, "the engine booted");
    check(doomSimLoadLevel(e1, m1, skillMedium) != 0, "E1M1 loaded");

    auto frame = Frame {};
    auto world = frame.build();

    check(!world.draws.empty(), "the builder emitted at least one draw");
    check(!world.vertices.empty(), "the builder emitted at least one vertex");

    auto surfaces = classify(world.vertices);

    // The two assertions that matter, and they are independent: walls coming out
    // while floors do not is precisely the Windows symptom, and a test that only
    // counted total vertices would have passed straight through it.
    check(surfaces.vertical > 0, "walls and things were emitted");
    check(surfaces.horizontal > 0, "floors and ceilings were emitted");
};

// The textures the floors are actually drawn with, fetched the way the renderer
// fetches them.
//
// The geometry coming out right does not mean a floor appears: the surface still
// has to be handed a texture. This walks the draws whose triangles are horizontal
// - the floors and ceilings, found from the vertices rather than from the id
// space, as above - and asks Engine::readTexturePixels for each one's pixels, the
// same call View::textureFor makes on first use.
//
// A flat is a raw 64x64 lump of palette indices, so what is checked is that the
// port reports those dimensions, does not claim the texture is masked (a flat has
// no holes, and claiming otherwise would send it up as RGBA from a buffer only
// large enough for one byte per pixel), and hands back something other than a
// single repeated index - an all-one-value block being what an unwritten or
// zero-filled buffer looks like, and indistinguishable from a real texture by any
// check on the call's return.
auto tFloorTexturesAreReadable = test("Port/floorTexturesAreReadable") = []
{
    check(doomSimBoot() != 0, "the engine booted");
    check(doomSimLoadLevel(e1, m1, skillMedium) != 0, "E1M1 loaded");

    auto frame = Frame {};
    auto world = frame.build();

    check(!world.draws.empty(), "the builder emitted at least one draw");

    auto floorTextures = 0;

    for (const auto& draw: world.draws)
    {
        // Only the runs that actually contain a horizontal triangle, which is
        // what makes this a test of the *floor* textures rather than of all of
        // them.
        if (horizontalTrianglesIn(world.vertices, draw) == 0)
            continue;

        // The id must be in the *flat* range, not merely 64x64 - plenty of wall
        // textures are 64x64 too, so the dimensions alone would not have caught a
        // floor drawn with a wall's texture.
        check(draw.textureId >= Doom::graphicsData().numtextures,
              "a floor's texture id is in the flat range");

        auto info = Engine::textureInfo(draw.textureId);

        check(info.width == 64 && info.height == 64,
              "a floor's texture is a 64x64 flat");
        check(!info.masked, "a flat is not masked");

        // The UVs a flat is tiled by are world coordinates over 64, so they run to
        // tens or hundreds rather than the 0..4 a wall sees. That is the one
        // property in which floor geometry differs from wall geometry, so it is
        // worth pinning: it must vary across the triangle (a constant UV samples
        // one texel and draws the surface as a single flat colour) and it must
        // stay finite.
        auto run = world.vertices.subspan(draw.firstVertex, draw.vertexCount);
        const auto& first = run.front();
        auto varyingUv = false;

        for (const auto& vertex: run)
        {
            check(std::isfinite(vertex.uv[0]) && std::isfinite(vertex.uv[1]),
                  "a flat's UV is finite");

            if (vertex.uv[0] != first.uv[0] || vertex.uv[1] != first.uv[1])
                varyingUv = true;
        }

        check(varyingUv, "a flat's UVs vary across its surface");

        auto pixels = std::vector<unsigned char>(
            static_cast<std::size_t>(info.width * info.height), 0);

        Engine::readTexturePixels(draw.textureId, pixels);

        auto varied = false;

        for (auto& pixel: pixels)
            if (pixel != pixels.front())
                varied = true;

        check(varied, "the flat's pixels are not one repeated index");

        floorTextures++;
    }

    check(floorTextures > 0, "at least one floor texture was examined");
};

// Every draw must name a texture that exists and a run that lies inside the
// buffer the builder was handed. A textureId outside the id space would be an
// out-of-bounds read the moment the renderer looked it up - which is invisible
// in a frame, since a wrong texture still draws something.
auto tWorldGeometryDrawsAreInRange = test("Port/worldGeometryDrawsAreInRange") = []
{
    check(doomSimBoot() != 0, "the engine booted");
    check(doomSimLoadLevel(e1, m1, skillMedium) != 0, "E1M1 loaded");

    auto frame = Frame {};
    auto world = frame.build();
    auto textureCount = Engine::textureCount();

    check(!world.draws.empty(), "the builder emitted at least one draw");
    check(textureCount > 0, "the engine loaded a texture id space");

    for (const auto& draw: world.draws)
    {
        check(draw.textureId >= 0 && draw.textureId < textureCount,
              "the draw's texture is inside the id space");
        check(draw.vertexCount > 0, "the draw covers at least one vertex");
        check(draw.firstVertex >= 0
                  && draw.firstVertex + draw.vertexCount
                         <= static_cast<int>(world.vertices.size()),
              "the draw's run lies inside the emitted vertices");
    }
};
} // namespace
