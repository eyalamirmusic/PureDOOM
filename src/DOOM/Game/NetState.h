#pragma once

#include "../d_net.h" // doomcom_t, doomdata_t, BACKUPTICS, MAXNETNODES
#include "../d_ticcmd.h" // ticcmd_t
#include "../doomdef.h" // MAXPLAYERS

namespace Doom
{
// The netcode's buffers and tic bookkeeping. doomcom is the driver's shared comms block and
// netbuffer points inside it; localcmds holds this node's commands, netcmds every node's, both
// indexed by tic modulo BACKUPTICS; nettics counts the tics received per node; maketic is the
// next tic to build and ticdup how many display tics one game tic spans. consistancy is the
// per-player, per-tic desync checksum (each tic's player x, or the random index in -devparm):
// G_BuildTiccmd stamps the current tic's value into the command it sends, and G_Ticker compares
// the value that comes back, calling I_Error on a mismatch. doomstat.h's netgame-buffers tail of
// "Internal parameters, used for engine", plus g_game's own consistancy array beside it.
//
// The first seven were a doomstat.h loose-global cluster, defined in Game/Net.cpp; consistancy is
// g_game's own file-scope state (read by no other file - the consistancy in d_ticcmd.h / Host/Net
// is the unrelated ticcmd_t member), folded in here as the file-scope-statics sweep reaches it -
// one netcode-bookkeeping owner (REFACTOR.md, Step 5). The vanilla names become references onto
// the members, the arrays as references-to-array so every indexed read is unchanged; the seven
// bind in Game/Net.cpp, consistancy in Game/Game.cpp, each at its definition site. PureDOOM ships
// single-player (the socket code sits behind I_NET_ENABLED), but netcmds[gametic] is how
// singletics feeds each built command to the ticker and consistancy is checked every tic, so
// these are on the demo path - golden-neutral through the reference all the same.
struct NetState
{
    doomcom_t* doomcom = nullptr; // the driver's shared comms block
    doomdata_t* netbuffer = nullptr; // points inside doomcom

    ticcmd_t localcmds[BACKUPTICS] = {}; // this node's commands
    ticcmd_t netcmds[MAXPLAYERS][BACKUPTICS] = {}; // every node's commands
    int nettics[MAXNETNODES] = {}; // tics received per node

    int maketic = 0; // the next tic to build
    int ticdup = 0; // display tics per game tic

    // per-player, per-tic desync checksum
    short consistancy[MAXPLAYERS][BACKUPTICS] = {};
};

// The one NetState, a view onto the Engine's member - the same pattern as
// overlayState(), refreshFlags(), demoState(), gameFlow(), playerState() and levelStats().
NetState& netState();
} // namespace Doom
