// The bridge between vanilla's bare Doom::Fixed[4] boxes and Doom::BBox.
//
// Was m_bbox.cpp's M_ClearBox / M_AddToBox. BBox holds the same four numbers in the
// same order as the vanilla array, which the static_asserts below pin, so this
// reinterprets rather than copies. The element type is spelled std::int32_t rather
// than Doom::Fixed so this layer needs none of the vanilla headers; they are one type.

#include "BBox.h"

#include <cstddef>

namespace Doom
{
static_assert(sizeof(BBox) == 4 * sizeof(std::int32_t),
              "BBox must be layout-compatible with vanilla's Fixed[4]");
static_assert(offsetof(BBox, top) == boxTop * sizeof(std::int32_t));
static_assert(offsetof(BBox, bottom) == boxBottom * sizeof(std::int32_t));
static_assert(offsetof(BBox, left) == boxLeft * sizeof(std::int32_t));
static_assert(offsetof(BBox, right) == boxRight * sizeof(std::int32_t));

BBox& asBBox(Fixed* box)
{
    return *(BBox*) box;
}

void clearBox(Fixed* box)
{
    asBBox(box) = BBox::empty();
}

void addToBox(Fixed* box, Fixed x, Fixed y)
{
    asBBox(box).add(x, y);
}
} // namespace Doom
