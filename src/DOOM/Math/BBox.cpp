// The bridge between vanilla's bare fixed_t[4] boxes and Doom::BBox.
//
// Was m_bbox.cpp's M_ClearBox / M_AddToBox. BBox holds the same four numbers in the
// same order as the vanilla array, which the static_asserts below pin, so this
// reinterprets rather than copies. The element type is spelled std::int32_t rather
// than fixed_t so this layer needs none of the vanilla headers; they are one type.

#include "BBox.h"

#include <cstddef>

namespace Doom
{
static_assert(sizeof(BBox) == 4 * sizeof(std::int32_t),
              "BBox must be layout-compatible with vanilla's fixed_t[4]");
static_assert(offsetof(BBox, top) == BOXTOP * sizeof(std::int32_t));
static_assert(offsetof(BBox, bottom) == BOXBOTTOM * sizeof(std::int32_t));
static_assert(offsetof(BBox, left) == BOXLEFT * sizeof(std::int32_t));
static_assert(offsetof(BBox, right) == BOXRIGHT * sizeof(std::int32_t));

BBox& asBBox(std::int32_t* box) { return *(BBox*) box; }

void clearBox(std::int32_t* box) { asBBox(box) = BBox::empty(); }

void addToBox(std::int32_t* box, std::int32_t x, std::int32_t y)
{
    asBBox(box).add(Fixed {x}, Fixed {y});
}
} // namespace Doom
