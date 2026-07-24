// The port's automap builder, tested headlessly - the companion to
// Port/GeometryTests.cpp, and for the same reason: nothing had ever run
// Engine::buildAutomap outside a live GPU frame.
//
// Tests/Sim/AutomapTests.cpp covers the *engine's* automap, by hashing the frame
// AM_Drawer rasterizes. It cannot see this code at all: the port re-implements
// AM_Drawer's decisions as geometry and only its rasterizer is shared with
// nothing. So a transform that put every line of the map in the wrong place left
// automap.frames green, which is exactly what happened - the Doom::Fixed strong-type
// sweep turned `(double) x1` into a whole-units conversion while the origin it is
// measured against stayed in raw fixed-point, and the map collapsed to a point
// about ninety pixels left of where it belonged.
//
// Both cases below are stated against the frame the engine hands the port
// (f_x/f_y/f_w/f_h and the m_x/m_w window), so neither depends on the emitter's
// ordering or on how many lines E1M1 happens to have.

#include "../Common.h"

#include <EngineAccess.h>

#include <DOOM/DOOM.h>
#include <DOOM/UI/AutomapTypes.h>
#include <DOOM/UI/AutomapView.h>

#include <cmath>
#include <vector>

using namespace nano;
using namespace PureDoom;

namespace
{
constexpr auto e1 = 1;
constexpr auto m1 = 1;
constexpr auto skillMedium = 2;

// Doom::GameState::Level, as doomSimGameState reports it.
constexpr auto gsLevel = 0;

constexpr auto maxAutomapVertices = 131072;

struct Bounds
{
    float minX = 1e9f;
    float maxX = -1e9f;
    float minY = 1e9f;
    float maxY = -1e9f;

    void add(const Engine::AutomapVertex& vertex)
    {
        minX = std::fmin(minX, vertex.position[0]);
        maxX = std::fmax(maxX, vertex.position[0]);
        minY = std::fmin(minY, vertex.position[1]);
        maxY = std::fmax(maxY, vertex.position[1]);
    }

    float width() const { return maxX - minX; }
    float height() const { return maxY - minY; }
};

Bounds boundsOf(std::span<const Engine::AutomapVertex> vertices)
{
    auto bounds = Bounds {};

    for (const auto& vertex: vertices)
        bounds.add(vertex);

    return bounds;
}

// E1M1 with the map open, following the player - which is the state the game is
// in the moment Tab is pressed, and the one the GPU renderer draws most.
void openAutomapOnE1M1()
{
    check(doomSimBoot() != 0, "engine booted headless, no demo queued");
    check(doomSimLoadLevel(e1, m1, skillMedium) != 0, "E1M1 loaded");

    // A level load wipes as any level transition does; run it out first.
    doomSimStepTic();

    for (auto guard = 0; doomSimIsWiping() && guard < 200; ++guard)
        doomSimStepTic();

    check(doomSimGameState() == gsLevel, "the level is up");

    doomSimPostKeyDown(static_cast<int>(Doom::Key::Tab));
    doomSimPostKeyUp(static_cast<int>(Doom::Key::Tab));
    check(doomSimStepTic() != 0, "the tic ran");
    check(doomSimAutomapActive() != 0, "AM_STARTKEY opened the automap");
    check(Doom::automapView().followplayer != 0,
          "the map starts following the player");
}

// Following the player, the map window is centred on where the player stands -
// so the player's own position lands on the middle of the frame, and the arrow is
// drawn at exactly that position.
//
// This is the assertion the unit mix-up broke, and it breaks it hard: the origin
// was subtracted in raw fixed-point from a coordinate in whole units, which put
// the arrow about ninety pixels left of centre.
//
// The arrow is picked out of the buffer by position rather than by texture or
// colour, because the emitter has no id space to ask: buildAutomap emits the grid
// (off here), then the walls, then the arrow, then the things (cheat only), then
// the crosshair - so the arrow is the 42 corners (NUMPLYRLINES lines of six)
// before the crosshair's six. The crosshair is checked first, and separately: it
// is emitted in frame coordinates directly, without the transform under test, so
// finding it on the frame's centre is what proves the tail really is the tail.
auto tAutomapIsCentredOnThePlayer = test("Port/automapIsCentredOnThePlayer") = []
{
    openAutomapOnE1M1();

    auto buffer = std::vector<Engine::AutomapVertex>(maxAutomapVertices);
    auto map = Engine::buildAutomap(Engine::camera(), buffer);

    constexpr auto cornersPerLine = 6u;
    constexpr auto arrowCorners = NUMPLYRLINES * cornersPerLine;

    check(map.size() >= arrowCorners + cornersPerLine, "the arrow was emitted");

    auto centreX = static_cast<float>(Doom::automapView().f_x)
                   + static_cast<float>(Doom::automapView().f_w) * 0.5f;
    auto centreY = static_cast<float>(Doom::automapView().f_y)
                   + static_cast<float>(Doom::automapView().f_h) * 0.5f;

    auto crosshair = boundsOf(map.last(cornersPerLine));

    check(std::fabs(crosshair.minX - centreX) < 2.0f
              && std::fabs(crosshair.maxX - centreX) < 2.0f,
          "the tail of the buffer is the crosshair, on the frame's centre");
    check(std::fabs(crosshair.minY - centreY) < 2.0f,
          "the crosshair sits on the frame's middle row");

    auto arrow =
        boundsOf(map.last(arrowCorners + cornersPerLine).first(arrowCorners));

    // The arrow is a handful of map units across, which at the starting zoom is a
    // few pixels; twenty is loose enough to survive a zoom change and nowhere near
    // the ninety the mix-up cost.
    check(std::fabs((arrow.minX + arrow.maxX) * 0.5f - centreX) < 20.0f,
          "the arrow is centred horizontally on the frame");
    check(std::fabs((arrow.minY + arrow.maxY) * 0.5f - centreY) < 20.0f,
          "the arrow is centred vertically on the frame");
};

// With walls revealed, the map has to *span* the frame rather than sit on one
// point of it. The scale is the engine's own scale_mtof, so the emitted walls
// cover the same fraction of the frame AM_Drawer would rasterize into - and a
// transform whose two terms disagree by a factor of 65536 collapses them to a
// fraction of a pixel, which is what this measures.
//
// The walls alone, not the whole buffer: the arrow and the crosshair are drawn
// from the camera and the frame rather than from the map, so they keep their
// positions under exactly the mistake this is here to catch and would hold the
// bounding box open on their own.
auto tAutomapSpansTheFrame = test("Port/automapSpansTheFrame") = []
{
    openAutomapOnE1M1();

    // The port's own departure from vanilla: it walks the BSP while the map is up
    // so the walls in sight get their ML_MAPPED bit.
    Engine::revealAutomap();

    auto buffer = std::vector<Engine::AutomapVertex>(maxAutomapVertices);
    auto map = Engine::buildAutomap(Engine::camera(), buffer);

    constexpr auto cornersPerLine = 6u;
    constexpr auto tailCorners = (NUMPLYRLINES + 1) * cornersPerLine;

    check(map.size() > tailCorners, "walls were revealed and emitted");

    auto walls = boundsOf(map.first(map.size() - tailCorners));

    check(walls.width() > 20.0f, "the revealed walls span real frame pixels across");
    check(walls.height() > 20.0f, "the revealed walls span real frame pixels down");

    // And they stay the size of a room rather than a scale factor away from one.
    check(walls.width() < static_cast<float>(Doom::automapView().f_w) * 4.0f,
          "the revealed walls are not scaled up out of all proportion");
};
} // namespace
