#pragma once

#include "../d_net.h" // doomcom_t, doomdata_t, BACKUPTICS, MAXNETNODES
#include "../d_ticcmd.h" // ticcmd_t
#include "../doomdef.h" // MAXPLAYERS
#include "../doomtype.h" // doom_boolean

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
    // The driver's shared comms block. RAII-owned by value now (Step 9) - what was a
    // boot-once doom_malloc in I_InitNetwork; the vanilla name `doomcom` stays a
    // doomcom_t* VIEW that I_InitNetwork points at doomcomStorage, so every doomcom->
    // reader and netbuffer (= &doomcom->data, a stable pointer into the member) is
    // unchanged. This mirrors reboundstore below, already a doomdata_t held by value.
    doomcom_t doomcomStorage = {}; // the owned comms block
    doomcom_t* doomcom = nullptr; // view onto doomcomStorage
    doomdata_t* netbuffer = nullptr; // points inside doomcom

    ticcmd_t localcmds[BACKUPTICS] = {}; // this node's commands
    ticcmd_t netcmds[MAXPLAYERS][BACKUPTICS] = {}; // every node's commands
    int nettics[MAXNETNODES] = {}; // tics received per node

    int maketic = 0; // the next tic to build
    int ticdup = 0; // display tics per game tic

    // per-player, per-tic desync checksum
    short consistancy[MAXPLAYERS][BACKUPTICS] = {};

    // Game/Net's own private bookkeeping (the resend/rebound machinery and the frame-rate
    // counters), folded in here as the file-scope-statics sweep reaches it - the same
    // netcode-bookkeeping owner. Read by no other file, and inert in single-player (the socket
    // code sits behind I_NET_ENABLED), so verified by build + app-link rather than a golden.
    doom_boolean nodeingame[MAXNETNODES] = {};   // node still in the game
    doom_boolean remoteresend[MAXNETNODES] = {}; // node needs local tics resent
    int resendto[MAXNETNODES] = {};              // next tic to send that node
    int resendcount[MAXNETNODES] = {};           // resend backoff counter
    int nodeforplayer[MAXPLAYERS] = {};          // node index per player
    int lastnettic = 0;                          // last tic processed
    int skiptics = 0;                            // tics to skip catching up
    int maxsend = 0;                             // BACKUPTICS/(2*ticdup)-1
    doom_boolean reboundpacket = false;          // a loopback packet is queued
    doomdata_t reboundstore = {};                // the loopback packet
    char exitmsg[80] = {};                       // netgame exit message scratch
    int gametime = 0;                            // I_GetTime at the last TryRunTics
    int oldentertics = 0;                        // entertic at the last TryRunTics (was a static)
    int frametics[4] = {};                       // per-frame tic counts (rate meter)
    int frameon = 0;                             // rate-meter frame counter
    int frameskip[4] = {};                       // per-frame skip flags (rate meter)
    int oldnettics = 0;                          // nettics at the last rate sample
};

// The one NetState, a view onto the Engine's member - the same pattern as
// overlayState(), refreshFlags(), demoState(), gameFlow(), playerState() and levelStats().
NetState& netState();
} // namespace Doom
