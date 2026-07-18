#pragma once

#include "../d_player.h" // Player
#include "../r_defs.h" // Node, Seg, SubSector

// How fast light falls off with distance in the scale-light table. Anything
// reproducing DOOM's shading has to use the same number or the banding differs.
// Was r_main.h.
#define DISTMAP 2

// The column/span drawers the renderer switches between for detail mode and for
// the fuzz effect. Raw function pointers on purpose: these are called once per
// column and once per span, the hottest loop in the engine. Were r_main.h.
extern void (*colfunc)();
extern void (*basecolfunc)();
extern void (*fuzzcolfunc)();
extern void (*spanfunc)();

namespace Doom
{
// Renderer main/setup; r_main.cpp keeps the vanilla R_ names as shims.
void addPointToBox(int x, int y, fixed_t* box);
int pointOnSide(fixed_t x, fixed_t y, Node* node);
int pointOnSegSide(fixed_t x, fixed_t y, Seg* line);
angle_t pointToAngle(fixed_t x, fixed_t y);
angle_t pointToAngle2(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);
fixed_t pointToDist(fixed_t x, fixed_t y);
void initPointToAngle();
fixed_t scaleFromGlobalAngle(angle_t visangle);
void initTables();
void initTextureMapping();
void initLightTables();
void setViewSize(int blocks, int detail);
void executeSetViewSize();
void renderInit();
SubSector* pointInSubsector(fixed_t x, fixed_t y);
void setupFrame(Player& player);
void renderPlayerView(Player& player);
} // namespace Doom
