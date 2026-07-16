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
// dCheckNetGame / dQuitNetGame. The net buffers and tic counters (netcmds,
// maketic, ticdup, ... read by g_game and the loop) stay at file scope here,
// above the namespace. PureDOOM runs singletics; netUpdate's no-command-on-
// singletics behaviour is load-bearing (CLAUDE.md) and preserved verbatim. The
// demos drive the tic loop, so the goldens pin it.

#include "../doom_config.h"

#include "../m_menu.h"
#include "../i_system.h"
#include "../i_video.h"
#include "../i_net.h"
#include "../g_game.h"
#include "../doomdef.h"
#include "../doomstat.h"

#include "NetState.h"

#include "Net.h"

//
// NETWORKING
//
// gametic is the tic about to (or currently being) run
// maketic is the tick that hasn't had control made for it yet
// nettics[] has the maketics for all players
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

// The netcode buffers and tic bookkeeping are a Doom::NetState owned by the Engine now; these
// (and maketic/ticdup below) are references onto it, the arrays as references-to-array
// (REFACTOR.md, Step 5).
doomcom_t*& doomcom = Doom::netState().doomcom;
doomdata_t*& netbuffer = Doom::netState().netbuffer; // points inside doomcom

ticcmd_t (&localcmds)[BACKUPTICS] = Doom::netState().localcmds;

ticcmd_t (&netcmds)[MAXPLAYERS][BACKUPTICS] = Doom::netState().netcmds;
int (&nettics)[MAXNETNODES] = Doom::netState().nettics;
// Game/Net's private bookkeeping is a Doom::NetState now (Engine); references onto its members.
doom_boolean (&nodeingame)[MAXNETNODES] = Doom::netState().nodeingame;
doom_boolean (&remoteresend)[MAXNETNODES] = Doom::netState().remoteresend;
int (&resendto)[MAXNETNODES] = Doom::netState().resendto;
int (&resendcount)[MAXNETNODES] = Doom::netState().resendcount;

int (&nodeforplayer)[MAXPLAYERS] = Doom::netState().nodeforplayer;

int& maketic = Doom::netState().maketic;
int& lastnettic = Doom::netState().lastnettic;
int& skiptics = Doom::netState().skiptics;
int& ticdup = Doom::netState().ticdup;
int& maxsend = Doom::netState().maxsend; // BACKUPTICS/(2*ticdup)-1

doom_boolean& reboundpacket = Doom::netState().reboundpacket;
doomdata_t& reboundstore = Doom::netState().reboundstore;

char (&exitmsg)[80] = Doom::netState().exitmsg;
int& gametime = Doom::netState().gametime;
int (&frametics)[4] = Doom::netState().frametics;
int& frameon = Doom::netState().frameon;
int (&frameskip)[4] = Doom::netState().frameskip;
int& oldnettics = Doom::netState().oldnettics;

extern doom_boolean& advancedemo; // Doom::AttractMode (Engine member)

void D_ProcessEvents(void);
void G_BuildTiccmd(ticcmd_t* cmd);
void D_DoAdvanceDemo(void);

namespace Doom
{

int netbufferSize(void)
{
    return (int) (long long) &(((doomdata_t*) 0)->cmds[netbuffer->numtics]);
}

//
// Checksum
//
unsigned netbufferChecksum(void)
{
    unsigned c;
    int i, l;

    c = 0x1234567;

    // FIXME -endianess?
    // #ifdef NORMALUNIX
    return 0; // byte order problems
    // #endif

    l = (netbufferSize() - (int) (long long) &(((doomdata_t*) 0)->retransmitfrom))
        / 4;
    for (i = 0; i < l; i++)
        c += ((unsigned*) &netbuffer->retransmitfrom)[i] * (i + 1);

    return c & NCMD_CHECKSUM;
}

//
//
//
int expandTics(int low)
{
    int delta;

    delta = low - (maketic & 0xff);

    if (delta >= -64 && delta <= 64)
        return (maketic & ~0xff) + low;
    if (delta > 64)
        return (maketic & ~0xff) - 256 + low;
    if (delta < -64)
        return (maketic & ~0xff) + 256 + low;

    //I_Error("Error: expandTics: strange value %i at maketic %i", low, maketic);
    doom_strcpy(error_buf, "Error: expandTics: strange value ");
    doom_concat(error_buf, doom_itoa(low, 10));
    doom_concat(error_buf, " at maketic ");
    doom_concat(error_buf, doom_itoa(maketic, 10));
    I_Error(error_buf);
    return 0;
}

//
// hSendPacket
//
void hSendPacket(int node, int flags)
{
    netbuffer->checksum = netbufferChecksum() | flags;

    if (!node)
    {
        reboundstore = *netbuffer;
        reboundpacket = true;
        return;
    }

    if (demoplayback)
        return;

    if (!netgame)
        I_Error("Error: Tried to transmit to another node");

    doomcom->command = CMD_SEND;
    doomcom->remotenode = node;
    doomcom->datalength = netbufferSize();

    if (debugfile)
    {
        int i;
        int realretrans;
        if (netbuffer->checksum & NCMD_RETRANSMIT)
            realretrans = expandTics(netbuffer->retransmitfrom);
        else
            realretrans = -1;

        {
            //fprintf(debugfile, "send (%i + %i, R %i) [%i] ",
            //        expandTics(netbuffer->starttic),
            //        netbuffer->numtics, realretrans, doomcom->datalength);
            doom_fprint(debugfile, "send (");
            doom_fprint(debugfile, doom_itoa(expandTics(netbuffer->starttic), 10));
            doom_fprint(debugfile, " + ");
            doom_fprint(debugfile, doom_itoa(netbuffer->numtics, 10));
            doom_fprint(debugfile, ", R ");
            doom_fprint(debugfile, doom_itoa(realretrans, 10));
            doom_fprint(debugfile, ") [");
            doom_fprint(debugfile, doom_itoa(doomcom->datalength, 10));
            doom_fprint(debugfile, "] ");
        }

        for (i = 0; i < doomcom->datalength; i++)
        {
            //fprintf(debugfile, "%i ", ((byte*)netbuffer)[i]);
            doom_fprint(debugfile, doom_itoa(((byte*) netbuffer)[i], 10));
            doom_fprint(debugfile, " ");
        }

        doom_fprint(debugfile, "\n");
    }

    I_NetCmd();
}

//
// hGetPacket
// Returns false if no packet is waiting
//
doom_boolean hGetPacket(void)
{
    if (reboundpacket)
    {
        *netbuffer = reboundstore;
        doomcom->remotenode = 0;
        reboundpacket = false;
        return true;
    }

    if (!netgame)
        return false;

    if (demoplayback)
        return false;

    doomcom->command = CMD_GET;
    I_NetCmd();

    if (doomcom->remotenode == -1)
        return false;

    if (doomcom->datalength != netbufferSize())
    {
        if (debugfile)
        {
            //fprintf(debugfile, "bad packet length %i\n", doomcom->datalength);
            doom_fprint(debugfile, "bad packet length ");
            doom_fprint(debugfile, doom_itoa(doomcom->datalength, 10));
            doom_fprint(debugfile, "\n");
        }
        return false;
    }

    if (netbufferChecksum() != (netbuffer->checksum & NCMD_CHECKSUM))
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
        int i;

        if (netbuffer->checksum & NCMD_SETUP)
        {
            doom_fprint(debugfile, "setup packet\n");
        }
        else
        {
            if (netbuffer->checksum & NCMD_RETRANSMIT)
                realretrans = expandTics(netbuffer->retransmitfrom);
            else
                realretrans = -1;

            {
                //fprintf(debugfile, "get %i = (%i + %i, R %i)[%i] ",
                //        doomcom->remotenode,
                //        expandTics(netbuffer->starttic),
                //        netbuffer->numtics, realretrans, doomcom->datalength);
                doom_fprint(debugfile, "get ");
                doom_fprint(debugfile, doom_itoa(doomcom->remotenode, 10));
                doom_fprint(debugfile, " = (");
                doom_fprint(debugfile,
                            doom_itoa(expandTics(netbuffer->starttic), 10));
                doom_fprint(debugfile, " + ");
                doom_fprint(debugfile, doom_itoa(netbuffer->numtics, 10));
                doom_fprint(debugfile, ", R ");
                doom_fprint(debugfile, doom_itoa(realretrans, 10));
                doom_fprint(debugfile, ")[");
                doom_fprint(debugfile, doom_itoa(doomcom->datalength, 10));
                doom_fprint(debugfile, "] ");
            }

            for (i = 0; i < doomcom->datalength; i++)
            {
                //fprintf(debugfile, "%i ", ((byte*)netbuffer)[i]);
                doom_fprint(debugfile, doom_itoa(((byte*) netbuffer)[i], 10));
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
void getPackets(void)
{
    int netconsole;
    int netnode;
    ticcmd_t *src, *dest;
    int realend;
    int realstart;

    while (hGetPacket())
    {
        if (netbuffer->checksum & NCMD_SETUP)
            continue; // extra setup packet

        netconsole = netbuffer->player & ~PL_DRONE;
        netnode = doomcom->remotenode;

        // to save bytes, only the low byte of tic numbers are sent
        // Figure out what the rest of the bytes are
        realstart = expandTics(netbuffer->starttic);
        realend = (realstart + netbuffer->numtics);

        // check for exiting the game
        if (netbuffer->checksum & NCMD_EXIT)
        {
            if (!nodeingame[netnode])
                continue;
            nodeingame[netnode] = false;
            playeringame[netconsole] = false;
            doom_strcpy(exitmsg, "Player 1 left the game");
            exitmsg[7] += netconsole;
            players[consoleplayer].message = exitmsg;
            if (demorecording)
                G_CheckDemoStatus();
            continue;
        }

        // check for a remote game kill
        if (netbuffer->checksum & NCMD_KILL)
            I_Error("Error: Killed by network driver");

        nodeforplayer[netconsole] = netnode;

        // check for retransmit request
        if (resendcount[netnode] <= 0 && (netbuffer->checksum & NCMD_RETRANSMIT))
        {
            resendto[netnode] = expandTics(netbuffer->retransmitfrom);
            if (debugfile)
            {
                //fprintf(debugfile, "retransmit from %i\n", resendto[netnode]);
                doom_fprint(debugfile, "retransmit from ");
                doom_fprint(debugfile, doom_itoa(resendto[netnode], 10));
                doom_fprint(debugfile, "\n");
            }
            resendcount[netnode] = RESENDCOUNT;
        }
        else
            resendcount[netnode]--;

        // check for out of order / duplicated packet
        if (realend == nettics[netnode])
            continue;

        if (realend < nettics[netnode])
        {
            if (debugfile)
            {
                //fprintf(debugfile,
                //        "out of order packet (%i + %i)\n",
                //        realstart, netbuffer->numtics);
                doom_fprint(debugfile, "out of order packet (");
                doom_fprint(debugfile, doom_itoa(realstart, 10));
                doom_fprint(debugfile, " + ");
                doom_fprint(debugfile, doom_itoa(netbuffer->numtics, 10));
                doom_fprint(debugfile, ")\n");
            }
            continue;
        }

        // check for a missed packet
        if (realstart > nettics[netnode])
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
                doom_fprint(debugfile, doom_itoa(nettics[netnode], 10));
                doom_fprint(debugfile, ")\n");
            }
            remoteresend[netnode] = true;
            continue;
        }

        // update command store from the packet
        {
            int start;

            remoteresend[netnode] = false;

            start = nettics[netnode] - realstart;
            src = &netbuffer->cmds[start];

            while (nettics[netnode] < realend)
            {
                dest = &netcmds[netconsole][nettics[netnode] % BACKUPTICS];
                nettics[netnode]++;
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
void netUpdate(void)
{
    int nowtime;
    int newtics;
    int i, j;
    int realstart;
    int gameticdiv;

    // check time
    nowtime = I_GetTime() / ticdup;
    newtics = nowtime - gametime;
    gametime = nowtime;

    if (newtics <= 0) // nothing new to update
        goto listen;

    if (skiptics <= newtics)
    {
        newtics -= skiptics;
        skiptics = 0;
    }
    else
    {
        skiptics -= newtics;
        newtics = 0;
    }

    netbuffer->player = consoleplayer;

    // build new ticcmds for console player
    gameticdiv = gametic / ticdup;
    for (i = 0; i < newtics; i++)
    {
        I_StartTic();
        D_ProcessEvents();

        // [pd] A singletic update is synchronous: D_DoomLoop builds the tic's
        // command and runs it in the same breath, advancing maketic and gametic
        // together. Building another one here would consume the input that
        // command is about to read, and would advance maketic with no gametic to
        // match it -- and this runs from D_Display and R_RenderPlayerView too,
        // which vanilla called to keep the netcode fed while a slow frame
        // rendered. maketic therefore climbed until it jammed against the cap
        // below and stayed there, and since D_DoomLoop writes the command to
        // netcmds[maketic] while G_Ticker reads netcmds[gametic], every command
        // was executed five tics (143ms) after it was built. Events are still
        // drained above; there is simply no second command to build.
        if (singletics)
            continue;

        if (maketic - gameticdiv >= BACKUPTICS / 2 - 1)
            break; // can't hold any more

        //doom_print ("mk:%i ",maketic);
        G_BuildTiccmd(&localcmds[maketic % BACKUPTICS]);
        maketic++;
    }

    if (singletics)
        return; // singletic update is syncronous

    // send the packet to the other nodes
    for (i = 0; i < doomcom->numnodes; i++)
        if (nodeingame[i])
        {
            netbuffer->starttic = realstart = resendto[i];
            netbuffer->numtics = maketic - realstart;
            if (netbuffer->numtics > BACKUPTICS)
                I_Error("Error: netUpdate: netbuffer->numtics > BACKUPTICS");

            resendto[i] = maketic - doomcom->extratics;

            for (j = 0; j < netbuffer->numtics; j++)
                netbuffer->cmds[j] = localcmds[(realstart + j) % BACKUPTICS];

            if (remoteresend[i])
            {
                netbuffer->retransmitfrom = nettics[i];
                hSendPacket(i, NCMD_RETRANSMIT);
            }
            else
            {
                netbuffer->retransmitfrom = 0;
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
void checkAbort(void)
{
    event_t* ev;
    int stoptic;

    stoptic = I_GetTime() + 2;
    while (I_GetTime() < stoptic)
        I_StartTic();

    I_StartTic();
    for (; eventtail != eventhead;)
    {
        ev = &events[eventtail];
        if (ev->type == ev_keydown && ev->data1 == KEY_ESCAPE)
            I_Error("Error: Network game synchronization aborted.");
    }

    eventtail++;
    eventtail = (eventtail) & (MAXEVENTS - 1);
}

//
// dArbitrateNetStart
//
void dArbitrateNetStart(void)
{
    int i;
    doom_boolean gotinfo[MAXNETNODES];

    autostart = true;
    doom_memset(gotinfo, 0, sizeof(gotinfo));

    if (doomcom->consoleplayer)
    {
        // listen for setup info from key player
        doom_print("listening for network start info...\n");
        while (1)
        {
            checkAbort();
            if (!hGetPacket())
                continue;
            if (netbuffer->checksum & NCMD_SETUP)
            {
                if (netbuffer->player != VERSION)
                    I_Error(
                        "Error: Different DOOM versions cannot play a net game!");
                startskill = (skill_t) (netbuffer->retransmitfrom & 15);
                deathmatch = (netbuffer->retransmitfrom & 0xc0) >> 6;
                nomonsters = (netbuffer->retransmitfrom & 0x20) > 0;
                respawnparm = (netbuffer->retransmitfrom & 0x10) > 0;
                startmap = netbuffer->starttic & 0x3f;
                startepisode = netbuffer->starttic >> 6;
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
            for (i = 0; i < doomcom->numnodes; i++)
            {
                netbuffer->retransmitfrom = startskill;
                if (deathmatch)
                    netbuffer->retransmitfrom |= (deathmatch << 6);
                if (nomonsters)
                    netbuffer->retransmitfrom |= 0x20;
                if (respawnparm)
                    netbuffer->retransmitfrom |= 0x10;
                netbuffer->starttic = startepisode * 64 + startmap;
                netbuffer->player = VERSION;
                netbuffer->numtics = 0;
                hSendPacket(i, NCMD_SETUP);
            }

#if 1
            for (i = 10; i && hGetPacket(); --i)
            {
                if ((netbuffer->player & 0x7f) < MAXNETNODES)
                    gotinfo[netbuffer->player & 0x7f] = true;
            }
#else
            while (hGetPacket())
            {
                gotinfo[netbuffer->player & 0x7f] = true;
            }
#endif

            for (i = 1; i < doomcom->numnodes; i++)
                if (!gotinfo[i])
                    break;
        } while (i < doomcom->numnodes);
    }
}

//
// dCheckNetGame
// Works out player numbers among the net participants
//
void dCheckNetGame(void)
{
    int i;

    for (i = 0; i < MAXNETNODES; i++)
    {
        nodeingame[i] = false;
        nettics[i] = 0;
        remoteresend[i] = false; // set when local needs tics
        resendto[i] = 0; // which tic to start sending
    }

    // I_InitNetwork sets doomcom and netgame
    I_InitNetwork();
    if (doomcom->id != DOOMCOM_ID)
        I_Error("Error: Doomcom buffer invalid!");

    netbuffer = &doomcom->data;
    consoleplayer = displayplayer = doomcom->consoleplayer;
    if (netgame)
        dArbitrateNetStart();

    //doom_print("startskill %i  deathmatch: %i  startmap: %i  startepisode: %i\n",
    //       startskill, deathmatch, startmap, startepisode);
    doom_print("startskill ");
    doom_print(doom_itoa(startskill, 10));
    doom_print("  deathmatch: ");
    doom_print(doom_itoa(deathmatch, 10));
    doom_print("  startmap: ");
    doom_print(doom_itoa(startmap, 10));
    doom_print("  startepisode: ");
    doom_print(doom_itoa(startepisode, 10));
    doom_print("\n");

    // read values out of doomcom
    ticdup = doomcom->ticdup;
    maxsend = BACKUPTICS / (2 * ticdup) - 1;
    if (maxsend < 1)
        maxsend = 1;

    for (i = 0; i < doomcom->numplayers; i++)
        playeringame[i] = true;
    for (i = 0; i < doomcom->numnodes; i++)
        nodeingame[i] = true;

    //doom_print("player %i of %i (%i nodes)\n",
    //       consoleplayer + 1, doomcom->numplayers, doomcom->numnodes);
    doom_print("player ");
    doom_print(doom_itoa(consoleplayer + 1, 10));
    doom_print(" of ");
    doom_print(doom_itoa(doomcom->numplayers, 10));
    doom_print(" (");
    doom_print(doom_itoa(doomcom->numnodes, 10));
    doom_print(" nodes)\n");
}

//
// dQuitNetGame
// Called before quitting to leave a net game
// without hanging the other players
//
void dQuitNetGame(void)
{
    int i, j;

    if (debugfile)
        doom_close(debugfile);

    if (!netgame || !usergame || consoleplayer == -1 || demoplayback)
        return;

    // send a bunch of packets for security
    netbuffer->player = consoleplayer;
    netbuffer->numtics = 0;
    for (i = 0; i < 4; i++)
    {
        for (j = 1; j < doomcom->numnodes; j++)
            if (nodeingame[j])
                hSendPacket(j, NCMD_EXIT);
        I_WaitVBL(1);
    }
}

//
// tryRunTics
//
void tryRunTics(void)
{
    int i;
    int lowtic;
    int entertic;
    static int oldentertics;
    int realtics;
    int availabletics;
    int counts;

    // get real tics
    entertic = I_GetTime() / ticdup;
    realtics = entertic - oldentertics;
    oldentertics = entertic;

    // get available tics
    netUpdate();

    lowtic = DOOM_MAXINT;
    for (i = 0; i < doomcom->numnodes; i++)
    {
        if (nodeingame[i])
        {
            if (nettics[i] < lowtic)
                lowtic = nettics[i];
        }
    }
    availabletics = lowtic - gametic / ticdup;

    // decide how many tics to run
    if (realtics < availabletics - 1)
        counts = realtics + 1;
    else if (realtics < availabletics)
        counts = realtics;
    else
        counts = availabletics;

    if (counts < 1)
        counts = 1;

    frameon++;

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

    if (!demoplayback)
    {
        // ideally nettics[0] should be 1 - 3 tics above lowtic
        // if we are consistantly slower, speed up time
        for (i = 0; i < MAXPLAYERS; i++)
            if (playeringame[i])
                break;
        if (consoleplayer == i)
        {
            // the key player does not adapt
        }
        else
        {
            if (nettics[0] <= nettics[nodeforplayer[i]])
            {
                gametime--;
                // doom_print ("-");
            }
            frameskip[frameon & 3] = (oldnettics > nettics[nodeforplayer[i]]);
            oldnettics = nettics[0];
            if (frameskip[0] && frameskip[1] && frameskip[2] && frameskip[3])
            {
                skiptics = 1;
                // doom_print ("+");
            }
        }
    } // demoplayback

    // wait for new tics if needed
    while (lowtic < gametic / ticdup + counts)
    {
        netUpdate();
        lowtic = DOOM_MAXINT;

        for (i = 0; i < doomcom->numnodes; i++)
            if (nodeingame[i] && nettics[i] < lowtic)
                lowtic = nettics[i];

        if (lowtic < gametic / ticdup)
            I_Error("Error: tryRunTics: lowtic < gametic");

        // don't stay in here forever -- give the menu a chance to work
        if (I_GetTime() / ticdup - entertic >= 20)
        {
            M_Ticker();
            return;
        }
    }

    // run the count * ticdup dics
    while (counts--)
    {
        for (i = 0; i < ticdup; i++)
        {
            if (gametic / ticdup > lowtic)
                I_Error("Error: gametic>lowtic");
            if (advancedemo)
                D_DoAdvanceDemo();
            M_Ticker();
            G_Ticker();
            gametic++;

            // modify command for duplicated tics
            if (i != ticdup - 1)
            {
                ticcmd_t* cmd;
                int buf;
                int j;

                buf = (gametic / ticdup) % BACKUPTICS;
                for (j = 0; j < MAXPLAYERS; j++)
                {
                    cmd = &netcmds[j][buf];
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
