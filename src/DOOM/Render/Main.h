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
void addPointToBox(int x, int y, Fixed* box);
int pointOnSide(Fixed x, Fixed y, Node& node);
int pointOnSegSide(Fixed x, Fixed y, Seg& line);
Angle pointToAngle(Fixed x, Fixed y);
Angle pointToAngle2(Fixed x1, Fixed y1, Fixed x2, Fixed y2);
Fixed pointToDist(Fixed x, Fixed y);
void initPointToAngle();
Fixed scaleFromGlobalAngle(Angle visangle);
void initTables();
void initTextureMapping();
void initLightTables();
void setViewSize(int blocks, int detail);
void executeSetViewSize();
void renderInit();
SubSector* pointInSubsector(Fixed x, Fixed y);
void setupFrame(Player& player);
void renderPlayerView(Player& player);
} // namespace Doom
