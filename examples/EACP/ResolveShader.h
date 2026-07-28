#pragma once

#include "DoomShader.h"

#include <tuple>

namespace PureDoom
{
// drawFuzzColumn's own distortion table: 50 entries, each the row above or the
// row below, as vanilla walks it a pixel at a time. Written here as +-1 rows
// because a row on the screen is no longer a row of the frame - at the window's
// resolution it is fuzzGrain pixels.
inline constexpr auto fuzzTable =
    std::array {1.0f,  -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f,
                1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,
                -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,
                -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,
                -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f};

inline constexpr auto fuzzTableSize = (int) fuzzTable.size();

// The COLORMAP row a fuzz pixel is remapped through: "a bit brighter than
// average", as drawFuzzColumn's own comment puts it.
inline constexpr auto fuzzRow = 6.0f;

// How far along the table one column moves from the last. Vanilla's cursor runs
// down a column and straight on into the next, so the offset between neighbours
// is however tall the previous one was - which a fragment shader cannot know,
// having no idea what was drawn before it. Any step coprime with the table's
// length scatters the same way; this one is arbitrary and only has to not be 0.
inline constexpr auto fuzzColumnStep = 7;

// The world target resolved onto the screen: the pass that turns DOOM's own
// frame - one palette index per pixel - into colours, and the only place a
// spectre exists.
//
// It is one full-screen pass over the 3D viewport, and it is the price of the
// fuzz: a pass cannot sample the target it is drawing into, so the world has to
// be finished before anything can read it. What it buys is that the pixels being
// distorted are still indices, so the distortion is vanilla's own - a COLORMAP
// row applied to the frame behind the sprite - rather than an imitation of it in
// colour space.
struct ResolveShader final : ScreenQuadShader
{
    ResolveShader() { compile(); }

    void define() override
    {
        auto corner = quadCorner();
        setQuadPosition(corner);

        // The quad covers the 3D viewport, and the target holds the world where
        // the world was drawn - the same place - so a fragment's own position in
        // the window, in the target's pixels, is what it reads.
        auto place = float2(dstOrigin.x() + corner.x() * dstSize.x(),
                            dstOrigin.y() + corner.y() * dstSize.y());

        auto pixel = varying(float2(place.x() / viewSize.x() * targetSize.x(),
                                    place.y() / viewSize.y() * targetSize.y()));

        auto x = boundedX(toInt(pixel.x()));
        auto texel = fetch(world, int2(x, boundedY(toInt(pixel.y()))));

        // The table is indexed by the pixel's place in DOOM's own frame rather
        // than in the window's, so the grain stays the size it was in 1993
        // however large the window is, and the mark and the pixel it reads are
        // a whole frame row apart.
        auto column = toInt(pixel.x() / fuzzGrain.x());
        auto row = toInt(pixel.y() / fuzzGrain.y());
        auto slot =
            (row + column * fuzzColumnStep + toInt(fuzzPhase)) % fuzzTableSize;

        auto offsets = std::apply(
            [this](auto... rows) { return array(constant(rows)...); }, fuzzTable);

        auto offset = offsets[min(max(slot, 0), fuzzTableSize - 1)];
        auto beneath = fetch(
            world, int2(x, boundedY(toInt(pixel.y() + offset * fuzzGrain.y()))));

        // Green is the mark FuzzShader left, and nothing else in the frame
        // writes it.
        auto fuzzed = remap(indexOf(beneath), constant(fuzzRow));
        auto index = select(texel.y() > 0.5f, fuzzed, indexOf(texel));

        setPaletteFragment(darkened(index));
    }

    GPU::Int boundedX(const GPU::Int& x) { return bounded(x, targetSize.x()); }
    GPU::Int boundedY(const GPU::Int& y) { return bounded(y, targetSize.y()); }

    // A fetch outside the texture reads zero, which for an index means black, so
    // the edges of the frame would fringe rather than repeat their last row.
    GPU::Int bounded(const GPU::Int& value, const GPU::Float& size)
    {
        return min(max(value, 0), toInt(size) - 1);
    }

    GPU::Uniform<GPU::Texture2D> world;

    // The target's size in pixels, and how many of them one column and one row
    // of DOOM's 320x200 frame occupy.
    GPU::Uniform<GPU::Float2> targetSize;
    GPU::Uniform<GPU::Float2> fuzzGrain;

    // Where the engine's own fuzz walk has reached (Engine::fuzzPhase), so the
    // distortion animates the way vanilla's does: it advances with the pixels
    // drawn, and stands still when no spectre is in view.
    GPU::Uniform<GPU::Float> fuzzPhase;

    EACP_SHADER(viewSize,
                dstOrigin,
                dstSize,
                darkenRow,
                targetSize,
                fuzzGrain,
                fuzzPhase,
                world,
                colormap,
                palette)
};
} // namespace PureDoom
