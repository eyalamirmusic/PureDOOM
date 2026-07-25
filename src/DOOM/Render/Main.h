#pragma once

#include "../Game/PlayerTypes.h" // Player
#include "../Sim/MapTypes.h"
#include "RenderTypes.h" // Node, Seg, SubSector

#include "Drawers.h" // the column/span drawer selection, was four externs here

namespace Doom
{
// How fast light falls off with distance in the scale-light table. Anything
// reproducing DOOM's shading has to use the same number or the banding differs.
// Was r_main.h.
constexpr int DISTMAP = 2;

// Renderer main/setup; r_main.cpp keeps the vanilla R_ names as shims.
int pointOnSide(Vec2 point, Node& node);
int pointOnSegSide(Vec2 point, Seg& line);
Angle pointToAngle(Vec2 point);
Angle pointToAngle2(Vec2 from, Vec2 to);
Fixed pointToDist(Vec2 point);
void initPointToAngle();
Fixed scaleFromGlobalAngle(Angle visangle);
void initTables();
void initTextureMapping();
void initLightTables();
void setViewSize(int blocks, int detail);
void executeSetViewSize();
void renderInit();
SubSector* pointInSubsector(Vec2 point);
void setupFrame(Player& player);
void renderPlayerView(Player& player);
} // namespace Doom
