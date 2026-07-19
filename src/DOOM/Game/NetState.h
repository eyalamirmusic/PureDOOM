#pragma once

#include "NetTypes.h" // DoomCom, NetPacket, BACKUPTICS, MAXNETNODES
#include "Ticcmd.h" // Ticcmd
#include "GameDefs.h" // MAXPLAYERS
#include "Net.h"

#include <ea_data_structures/Structures/Array.h>

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

    EA::Array<Ticcmd, BACKUPTICS> localcmds = {}; // this node's commands
    EA::Array<EA::Array<Ticcmd, BACKUPTICS>, MAXPLAYERS> netcmds =
        {}; // every node's commands
    EA::Array<int, MAXNETNODES> nettics = {}; // tics received per node

    int maketic = 0; // the next tic to build
    int ticdup = 0; // display tics per game tic

    // per-player, per-tic desync checksum
    EA::Array<EA::Array<short, BACKUPTICS>, MAXPLAYERS> consistancy = {};

    // Game/Net's own private bookkeeping (the resend/rebound machinery and the frame-rate
    // counters), folded in here as the file-scope-statics sweep reaches it - the same
    // netcode-bookkeeping owner. Read by no other file, and inert in single-player (the socket
    // code sits behind I_NET_ENABLED), so verified by build + app-link rather than a golden.
    //
    // No lastnettic/frametics: both had zero reads *and* zero writes beyond their
    // own default member initializers, in this rewrite or in vanilla d_net.c.
    // Verified against the 1993 source in this repository's history; deleted
    // rather than carried, as no read was lost.
    EA::Array<bool, MAXNETNODES> nodeingame = {}; // node still in the game
    EA::Array<bool, MAXNETNODES> remoteresend = {}; // node needs local tics resent
    EA::Array<int, MAXNETNODES> resendto = {}; // next tic to send that node
    EA::Array<int, MAXNETNODES> resendcount = {}; // resend backoff counter
    EA::Array<int, MAXPLAYERS> nodeforplayer = {}; // node index per player
    int skiptics = 0; // tics to skip catching up
    int maxsend = 0; // BACKUPTICS/(2*ticdup)-1
    bool reboundpacket = false; // a loopback packet is queued
    NetPacket reboundstore = {}; // the loopback packet
    EA::Array<char, 80> exitmsg = {}; // netgame exit message scratch
    int gametime = 0; // currentTic at the last Doom::tryRunTics
    int oldentertics = 0; // entertic at the last Doom::tryRunTics (was a static)
    int frameon = 0; // rate-meter frame counter
    EA::Array<int, 4> frameskip = {}; // per-frame skip flags (rate meter)
    int oldnettics = 0; // nettics at the last rate sample
};

// The one NetState, a view onto the Engine's member - the same pattern as
// overlayState(), refreshFlags(), demoState(), gameFlow(), playerState() and levelStats().
NetState& netState();
} // namespace Doom
