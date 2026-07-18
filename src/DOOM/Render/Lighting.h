#pragma once

#include "../Sim/MapTypes.h"
#include "RenderTypes.h" // LightTable

// The light-selection constants: how many light levels the renderer resolves, and
// the scale/z table sizes it picks a COLORMAP row from. Were r_main.h.
#define LIGHTLEVELS 16
#define LIGHTSEGSHIFT 4
#define MAXLIGHTSCALE 48
#define LIGHTSCALESHIFT 12
#define MAXLIGHTZ 128
#define LIGHTZSHIFT 20
#define NUMCOLORMAPS 32

namespace Doom
{
// The renderer's light selection. Doom::initLightTables builds the diminishing-light
// lookups once - scalelight indexed by light level and on-screen scale, zlight by
// light level and distance, each cell a COLORMAP row (a LightTable* into the
// colormaps lump) - and R_SetupFrame sets the per-frame part: fixedcolormap, the one
// row a powerup locks the whole view to (the invulnerability sphere's inverse map, the
// light-amp visor's brightest row; 0 when no powerup is active), and extralight, the
// brightness bump a muzzle flash adds. scalelightfixed is the fixed-colormap row
// spread across the scale axis, so a locked view still indexes scalelight[]-shaped
// code. The wall and sprite drawers pick a row out of these each column.
//
// The fourth scalar cluster off the loose globals into the Engine (REFACTOR.md,
// Step 5). Storage moves off the r_main.cpp file-scope globals; the vanilla names are
// references onto these members, the three tables as references-to-array so their type
// and every indexed read (including the walllights = scalelight[light] row assignment)
// are unchanged. Nothing here is hashed, so gathering it is golden-neutral, as the
// earlier clusters were.
struct Lighting
{
    // The one row a powerup locks the view to, 0 when none is active.
    LightTable* fixedcolormap = nullptr;

    // The muzzle-flash brightness bump R_SetupFrame reads off the player.
    int extralight = 0;

    // The diminishing-light lookups Doom::initLightTables builds.
    LightTable* scalelight[LIGHTLEVELS][MAXLIGHTSCALE] = {};
    LightTable* scalelightfixed[MAXLIGHTSCALE] = {};
    LightTable* zlight[LIGHTLEVELS][MAXLIGHTZ] = {};
};

// The one Lighting, a view onto the Engine's member - the same pattern as
// viewWindow(), viewProjection(), viewPoint(), clip(), level(), wad() and randomness().
Lighting& lighting();
} // namespace Doom
