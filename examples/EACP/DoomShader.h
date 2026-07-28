#pragma once

#include "Common.h"

namespace PureDoom
{
struct ScreenVertex
{
    float corner[2] = {};
};

inline constexpr ScreenVertex unitQuad[] = {
    {{0.0f, 0.0f}},
    {{1.0f, 0.0f}},
    {{1.0f, 1.0f}},
    {{0.0f, 0.0f}},
    {{1.0f, 1.0f}},
    {{0.0f, 1.0f}},
};

// Every pixel the port draws resolves the way the software renderer resolves
// one: a palette index is remapped by the COLORMAP row that the surface's light
// and distance pick, and the palette turns the result into a colour. Indices
// travel as 0..255 floats, so a texture's 0..1 sample is scaled back up on the
// way in and a lookup row's sample on the way out.
//
// Both lookup tables are read with fetch rather than sample: a table is
// addressed by an index the shader already holds, and fetch takes that index as
// one, so the half-texel arithmetic that used to stand between the two - and
// the Nearest sampler that had to round back to the texel it was aimed at - is
// gone. What the sampler was silently supplying is written out below instead.
struct DoomShader : GPU::ShaderProgram
{
    GPU::Float indexOf(const GPU::Float4& texel) { return texel.x() * 255.0f; }

    // fetch truncates towards zero, so this is where the rounding lives now. An
    // index arrives as a unorm scaled back up, which lands a hair either side of
    // the whole number it means.
    GPU::Int texelOf(const GPU::Float& index) { return toInt(index + 0.5f); }

    GPU::Float remap(const GPU::Float& index, const GPU::Float& row)
    {
        // The row is a continuous falloff, and at close range it runs tens of
        // rows off the bright end of the table - WorldShader subtracts up to 64
        // of them at the near plane. A texel outside the texture fetches as zero,
        // where the Clamp address mode used to hold it at the first row, so the
        // clamp is the shader's to make now. It bounds the table rather than the
        // light levels: rows 32 and 33 are the invulnerability and blackout
        // maps, which a surface locked to one of them arrives here already
        // carrying.
        auto bounded = clamp(row, 0.0f, (float) Engine::colormapRows - 1.0f);
        return indexOf(fetch(colormap, int2(texelOf(index), texelOf(bounded))));
    }

    // Row 0 is the identity, so playing costs the lookup and nothing else.
    GPU::Float darkened(const GPU::Float& index) { return remap(index, darkenRow); }

    void setPaletteFragment(const GPU::Float& index)
    {
        auto color = fetch(palette, int2(texelOf(index), 0));
        setFragment(float4(color.xyz(), 1.0f));
    }

    // The other thing a shader here can write: the index itself, unresolved.
    // What the world renders into is DOOM's own frame - one palette index per
    // pixel, already through its COLORMAP row - because a spectre is a
    // distortion of the indices behind it and has to be applied while they are
    // still indices (ResolveShader). Green carries the fuzz mark, and blue and
    // alpha nothing; a single-channel render target would do for the index
    // alone, and eacp has no such PixelFormat (see the gap log).
    void setIndexFragment(const GPU::Float& index)
    {
        setFragment(float4(index / 255.0f, 0.0f, 0.0f, 1.0f));
    }

    GPU::Uniform<GPU::Float> darkenRow;
    GPU::Uniform<GPU::Texture2D> colormap;
    GPU::Uniform<GPU::Texture2D> palette;
};

// A quad drawn over a destination rect in view points, which the unit quad's
// corner is mapped onto and then into clip space.
struct ScreenQuadShader : DoomShader
{
    GPU::Float2 quadCorner() { return vertexInput(&ScreenVertex::corner); }

    void setQuadPosition(const GPU::Float2& corner)
    {
        setViewPosition(dstOrigin.x() + corner.x() * dstSize.x(),
                        dstOrigin.y() + corner.y() * dstSize.y());
    }

    void setViewPosition(const GPU::Float& x, const GPU::Float& y)
    {
        auto ndcX = x / viewSize.x() * 2.0f - 1.0f;
        auto ndcY = 1.0f - y / viewSize.y() * 2.0f;
        setPosition(float4(ndcX, ndcY, 0.0f, 1.0f));
    }

    void setDestination(const Graphics::Rect& bounds, const Graphics::Rect& dst)
    {
        viewSize = std::array {bounds.w, bounds.h};
        dstOrigin = std::array {dst.x, dst.y};
        dstSize = std::array {dst.w, dst.h};
    }

    GPU::Uniform<GPU::Float2> viewSize;
    GPU::Uniform<GPU::Float2> dstOrigin;
    GPU::Uniform<GPU::Float2> dstSize;
};

// The level as hardware 3D, at the window's resolution. DOOM's map coordinates
// (x, y on the ground, z up) arrive as (x, z, -y), and the full-frame projection
// is squeezed into the 3D viewport's sub-rect of the window - offsets scale by w
// so they survive the perspective divide - leaving the status bar and the
// letterbox bars alone.
//
// Two shaders draw off the world's vertex buffer - the surfaces, and the
// spectres' silhouettes marked over them - and they share this because they have
// to agree to the pixel: a mark that landed anywhere but where the sprite was
// would fuzz something else.
struct WorldViewShader : DoomShader
{
    WorldViewShader()
    {
        // Wall textures and flats tile across a surface, so they repeat. A floor
        // needs it most: its UVs are world coordinates over 64, running to
        // hundreds, where clamping would sample one texel and draw the whole
        // surface a single flat colour.
        //
        // Declared here rather than on the Texture because the sampler is fixed
        // when the shader compiles - see GPU::TextureSampling. Set before
        // compile(), which each derived shader calls.
        texture.sampling = {GPU::TextureFilter::Nearest,
                            GPU::TextureAddressMode::Repeat};
    }

    // Places a world vertex on the screen and hands back the view depth, which
    // the projection's w already is.
    GPU::Float setWorldPosition(const GPU::Float3& position)
    {
        auto view = rotateY(-yaw) * translate(-camX, -camY, -camZ);
        auto fovY = 2.0f * std::atan(1.0f / worldAspect);
        auto projection = perspective(constant(worldAspect), fovY, 4.0f, 16384.0f);
        auto clip = projection * view * float4(position, 1.0f);

        auto x = clip.x() * ndcScale.x() + clip.w() * ndcOffset.x();
        auto y = clip.y() * ndcScale.y() + clip.w() * ndcOffset.y();
        setPosition(float4(x, y, clip.z(), clip.w()));

        return clip.w();
    }

    // Where the frame is drawn from, and where in the window the 3D view sits.
    //
    // The projection is built for the view with the status bar up, so a taller
    // view has to widen its vertical field of view by the same proportion -
    // which on a perspective projection is one scale on y, and the viewport
    // already applies one.
    void setView(const Engine::Camera& camera,
                 const Graphics::Rect& bounds,
                 const Graphics::Rect& viewport,
                 float rows)
    {
        camX = camera.pos.x;
        camY = camera.pos.z;
        camZ = -camera.pos.y;
        yaw = camera.angle - pi / 2.0f;

        ndcScale =
            std::array {viewport.w / bounds.w,
                        viewport.h / bounds.h * (viewRowsWithStatusBar / rows)};

        ndcOffset =
            std::array {(viewport.x + viewport.w * 0.5f) / bounds.w * 2.0f - 1.0f,
                        1.0f - (viewport.y + viewport.h * 0.5f) / bounds.h * 2.0f};
    }

    GPU::Uniform<GPU::Float> camX;
    GPU::Uniform<GPU::Float> camY;
    GPU::Uniform<GPU::Float> camZ;
    GPU::Uniform<GPU::Float> yaw;
    GPU::Uniform<GPU::Float2> ndcScale;
    GPU::Uniform<GPU::Float2> ndcOffset;

    // Rebound per draw: the frame's geometry is grouped by texture.
    GPU::Uniform<GPU::Texture2D> texture;
};
} // namespace PureDoom
