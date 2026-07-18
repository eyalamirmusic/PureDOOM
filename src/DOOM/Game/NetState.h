#pragma once

#include "../d_net.h" // DoomCom, NetPacket, BACKUPTICS, MAXNETNODES
#include "../d_ticcmd.h" // Ticcmd
#include "../doomdef.h" // MAXPLAYERS
#include "../doomtype.h" // doom_boolean
#include "Net.h"

namespace Doom
{
// The netcode's buffers and tic bookkeeping. doomcom is the driver's shared comms block and
// netbuffer points inside it; localcmds holds this node's commands, netcmds every node's, both
// indexed by tic modulo BACKUPTICS; nettics counts the tics received per node; maketic is the
// next tic to build and ticdup how many display tics one game tic spans. consistancy is the
// per-player, per-tic desync checksum (each tic's player x, or the random index in -devparm):
// Doom::buildTiccmd stamps the current tic's value into the command it sends, and Doom::gameTicker compares
// the value that comes back, calling fatalError on a mismatch. doomstat.h's netgame-buffers tail of
// "Internal parameters, used for engine", plus g_game's own consistancy array beside it.
//
// The first seven were a doomstat.h loose-global cluster, defined in Game/Net.cpp; consistancy is
// g_game's own file-scope state (read by no other file - the consistancy in d_ticcmd.h / Host/Net
// is the unrelated Ticcmd member), folded in here as the file-scope-statics sweep reaches it -
// one netcode-bookkeeping owner (REFACTOR.md, Step 5). The vanilla names become references onto
// the members, the arrays as references-to-array so every indexed read is unchanged; the seven
// bind in Game/Net.cpp, consistancy in Game/Game.cpp, each at its definition site. PureDOOM ships
// single-player (the socket code sits behind I_NET_ENABLED), but netcmds[gametic] is how
// singletics feeds each built command to the ticker and consistancy is checked every tic, so
// these are on the demo path - golden-neutral through the reference all the same.
struct NetState
{
    // The driver's shared comms block. RAII-owned by value now (Step 9) - what was a
    // boot-once doom_malloc in initNetwork; the vanilla name `doomcom` stays a
    // DoomCom* VIEW that initNetwork points at doomcomStorage, so every doomcom->
    // reader and netbuffer (= &doomcom->data, a stable pointer into the member) is
    // unchanged. This mirrors reboundstore below, already a NetPacket held by value.
    DoomCom doomcomStorage = {}; // the owned comms block
    DoomCom* doomcom = nullptr; // view onto doomcomStorage
    NetPacket* netbuffer = nullptr; // points inside doomcom

    Ticcmd localcmds[BACKUPTICS] = {}; // this node's commands
    Ticcmd netcmds[MAXPLAYERS][BACKUPTICS] = {}; // every node's commands
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
    NetPacket reboundstore = {};                // the loopback packet
    char exitmsg[80] = {};                       // netgame exit message scratch
    int gametime = 0;                            // currentTic at the last Doom::tryRunTics
    int oldentertics = 0;                        // entertic at the last Doom::tryRunTics (was a static)
    int frametics[4] = {};                       // per-frame tic counts (rate meter)
    int frameon = 0;                             // rate-meter frame counter
    int frameskip[4] = {};                       // per-frame skip flags (rate meter)
    int oldnettics = 0;                          // nettics at the last rate sample
};

// The one NetState, a view onto the Engine's member - the same pattern as
// overlayState(), refreshFlags(), demoState(), gameFlow(), playerState() and levelStats().
NetState& netState();
} // namespace Doom
