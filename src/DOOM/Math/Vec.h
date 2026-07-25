#pragma once

#include "Fixed.h"

namespace Doom
{
// A point or a vector in the map plane: two 16.16 fixed-point numbers. Every
// position, velocity and edge in the simulation is one of these, and the playsim
// is where they earn their keep - the renderer has its own.
//
// It is a plain aggregate over two Fixed, so it is trivially copyable and
// layout-compatible with the pair of `Fixed`s the engine used to spell out at
// every site (a mobj's x/y, a line's dx/dy). That is what lets the savegame keep
// memcpy'ing a Mobj whole, and what lets a DegenMobj stay castable to one.
//
// Do not give it a constructor. Aggregate initialization is how every call site
// builds one (`{x, y}`, and braced into a Vec3), and a user-declared constructor
// would end it.
struct Vec2
{
    Fixed x;
    Fixed y;

    constexpr Vec2 operator+(Vec2 other) const { return {x + other.x, y + other.y}; }
    constexpr Vec2 operator-(Vec2 other) const { return {x - other.x, y - other.y}; }
    constexpr Vec2 operator-() const { return {-x, -y}; }

    constexpr Vec2& operator+=(Vec2 other) { return *this = *this + other; }
    constexpr Vec2& operator-=(Vec2 other) { return *this = *this - other; }

    // Raw shifts, scaling both components. The same meaning as Fixed's: they
    // halve and double the value, they do not convert it to or from whole units.
    constexpr Vec2 operator>>(int shift) const { return {x >> shift, y >> shift}; }
    constexpr Vec2 operator<<(int shift) const { return {x << shift, y << shift}; }

    constexpr bool operator==(const Vec2&) const = default;
};

// The same point with a height: a position or a momentum in the world rather
// than in the map plane. A mobj holds two of these.
//
// The playsim is mostly two-dimensional - clipping, the blockmap, the BSP and
// sight all work in the plane and z is carried along - so xy() is the common
// read, not an afterthought. Same aggregate rules as Vec2, for the same reasons.
struct Vec3
{
    Fixed x;
    Fixed y;
    Fixed z;

    constexpr Vec2 xy() const { return {x, y}; }

    constexpr void setXY(Vec2 value)
    {
        x = value.x;
        y = value.y;
    }

    constexpr Vec3 operator+(Vec3 other) const
    {
        return {x + other.x, y + other.y, z + other.z};
    }

    constexpr Vec3 operator-(Vec3 other) const
    {
        return {x - other.x, y - other.y, z - other.z};
    }

    constexpr Vec3 operator-() const { return {-x, -y, -z}; }

    constexpr Vec3 operator>>(int shift) const
    {
        return {x >> shift, y >> shift, z >> shift};
    }

    constexpr Vec3 operator<<(int shift) const
    {
        return {x << shift, y << shift, z << shift};
    }

    constexpr Vec3& operator+=(Vec3 other) { return *this = *this + other; }
    constexpr Vec3& operator-=(Vec3 other) { return *this = *this - other; }

    constexpr bool operator==(const Vec3&) const = default;
};

// A point in a screen, in whole pixels: where the UI draws. Nothing fixed-point
// about it - the status bar, the HUD and the intermission all position patches by
// integer column and row, in the 320x200 frame.
//
// Zero-initialized, unlike Vec2/Vec3, because the widgets it replaces were
// (`int x = 0`) and a widget is default-constructed before it is placed.
struct Vec2i
{
    int x = 0;
    int y = 0;

    constexpr Vec2i operator+(Vec2i other) const
    {
        return {x + other.x, y + other.y};
    }

    constexpr Vec2i operator-(Vec2i other) const
    {
        return {x - other.x, y - other.y};
    }

    constexpr Vec2i& operator+=(Vec2i other) { return *this = *this + other; }
    constexpr Vec2i& operator-=(Vec2i other) { return *this = *this - other; }

    constexpr bool operator==(const Vec2i&) const = default;
};
} // namespace Doom
