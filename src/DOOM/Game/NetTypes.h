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
} // namespace Doom

namespace Doom
{
enum class NetCommandKind
{
    Send = 1,
    Get = 2
};
} // namespace Doom

//
// Network packet data.
//
namespace Doom
{
struct NetPacket
{
    // High bit is retransmit request.
    unsigned checksum;
    // Only valid if NCMD_RETRANSMIT.
    byte retransmitfrom;

    byte starttic;
    byte player;
    byte numtics;
    Ticcmd cmds[BACKUPTICS];
};
} // namespace Doom

namespace Doom
{
struct DoomCom
{
    // Supposed to be DOOMCOM_ID?
    long id;

    // DOOM executes an int to execute commands.
    short intnum;
    // Communication between DOOM and the driver.
    // Is NetCommandKind::Send or NetCommandKind::Get.
    short command;
    // Is dest for send, set by get (-1 = no packet).
    short remotenode;

    // Number of bytes in doomdata to be sent
    short datalength;

    // Info common to all nodes.
    // Console is allways node 0.
    short numnodes;
    // Flag: 1 = no duplication, 2-5 = dup for slow nets.
    short ticdup;
    // Flag: 1 = send a backup tic in every packet.
    short extratics;
    // Flag: 1 = deathmatch.
    short deathmatch;
    // Flag: -1 = new game, 0-5 = load savegame
    short savegame;
    short episode; // 1-3
    short map; // 1-9
    short skill; // 1-5

    // Info specific to this node.
    short consoleplayer;
    short numplayers;

    // These are related to the 3-display mode,
    //  in which two drones looking left and right
    //  were used to render two additional views
    //  on two additional computers.
    // Probably not operational anymore.
    // 1 = left, 0 = center, -1 = right
    short angleoffset;
    // 1 = drone
    short drone;

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
