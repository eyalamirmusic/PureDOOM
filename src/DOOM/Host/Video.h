#pragma once


namespace Doom
{
// The engine's video seam. In PureDOOM this is a thin host stub - the real
// drawing is the eacp app's, reading screens[0] and the palette back out - so
// most of these are empty. i_video.cpp keeps the vanilla I_ names as shims over
// these; screen_palette stays at file scope in Video.cpp for its many readers
// (DOOM.cpp, the menu, the status bar, the eacp port, the frame hash).
void initGraphics();
void shutdownGraphics();
void setPalette(byte* palette);
void updateNoBlit();
void finishUpdate();
void readScreen(byte* scr);
void startFrame();
void startTic();
void pollHostEvent();
} // namespace Doom
