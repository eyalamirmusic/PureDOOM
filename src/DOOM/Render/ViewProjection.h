#pragma once

#include "../Game/GameDefs.h" // SCREENWIDTH
#include "../Math/FixedPoint.h" // Doom::Fixed
#include "../Math/TrigTables.h" // Doom::Angle, fineAngles

#include "../Containers.h"

namespace Doom
{
// How the 3D view projects onto the screen. executeSetViewSize computes it once
// whenever the view size changes (the menu's screen-size slider, or startup) and the
// renderer reads it back on every seg and sprite: the screen centre in pixels and in
// fixed point, the projection scale, and the angle<->column tables initTextureMapping
// builds (viewangletox maps a view angle to its screen column, xtoviewangle the
// reverse, and clipangle is the field-of-view edge, xtoviewangle[0]).
//
// The second scalar cluster off the loose globals into the Engine (REFACTOR.md,
// Step 5), the sibling of ViewPoint: ViewPoint is where the camera *is* (per frame),
// ViewProjection is how its view lands on the screen (per view-size change). The
// vanilla names are references onto these members while r_main and the Render/ units
// still read them as globals; the two arrays are references-to-array, so the type is
// unchanged and every indexed read resolves exactly as before. Nothing here is hashed
// (the frame goldens see the picture, not these numbers), so gathering it is
// golden-neutral, as Random / Level / Clip / ViewPoint were.
struct ViewProjection
{
    int centerx = 0;
    int centery = 0;

    Fixed centerxfrac {};
    Fixed centeryfrac {};
    Fixed projection {};

    // The field-of-view edge angle (xtoviewangle[0]), clipped against per seg.
    Angle clipangle {};

    // initTextureMapping's angle<->column maps.
    Array<int, fineAngles / 2> viewangletox = {};
    Array<Angle, SCREENWIDTH + 1> xtoviewangle = {};
};

// The one ViewProjection, a view onto the Engine's member - the same pattern as
// viewPoint(), clipping(), level(), wad() and randomness().
ViewProjection& viewProjection();
} // namespace Doom
