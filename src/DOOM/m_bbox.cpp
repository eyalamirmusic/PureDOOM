// The vanilla bounding-box API, now a shim over Doom::BBox (Math/BBox.h).
//
// Vanilla passes boxes around as a bare fixed_t[4] indexed by BOXTOP/BOXBOTTOM/
// BOXLEFT/BOXRIGHT, and half the engine still holds them that way - a sector's
// bbox, a linedef's, the blockmap's. BBox has the same four numbers in the same
// order, so the two are layout-compatible and this can reinterpret rather than
// copy. That is scaffolding, and it goes when the last fixed_t[4] does.

#include "Math/BBox.h"

#include "doom_config.h"
#include "m_bbox.h"

namespace
{
Doom::BBox& asBBox(fixed_t* box)
{
    return *(Doom::BBox*) box;
}

static_assert(sizeof(Doom::BBox) == 4 * sizeof(fixed_t),
              "BBox must be layout-compatible with vanilla's fixed_t[4]");
static_assert(offsetof(Doom::BBox, top) == BOXTOP * sizeof(fixed_t));
static_assert(offsetof(Doom::BBox, bottom) == BOXBOTTOM * sizeof(fixed_t));
static_assert(offsetof(Doom::BBox, left) == BOXLEFT * sizeof(fixed_t));
static_assert(offsetof(Doom::BBox, right) == BOXRIGHT * sizeof(fixed_t));
} // namespace

void M_ClearBox(fixed_t* box)
{
    asBBox(box) = Doom::BBox::empty();
}

void M_AddToBox(fixed_t* box, fixed_t x, fixed_t y)
{
    asBBox(box).add(Doom::Fixed {x}, Doom::Fixed {y});
}
