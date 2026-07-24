// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//        Networking stuff.
//
//-----------------------------------------------------------------------------

#pragma once

#include "PlayerTypes.h"

//
// Network play related stuff.
// There is a data struct that stores network
// communication related stuff, and another
// one that defines the actual packets to
// be transmitted.
//

namespace Doom
{
// Sanity marker the driver and the engine agree on; DoomCom::id holds it.
constexpr long DOOMCOM_ID = 0x12345678l;

// Max computers/players in a game.
constexpr int MAXNETNODES = 8;

// Networking and tick handling related.
constexpr int BACKUPTICS = 12;

enum class NetCommandKind
{
    Send = 1,
    Get = 2
};

//
// Network packet data.
//
struct NetPacket
{
    // High bit is retransmit request.
    unsigned checksum = 0;
    // Only valid if NCMD_RETRANSMIT.
    byte retransmitfrom = 0;

    byte starttic = 0;
    byte player = 0;
    byte numtics = 0;
    Ticcmd cmds[BACKUPTICS];
};

struct DoomCom
{
    // Supposed to be DOOMCOM_ID?
    long id = 0;

    // DOOM executes an int to execute commands.
    short intnum = 0;
    // Communication between DOOM and the driver.
    // Is NetCommandKind::Send or NetCommandKind::Get.
    short command = 0;
    // Is dest for send, set by get (-1 = no packet).
    short remotenode = 0;

    // Number of bytes in doomdata to be sent
    short datalength = 0;

    // Info common to all nodes.
    // Console is allways node 0.
    short numnodes = 0;
    // Flag: 1 = no duplication, 2-5 = dup for slow nets.
    short ticdup = 0;
    // Flag: 1 = send a backup tic in every packet.
    short extratics = 0;
    // Flag: 1 = deathmatch.
    short deathmatch = 0;
    // Flag: -1 = new game, 0-5 = load savegame
    short savegame = 0;
    short episode = 0; // 1-3
    short map = 0; // 1-9
    short skill = 0; // 1-5

    // Info specific to this node.
    short consoleplayer = 0;
    short numplayers = 0;

    // These are related to the 3-display mode,
    //  in which two drones looking left and right
    //  were used to render two additional views
    //  on two additional computers.
    // Probably not operational anymore.
    // 1 = left, 0 = center, -1 = right
    short angleoffset = 0;
    // 1 = drone
    short drone = 0;

    // The packet data to be sent.
    NetPacket data;
};
} // namespace Doom

// Create any new ticcmds and broadcast to other players.

// Broadcasts special packets to other players
//  to notify of game exit

//? how many ticks to run?

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
