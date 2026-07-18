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
// $Log:$
//
// DESCRIPTION:
//        DOOM Network game communication and protocol,
//        all OS independend parts.
//
//-----------------------------------------------------------------------------

// Rewritten out of vanilla d_net into namespace Doom.
//
// The netcode and the tic run loop. d_net.cpp shims netUpdate / tryRunTics /
// checkNetGame / quitNetGame. The net buffers and tic counters (netcmds,
// maketic, ticdup, ... read by g_game and the loop) stay at file scope here,
// above the namespace. PureDOOM runs singletics; netUpdate's no-command-on-
// singletics behaviour is load-bearing (CLAUDE.md) and preserved verbatim. The
// demos drive the tic loop, so the goldens pin it.

#include "../Host/Platform.h"

#include "../UI/Menu.h"
#include "GameDefs.h"
#include "MapSpawns.h"

#include "AttractMode.h"
#include "DemoState.h"
#include "EngineParams.h"
#include "EventQueue.h"
#include "GameClock.h"
#include "GameSession.h"
#include "LaunchOptions.h"
#include "NetState.h"
#include "PlayerState.h"
#include "StartupDefaults.h"

#include "Net.h"

//
#include "DoomMain.h"
// NETWORKING
#include "../Host/Video.h"
//
#include "../UI/Menu.h"
// gametic is the tic about to (or currently being) run
#include "../Host/Net.h"
// maketic is the tick that hasn't had control made for it yet
#include "../Host/System.h"
// nettics[] has the maketics for all players
#include "Game.h"
//
// a gametic cannot be run until nettics[] > gametic for all players
//

#define NCMD_EXIT 0x80000000
#define NCMD_RETRANSMIT 0x40000000
#define NCMD_SETUP 0x20000000
#define NCMD_KILL 0x10000000 // kill game
#define NCMD_CHECKSUM 0x0fffffff

#define RESENDCOUNT 10
#define PL_DRONE 0x80 // bit flag in doomdata->player

// The netcode buffers and tic bookkeeping are a Doom::NetState owned by the Engine now, and
// this file reads them straight off netState(). These five are still externed in doomstat.h
// and read through those externs by Game/Game.cpp, Game/DoomMain.cpp and Host/Net.cpp, so the
// references stay until their readers go through netState() too (REFACTOR.md, Step 5).




void Doom::processEvents();
void Doom::buildTiccmd(Doom::Ticcmd* cmd);
void Doom::doAdvanceDemo();

namespace Doom
{

int netbufferSize()
{
    return (int) (long long) &(
        ((NetPacket*) 0)->cmds[netState().netbuffer->numtics]);
}

//
// Checksum
//
unsigned netbufferChecksum()
{
    unsigned c;
    int i, l;

    c = 0x1234567;

    // FIXME -endianess?
    // #ifdef NORMALUNIX
    return 0; // byte order problems
    // #endif

    l = (netbufferSize() - (int) (long long) &(((NetPacket*) 0)->retransmitfrom))
        / 4;
    for (i = 0; i < l; i++)
        c += (reinterpret_cast<unsigned*>(&netState().netbuffer->retransmitfrom))[i]
             * (i + 1);

    return c & NCMD_CHECKSUM;
}

//
//
//
int expandTics(int low)
{
    int delta;

    auto& net = netState();

    delta = low - (net.maketic & 0xff);

    if (delta >= -64 && delta <= 64)
        return (net.maketic & ~0xff) + low;
    if (delta > 64)
        return (net.maketic & ~0xff) - 256 + low;
    if (delta < -64)
        return (net.maketic & ~0xff) + 256 + low;

    //fatalError("Error: expandTics: strange value %i at maketic %i", low, maketic);
    doom_strcpy(error_buf, "Error: expandTics: strange value ");
    doom_concat(error_buf, doom_itoa(low, 10));
    doom_concat(error_buf, " at maketic ");
    doom_concat(error_buf, doom_itoa(net.maketic, 10));
    fatalError(error_buf);
    return 0;
}

//
// hSendPacket
//
void hSendPacket(int node, int flags)
{
    auto& net = netState();
    auto* debugfile = engineParams().debugfile;

    net.netbuffer->checksum = netbufferChecksum() | flags;

    if (!node)
    {
        net.reboundstore = *net.netbuffer;
        net.reboundpacket = true;
        return;
    }

    if (demoState().demoplayback)
        return;

    if (!gameSession().netgame)
        fatalError("Error: Tried to transmit to another node");

    net.doomcom->command = CMD_SEND;
    net.doomcom->remotenode = node;
    net.doomcom->datalength = netbufferSize();

    if (debugfile)
    {
        int realretrans;
        if (net.netbuffer->checksum & NCMD_RETRANSMIT)
            realretrans = expandTics(net.netbuffer->retransmitfrom);
        else
            realretrans = -1;

        {
            //fprintf(debugfile, "send (%i + %i, R %i) [%i] ",
            //        expandTics(netbuffer->starttic),
            //        netbuffer->numtics, realretrans, doomcom->datalength);
            doom_fprint(debugfile, "send (");
            doom_fprint(debugfile,
                        doom_itoa(expandTics(net.netbuffer->starttic), 10));
            doom_fprint(debugfile, " + ");
            doom_fprint(debugfile, doom_itoa(net.netbuffer->numtics, 10));
            doom_fprint(debugfile, ", R ");
            doom_fprint(debugfile, doom_itoa(realretrans, 10));
            doom_fprint(debugfile, ") [");
            doom_fprint(debugfile, doom_itoa(net.doomcom->datalength, 10));
            doom_fprint(debugfile, "] ");
        }

        for (int i = 0; i < net.doomcom->datalength; i++)
        {
            //fprintf(debugfile, "%i ", ((byte*)netbuffer)[i]);
            doom_fprint(debugfile,
                        doom_itoa((reinterpret_cast<byte*>(net.netbuffer))[i], 10));
            doom_fprint(debugfile, " ");
        }

        doom_fprint(debugfile, "\n");
    }

    netCommand();
}

//
// hGetPacket
// Returns false if no packet is waiting
//
doom_boolean hGetPacket()
{
    auto& net = netState();
    auto* debugfile = engineParams().debugfile;

    if (net.reboundpacket)
    {
        *net.netbuffer = net.reboundstore;
        net.doomcom->remotenode = 0;
        net.reboundpacket = false;
        return true;
    }

    if (!gameSession().netgame)
        return false;

    if (demoState().demoplayback)
        return false;

    net.doomcom->command = CMD_GET;
    netCommand();

    if (net.doomcom->remotenode == -1)
        return false;

    if (net.doomcom->datalength != netbufferSize())
    {
        if (debugfile)
        {
            //fprintf(debugfile, "bad packet length %i\n", doomcom->datalength);
            doom_fprint(debugfile, "bad packet length ");
            doom_fprint(debugfile, doom_itoa(net.doomcom->datalength, 10));
            doom_fprint(debugfile, "\n");
        }
        return false;
    }

    if (netbufferChecksum() != (net.netbuffer->checksum & NCMD_CHECKSUM))
    {
        if (debugfile)
        {
            doom_fprint(debugfile, "bad packet checksum\n");
        }
        return false;
    }

    if (debugfile)
    {
        int realretrans;

        if (net.netbuffer->checksum & NCMD_SETUP)
        {
            doom_fprint(debugfile, "setup packet\n");
        }
        else
        {
            if (net.netbuffer->checksum & NCMD_RETRANSMIT)
                realretrans = expandTics(net.netbuffer->retransmitfrom);
            else
                realretrans = -1;

            {
                //fprintf(debugfile, "get %i = (%i + %i, R %i)[%i] ",
                //        doomcom->remotenode,
                //        expandTics(netbuffer->starttic),
                //        netbuffer->numtics, realretrans, doomcom->datalength);
                doom_fprint(debugfile, "get ");
                doom_fprint(debugfile, doom_itoa(net.doomcom->remotenode, 10));
                doom_fprint(debugfile, " = (");
                doom_fprint(debugfile,
                            doom_itoa(expandTics(net.netbuffer->starttic), 10));
                doom_fprint(debugfile, " + ");
                doom_fprint(debugfile, doom_itoa(net.netbuffer->numtics, 10));
                doom_fprint(debugfile, ", R ");
                doom_fprint(debugfile, doom_itoa(realretrans, 10));
                doom_fprint(debugfile, ")[");
                doom_fprint(debugfile, doom_itoa(net.doomcom->datalength, 10));
                doom_fprint(debugfile, "] ");
            }

            for (int i = 0; i < net.doomcom->datalength; i++)
            {
                //fprintf(debugfile, "%i ", ((byte*)netbuffer)[i]);
                doom_fprint(
                    debugfile,
                    doom_itoa((reinterpret_cast<byte*>(net.netbuffer))[i], 10));
                doom_fprint(debugfile, " ");
            }
            doom_fprint(debugfile, "\n");
        }
    }
    return true;
}

//
// getPackets
//
void getPackets()
{
    int netconsole;
    int netnode;
    Ticcmd *src, *dest;
    int realend;
    int realstart;

    auto& net = netState();
    auto& state = playerState();
    auto* debugfile = engineParams().debugfile;

    while (hGetPacket())
    {
        if (net.netbuffer->checksum & NCMD_SETUP)
            continue; // extra setup packet

        netconsole = net.netbuffer->player & ~PL_DRONE;
        netnode = net.doomcom->remotenode;

        // to save bytes, only the low byte of tic numbers are sent
        // Figure out what the rest of the bytes are
        realstart = expandTics(net.netbuffer->starttic);
        realend = (realstart + net.netbuffer->numtics);

        // check for exiting the game
        if (net.netbuffer->checksum & NCMD_EXIT)
        {
            if (!net.nodeingame[netnode])
                continue;
            net.nodeingame[netnode] = false;
            state.playeringame[netconsole] = false;
            doom_strcpy(net.exitmsg, "Player 1 left the game");
            net.exitmsg[7] += netconsole;
            state.players[state.consoleplayer].message = net.exitmsg;
            if (demoState().demorecording)
                Doom::checkDemoStatus();
            continue;
        }

        // check for a remote game kill
        if (net.netbuffer->checksum & NCMD_KILL)
            fatalError("Error: Killed by network driver");

        net.nodeforplayer[netconsole] = netnode;

        // check for retransmit request
        if (net.resendcount[netnode] <= 0
            && (net.netbuffer->checksum & NCMD_RETRANSMIT))
        {
            net.resendto[netnode] = expandTics(net.netbuffer->retransmitfrom);
            if (debugfile)
            {
                //fprintf(debugfile, "retransmit from %i\n", resendto[netnode]);
                doom_fprint(debugfile, "retransmit from ");
                doom_fprint(debugfile, doom_itoa(net.resendto[netnode], 10));
                doom_fprint(debugfile, "\n");
            }
            net.resendcount[netnode] = RESENDCOUNT;
        }
        else
            net.resendcount[netnode]--;

        // check for out of order / duplicated packet
        if (realend == net.nettics[netnode])
            continue;

        if (realend < net.nettics[netnode])
        {
            if (debugfile)
            {
                //fprintf(debugfile,
                //        "out of order packet (%i + %i)\n",
                //        realstart, netbuffer->numtics);
                doom_fprint(debugfile, "out of order packet (");
                doom_fprint(debugfile, doom_itoa(realstart, 10));
                doom_fprint(debugfile, " + ");
                doom_fprint(debugfile, doom_itoa(net.netbuffer->numtics, 10));
                doom_fprint(debugfile, ")\n");
            }
            continue;
        }

        // check for a missed packet
        if (realstart > net.nettics[netnode])
        {
            // stop processing until the other system resends the missed tics
            if (debugfile)
            {
                //fprintf(debugfile,
                //        "missed tics from %i (%i - %i)\n",
                //        netnode, realstart, nettics[netnode]);
                doom_fprint(debugfile, "missed tics from ");
                doom_fprint(debugfile, doom_itoa(netnode, 10));
                doom_fprint(debugfile, " (");
                doom_fprint(debugfile, doom_itoa(realstart, 10));
                doom_fprint(debugfile, " - ");
                doom_fprint(debugfile, doom_itoa(net.nettics[netnode], 10));
                doom_fprint(debugfile, ")\n");
            }
            net.remoteresend[netnode] = true;
            continue;
        }

        // update command store from the packet
        {
            int start;

            net.remoteresend[netnode] = false;

            start = net.nettics[netnode] - realstart;
            src = &net.netbuffer->cmds[start];

            while (net.nettics[netnode] < realend)
            {
                dest = &net.netcmds[netconsole][net.nettics[netnode] % BACKUPTICS];
                net.nettics[netnode]++;
                *dest = *src;
                src++;
            }
        }
    }
}

//
// netUpdate
// Builds ticcmds for console player,
// sends out a packet
//
void netUpdate()
{
    int nowtime;
    int newtics;
    int realstart;
    int gameticdiv;

    auto& net = netState();
    const doom_boolean singletics = engineParams().singletics;

    // check time
    nowtime = currentTic() / net.ticdup;
    newtics = nowtime - net.gametime;
    net.gametime = nowtime;

    if (newtics <= 0) // nothing new to update
        goto listen;

    if (net.skiptics <= newtics)
    {
        newtics -= net.skiptics;
        net.skiptics = 0;
    }
    else
    {
        net.skiptics -= newtics;
        newtics = 0;
    }

    net.netbuffer->player = playerState().consoleplayer;

    // build new ticcmds for console player
    gameticdiv = gameClock().gametic / net.ticdup;
    for (int i = 0; i < newtics; i++)
    {
        startTic();
        Doom::processEvents();

        // [pd] A singletic update is synchronous: Doom::doomLoop builds the tic's
        // command and runs it in the same breath, advancing maketic and gametic
        // together. Building another one here would consume the input that
        // command is about to read, and would advance maketic with no gametic to
        // match it -- and this runs from Doom::displayFrame and R_RenderPlayerView too,
        // which vanilla called to keep the netcode fed while a slow frame
        // rendered. maketic therefore climbed until it jammed against the cap
        // below and stayed there, and since Doom::doomLoop writes the command to
        // netcmds[maketic] while Doom::gameTicker reads netcmds[gametic], every command
        // was executed five tics (143ms) after it was built. Events are still
        // drained above; there is simply no second command to build.
        if (singletics)
            continue;

        if (net.maketic - gameticdiv >= BACKUPTICS / 2 - 1)
            break; // can't hold any more

        //doom_print ("mk:%i ",maketic);
        Doom::buildTiccmd(&net.localcmds[net.maketic % BACKUPTICS]);
        net.maketic++;
    }

    if (singletics)
        return; // singletic update is syncronous

    // send the packet to the other nodes
    for (int i = 0; i < net.doomcom->numnodes; i++)
        if (net.nodeingame[i])
        {
            net.netbuffer->starttic = realstart = net.resendto[i];
            net.netbuffer->numtics = net.maketic - realstart;
            if (net.netbuffer->numtics > BACKUPTICS)
                fatalError("Error: netUpdate: netbuffer->numtics > BACKUPTICS");

            net.resendto[i] = net.maketic - net.doomcom->extratics;

            for (int j = 0; j < net.netbuffer->numtics; j++)
                net.netbuffer->cmds[j] = net.localcmds[(realstart + j) % BACKUPTICS];

            if (net.remoteresend[i])
            {
                net.netbuffer->retransmitfrom = net.nettics[i];
                hSendPacket(i, NCMD_RETRANSMIT);
            }
            else
            {
                net.netbuffer->retransmitfrom = 0;
                hSendPacket(i, 0);
            }
        }

    // listen for other packets
listen:
    getPackets();
}

//
// checkAbort
//
void checkAbort()
{
    Event* ev;
    int stoptic;

    stoptic = currentTic() + 2;
    while (currentTic() < stoptic)
        startTic();

    auto& events_ = eventQueue();

    startTic();
    for (; events_.eventtail != events_.eventhead;)
    {
        ev = &events_.events[events_.eventtail];
        if (ev->type == ev_keydown && ev->data1 == KEY_ESCAPE)
            fatalError("Error: Network game synchronization aborted.");
    }

    events_.eventtail++;
    events_.eventtail = (events_.eventtail) & (MAXEVENTS - 1);
}

//
// dArbitrateNetStart
//
void dArbitrateNetStart()
{
    int i;
    doom_boolean gotinfo[MAXNETNODES];

    auto& net = netState();
    auto& defaults_ = startupDefaults();
    auto& opts = launchOptions();

    defaults_.autostart = true;
    doom_memset(gotinfo, 0, sizeof(gotinfo));

    if (net.doomcom->consoleplayer)
    {
        // listen for setup info from key player
        doom_print("listening for network start info...\n");
        while (1)
        {
            checkAbort();
            if (!hGetPacket())
                continue;
            if (net.netbuffer->checksum & NCMD_SETUP)
            {
                if (net.netbuffer->player != VERSION)
                    fatalError(
                        "Error: Different DOOM versions cannot play a net game!");
                defaults_.startskill =
                    static_cast<Skill>((net.netbuffer->retransmitfrom & 15));
                gameSession().deathmatch =
                    (net.netbuffer->retransmitfrom & 0xc0) >> 6;
                opts.nomonsters = (net.netbuffer->retransmitfrom & 0x20) > 0;
                opts.respawnparm = (net.netbuffer->retransmitfrom & 0x10) > 0;
                defaults_.startmap = net.netbuffer->starttic & 0x3f;
                defaults_.startepisode = net.netbuffer->starttic >> 6;
                return;
            }
        }
    }
    else
    {
        // key player, send the setup info
        doom_print("sending network start info...\n");
        do
        {
            checkAbort();
            for (i = 0; i < net.doomcom->numnodes; i++)
            {
                const int deathmatch = gameSession().deathmatch;

                net.netbuffer->retransmitfrom = defaults_.startskill;
                if (deathmatch)
                    net.netbuffer->retransmitfrom |= (deathmatch << 6);
                if (opts.nomonsters)
                    net.netbuffer->retransmitfrom |= 0x20;
                if (opts.respawnparm)
                    net.netbuffer->retransmitfrom |= 0x10;
                net.netbuffer->starttic =
                    defaults_.startepisode * 64 + defaults_.startmap;
                net.netbuffer->player = VERSION;
                net.netbuffer->numtics = 0;
                hSendPacket(i, NCMD_SETUP);
            }

#if 1
            for (i = 10; i && hGetPacket(); --i)
            {
                if ((net.netbuffer->player & 0x7f) < MAXNETNODES)
                    gotinfo[net.netbuffer->player & 0x7f] = true;
            }
#else
            while (hGetPacket())
            {
                gotinfo[net.netbuffer->player & 0x7f] = true;
            }
#endif

            for (i = 1; i < net.doomcom->numnodes; i++)
                if (!gotinfo[i])
                    break;
        } while (i < net.doomcom->numnodes);
    }
}

//
// checkNetGame
// Works out player numbers among the net participants
//
void checkNetGame()
{
    auto& net = netState();
    auto& state = playerState();
    auto& session = gameSession();
    auto& defaults_ = startupDefaults();

    for (int i = 0; i < MAXNETNODES; i++)
    {
        net.nodeingame[i] = false;
        net.nettics[i] = 0;
        net.remoteresend[i] = false; // set when local needs tics
        net.resendto[i] = 0; // which tic to start sending
    }

    // initNetwork sets doomcom and netgame
    initNetwork();
    if (net.doomcom->id != DOOMCOM_ID)
        fatalError("Error: Doomcom buffer invalid!");

    net.netbuffer = &net.doomcom->data;
    state.consoleplayer = state.displayplayer = net.doomcom->consoleplayer;
    if (session.netgame)
        dArbitrateNetStart();

    //doom_print("startskill %i  deathmatch: %i  startmap: %i  startepisode: %i\n",
    //       startskill, deathmatch, startmap, startepisode);
    doom_print("startskill ");
    doom_print(doom_itoa(defaults_.startskill, 10));
    doom_print("  deathmatch: ");
    doom_print(doom_itoa(session.deathmatch, 10));
    doom_print("  startmap: ");
    doom_print(doom_itoa(defaults_.startmap, 10));
    doom_print("  startepisode: ");
    doom_print(doom_itoa(defaults_.startepisode, 10));
    doom_print("\n");

    // read values out of doomcom
    net.ticdup = net.doomcom->ticdup;
    net.maxsend = BACKUPTICS / (2 * net.ticdup) - 1;
    if (net.maxsend < 1)
        net.maxsend = 1;

    for (int i = 0; i < net.doomcom->numplayers; i++)
        state.playeringame[i] = true;
    for (int i = 0; i < net.doomcom->numnodes; i++)
        net.nodeingame[i] = true;

    //doom_print("player %i of %i (%i nodes)\n",
    //       consoleplayer + 1, doomcom->numplayers, doomcom->numnodes);
    doom_print("player ");
    doom_print(doom_itoa(state.consoleplayer + 1, 10));
    doom_print(" of ");
    doom_print(doom_itoa(net.doomcom->numplayers, 10));
    doom_print(" (");
    doom_print(doom_itoa(net.doomcom->numnodes, 10));
    doom_print(" nodes)\n");
}

//
// quitNetGame
// Called before quitting to leave a net game
// without hanging the other players
//
void quitNetGame()
{
    auto& net = netState();
    auto& demo = demoState();
    auto* debugfile = engineParams().debugfile;
    const int consoleplayer = playerState().consoleplayer;

    if (debugfile)
        doom_close(debugfile);

    if (!gameSession().netgame || !demo.usergame || consoleplayer == -1
        || demo.demoplayback)
        return;

    // send a bunch of packets for security
    net.netbuffer->player = consoleplayer;
    net.netbuffer->numtics = 0;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 1; j < net.doomcom->numnodes; j++)
            if (net.nodeingame[j])
                hSendPacket(j, NCMD_EXIT);
        waitVBlank(1);
    }
}

//
// tryRunTics
//
void tryRunTics()
{
    int i;
    int lowtic;
    int entertic;
    auto& net = netState();
    auto& state = playerState();
    auto& clock = gameClock();
    auto* debugfile = engineParams().debugfile;
    int& oldentertics = net.oldentertics;
    int realtics;
    int availabletics;
    int counts;

    // get real tics
    entertic = currentTic() / net.ticdup;
    realtics = entertic - oldentertics;
    oldentertics = entertic;

    // get available tics
    netUpdate();

    lowtic = DOOM_MAXINT;
    for (i = 0; i < net.doomcom->numnodes; i++)
    {
        if (net.nodeingame[i])
        {
            if (net.nettics[i] < lowtic)
                lowtic = net.nettics[i];
        }
    }
    availabletics = lowtic - clock.gametic / net.ticdup;

    // decide how many tics to run
    if (realtics < availabletics - 1)
        counts = realtics + 1;
    else if (realtics < availabletics)
        counts = realtics;
    else
        counts = availabletics;

    if (counts < 1)
        counts = 1;

    net.frameon++;

    if (debugfile)
    {
        //fprintf(debugfile,
        //        "=======real: %i  avail: %i  game: %i\n",
        //        realtics, availabletics, counts);
        doom_fprint(debugfile, "=======real: ");
        doom_fprint(debugfile, doom_itoa(realtics, 10));
        doom_fprint(debugfile, "  avail: ");
        doom_fprint(debugfile, doom_itoa(availabletics, 10));
        doom_fprint(debugfile, "  game: ");
        doom_fprint(debugfile, doom_itoa(counts, 10));
        doom_fprint(debugfile, "\n");
    }

    if (!demoState().demoplayback)
    {
        // ideally nettics[0] should be 1 - 3 tics above lowtic
        // if we are consistantly slower, speed up time
        for (i = 0; i < MAXPLAYERS; i++)
            if (state.playeringame[i])
                break;
        if (state.consoleplayer == i)
        {
            // the key player does not adapt
        }
        else
        {
            if (net.nettics[0] <= net.nettics[net.nodeforplayer[i]])
            {
                net.gametime--;
                // doom_print ("-");
            }
            net.frameskip[net.frameon & 3] =
                (net.oldnettics > net.nettics[net.nodeforplayer[i]]);
            net.oldnettics = net.nettics[0];
            if (net.frameskip[0] && net.frameskip[1] && net.frameskip[2]
                && net.frameskip[3])
            {
                net.skiptics = 1;
                // doom_print ("+");
            }
        }
    } // demoplayback

    // wait for new tics if needed
    while (lowtic < clock.gametic / net.ticdup + counts)
    {
        netUpdate();
        lowtic = DOOM_MAXINT;

        for (i = 0; i < net.doomcom->numnodes; i++)
            if (net.nodeingame[i] && net.nettics[i] < lowtic)
                lowtic = net.nettics[i];

        if (lowtic < clock.gametic / net.ticdup)
            fatalError("Error: tryRunTics: lowtic < gametic");

        // don't stay in here forever -- give the menu a chance to work
        if (currentTic() / net.ticdup - entertic >= 20)
        {
            menuTicker();
            return;
        }
    }

    // run the count * ticdup dics
    while (counts--)
    {
        for (i = 0; i < net.ticdup; i++)
        {
            if (clock.gametic / net.ticdup > lowtic)
                fatalError("Error: gametic>lowtic");
            if (attractMode().advancedemo)
                Doom::doAdvanceDemo();
            menuTicker();
            Doom::gameTicker();
            clock.gametic++;

            // modify command for duplicated tics
            if (i != net.ticdup - 1)
            {
                Ticcmd* cmd;
                int buf;

                buf = (clock.gametic / net.ticdup) % BACKUPTICS;
                for (int j = 0; j < MAXPLAYERS; j++)
                {
                    cmd = &net.netcmds[j][buf];
                    cmd->chatchar = 0;
                    if (cmd->buttons & BT_SPECIAL)
                        cmd->buttons = 0;
                }
            }
        }
        netUpdate(); // check for new console commands
    }
}

} // namespace Doom
