#pragma once

#include "Fixed.h"

namespace Doom
{
// A point or a vector in the map plane: two 16.16 fixed-point numbers. Every
// position, velocity and edge in the simulation is one of these, and the playsim
// is where they earn their keep - the renderer has its own.
//
// It is a plain aggregate over two Fixed, so it is trivially copyable and
// layout-compatible with the pair of `Fixed`s the vanilla structs still store
// (a mobj's x/y, a line's dx/dy). That is what lets a rewritten function take a
// Vec2 while the struct it came from is still 1993 C.
struct Vec2
{
    Fixed x;
    Fixed y;

    constexpr Vec2 operator+(Vec2 other) const { return {x + other.x, y + other.y}; }
    constexpr Vec2 operator-(Vec2 other) const { return {x - other.x, y - other.y}; }
    constexpr Vec2 operator-() const { return {-x, -y}; }

    constexpr bool operator==(const Vec2&) const = default;
};
} // namespace Doom
