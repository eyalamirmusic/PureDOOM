#pragma once

namespace Doom
{
// The -timedemo benchmark state. Doom::startTimeDemo raises timingdemo and plays a demo flat out; starttime
// is the wall clock (currentTic) captured at level start in G_DoLoadLevel, and when the demo ends
// Doom::checkDemoStatus - if timingdemo - reports the frame rate from the elapsed tics (endtime minus
// starttime) and exits. starttime is written on every level load but read only on that path.
// g_game's own file-scope state, read by no other file.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); the vanilla names
// become references onto these members. -timedemo is never passed in the headless suite, so
// timingdemo stays false and starttime is written-but-unread - golden-neutral.
struct TimeDemo
{
    bool timingdemo = false; // report fps and exit when the demo ends
    int starttime = 0; // currentTic at level start, the report's reference point
};

// The one TimeDemo, a view onto the Engine's member - the same pattern as the other Game/
// clusters (demoState(), gameClock(), ...).
TimeDemo& timeDemo();
} // namespace Doom
