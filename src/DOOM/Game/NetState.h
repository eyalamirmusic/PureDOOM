#pragma once

#include "../d_net.h" // doomcom_t, doomdata_t, BACKUPTICS, MAXNETNODES
#include "../d_ticcmd.h" // ticcmd_t
#include "../doomdef.h" // MAXPLAYERS

namespace Doom
{
// The netcode's buffers and tic bookkeeping. doomcom is the driver's shared comms block and
// netbuffer points inside it; localcmds holds this node's commands, netcmds every node's, both
// indexed by tic modulo BACKUPTICS; nettics counts the tics received per node; maketic is the
// next tic to build and ticdup how many display tics one game tic spans. doomstat.h's
// netgame-buffers tail of "Internal parameters, used for engine".
//
// A cluster of doomstat.h's game state moved off the loose globals into the Engine
// (REFACTOR.md, Step 5). All seven were externed only in doomstat.h and defined in
// Game/Net.cpp above its namespace (a state owner); the vanilla names become references onto
// the members, the arrays as references-to-array so every indexed read is unchanged. PureDOOM
// ships single-player (the socket code sits behind I_NET_ENABLED), but netcmds[gametic] is
// how singletics feeds each built command to the ticker, so these are on the demo path -
// golden-neutral through the reference all the same.
struct NetState
{
    doomcom_t* doomcom = nullptr; // the driver's shared comms block
    doomdata_t* netbuffer = nullptr; // points inside doomcom

    ticcmd_t localcmds[BACKUPTICS] = {}; // this node's commands
    ticcmd_t netcmds[MAXPLAYERS][BACKUPTICS] = {}; // every node's commands
    int nettics[MAXNETNODES] = {}; // tics received per node

    int maketic = 0; // the next tic to build
    int ticdup = 0; // display tics per game tic
};

// The one NetState, a view onto the Engine's member - the same pattern as
// overlayState(), refreshFlags(), demoState(), gameFlow(), playerState() and levelStats().
NetState& netState();
} // namespace Doom
