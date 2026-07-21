// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        The engine's network seam. Rewritten from i_net.cpp, which keeps the
//        vanilla I_ names as shims. PureDOOM ships single-player, so the socket
//        code lives behind I_NET_ENABLED (off by default) and only the
//        single-player init path runs. doomcom / netbuffer live on the Engine's
//        NetState and netgame on its GameSession, reached through their accessors.
//
//-----------------------------------------------------------------------------

#if defined(I_NET_ENABLED)
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif
#include "Platform.h"

#if defined(I_NET_ENABLED)
#if defined(DOOM_WIN32)
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <winsock.h>
#define IPPORT_USERRESERVED 5000
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#define SOCKET int
#endif
#else
#define IPPORT_USERRESERVED 5000
#endif

#include "../Game/NetTypes.h"
#include "../Game/Args.h"
#include "../Game/MapSpawns.h"

#include "../Game/GameSession.h"
#include "../Game/NetState.h"
#include "Net.h"

#include "../Game/Args.h"
#include "../Containers.h"

// The byte-swap helpers are only reached by the socket packet code, so they are
#include "System.h"
// scoped to it: defining them in the single-player build risks clashing with a
// host header's own ntohl/htonl.
#if defined(I_NET_ENABLED)
// For some odd reason...
#if !defined(DOOM_APPLE) // It doesn't complain on Win32? O_o
#define ntohl(x)                                                                    \
    ((unsigned long int) ((((unsigned long int) (x) & 0x000000ffU) << 24)           \
                          | (((unsigned long int) (x) & 0x0000ff00U) << 8)          \
                          | (((unsigned long int) (x) & 0x00ff0000U) >> 8)          \
                          | (((unsigned long int) (x) & 0xff000000U) >> 24)))

#define ntohs(x)                                                                    \
    ((unsigned short int) ((((unsigned short int) (x) & 0x00ff) << 8)               \
                           | (((unsigned short int) (x) & 0xff00) >> 8)))

#define htonl(x) ntohl(x)
#define htons(x) ntohs(x)
#endif
#endif

namespace Doom
{

//
// NETWORKING
//

int DOOMPORT = (IPPORT_USERRESERVED + 0x1d);
int DOOMPORT_SEND = (IPPORT_USERRESERVED + 0x1e);

#if defined(I_NET_ENABLED)
SOCKET sendsocket;
SOCKET insocket;

struct sockaddr_in sendaddress[MAXNETNODES];
#endif

void (*netget)();
void (*netsend)();

//
// UDPsocket
//
#if defined(I_NET_ENABLED)
SOCKET UDPsocket()
{
    SOCKET s;

    // allocate a socket
    s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0)
    {
        // fatalError("Error: can't create socket: %s", strerror(errno));

        fatalError("Error: can't create socket: ", strerror(errno));
    }

    return s;
}
#endif

//
// BindToLocalPort
//
#if defined(I_NET_ENABLED)
void BindToLocalPort(SOCKET s, int port)
{
    int v;
    struct sockaddr_in address;

    doom_memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = port;

    v = bind(s, (void*) &address, sizeof(address));
    if (v == -1)
    {
        // fatalError("Error: BindToPort: bind: %s", strerror(errno));

        fatalError("Error: BindToPort: bind: ", strerror(errno));
    }
}
#endif

//
// PacketSend
//
void PacketSend()
{
#if defined(I_NET_ENABLED)
    int c;
    NetPacket sw;

    auto& net = netState();

    // byte swap
    sw.checksum = htonl(net.netbuffer->checksum);
    sw.player = net.netbuffer->player;
    sw.retransmitfrom = net.netbuffer->retransmitfrom;
    sw.starttic = net.netbuffer->starttic;
    sw.numtics = net.netbuffer->numtics;
    for (c = 0; c < net.netbuffer->numtics; c++)
    {
        sw.cmds[c].forwardmove = net.netbuffer->cmds[c].forwardmove;
        sw.cmds[c].sidemove = net.netbuffer->cmds[c].sidemove;
        sw.cmds[c].angleturn = htons(net.netbuffer->cmds[c].angleturn);
        sw.cmds[c].consistancy = htons(net.netbuffer->cmds[c].consistancy);
        sw.cmds[c].chatchar = net.netbuffer->cmds[c].chatchar;
        sw.cmds[c].buttons = net.netbuffer->cmds[c].buttons;
    }

    // doom_print ("sending %i\n",gametic);
    c = sendto(sendsocket,
               (const char*) &sw,
               net.doomcom->datalength,
               0,
               (void*) &sendaddress[net.doomcom->remotenode],
               sizeof(sendaddress[net.doomcom->remotenode]));
#endif
}

//
// PacketGet
//
void PacketGet()
{
#if defined(I_NET_ENABLED)
    int i;
    int c;
    struct sockaddr_in fromaddress;
#if defined(__APPLE__)
    socklen_t fromlen;
#else
    int fromlen;
#endif
    NetPacket sw;

    auto& net = netState();

    fromlen = sizeof(fromaddress);
    c = recvfrom(insocket,
                 (char*) &sw,
                 sizeof(sw),
                 0,
                 (struct sockaddr*) &fromaddress,
                 &fromlen);
    if (c == -1)
    {
#if defined(DOOM_WIN32)
        int r = WSAGetLastError();
        if (r != WSAEWOULDBLOCK)
        {
            // fatalError("Error: GetPacket: %i", r);

            fatalError("Error: GetPacket: ", r);
        }
#else
        if (errno != EWOULDBLOCK)
        {
            // fatalError("Error: GetPacket: %s", strerror(errno));

            fatalError("Error: GetPacket: ", strerror(errno));
        }
#endif
        net.doomcom->remotenode = -1; // no packet
        return;
    }

    {
        static int first = 1;
        if (first)
        {
            // doom_print("len=%d:p=[0x%x 0x%x] \n", c, *(int*)&sw, *((int*)&sw
            // + 1));
            print("len=",
                  c,
                  ":p=[0x",
                  hexString(*(int*) &sw),
                  " 0x",
                  hexString(*((int*) &sw + 1)),
                  "] \n");
        }
        first = 0;
    }

    // find remote node number
    for (i = 0; i < net.doomcom->numnodes; i++)
        if (fromaddress.sin_addr.s_addr == sendaddress[i].sin_addr.s_addr)
            break;

    if (i == net.doomcom->numnodes)
    {
        // packet is not from one of the players (new game broadcast)
        net.doomcom->remotenode = -1; // no packet
        return;
    }

    net.doomcom->remotenode = i; // good packet from a game player
    net.doomcom->datalength = c;

    // byte swap
    net.netbuffer->checksum = ntohl(sw.checksum);
    net.netbuffer->player = sw.player;
    net.netbuffer->retransmitfrom = sw.retransmitfrom;
    net.netbuffer->starttic = sw.starttic;
    net.netbuffer->numtics = sw.numtics;

    for (c = 0; c < net.netbuffer->numtics; c++)
    {
        net.netbuffer->cmds[c].forwardmove = sw.cmds[c].forwardmove;
        net.netbuffer->cmds[c].sidemove = sw.cmds[c].sidemove;
        net.netbuffer->cmds[c].angleturn = ntohs(sw.cmds[c].angleturn);
        net.netbuffer->cmds[c].consistancy = ntohs(sw.cmds[c].consistancy);
        net.netbuffer->cmds[c].chatchar = sw.cmds[c].chatchar;
        net.netbuffer->cmds[c].buttons = sw.cmds[c].buttons;
    }
#endif
}

int GetLocalAddress()
{
#if defined(I_NET_ENABLED)
    Array<char, 1024> hostname;
    struct hostent* hostentry; // host information entry
    int v;

    // get local address
    v = gethostname(hostname.data(), sizeof(hostname));
    if (v == -1)
    {
        // fatalError("Error: GetLocalAddress : gethostname: errno %d", errno);

        fatalError("Error: GetLocalAddress : gethostname: errno ", strerror(errno));
    }

    hostentry = gethostbyname(hostname);
    if (!hostentry)
    {
        fatalError(
            "Error: GetLocalAddress : gethostbyname: couldn't get local host");
    }

    return *(int*) hostentry->h_addr_list[0];
#else
    return 0;
#endif
}

//
// initNetwork
//
void initNetwork()
{
#if defined(I_NET_ENABLED)
#if defined(DOOM_WIN32)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    u_long trueval = 1;
#else
    // An int, not a bool, and this one is not a spelling choice: the address is
    // handed to ioctl(FIONBIO) below, which reads a full word back out through
    // it. A one-byte bool makes that read three bytes of neighbouring stack.
    // The Windows arm above says the same thing in its own ABI's spelling.
    int trueval = 1;
#endif
    struct hostent* hostentry; // host information entry
#endif
    int i;
    int p;

    // RAII now (Step 9): doomcom is the view onto NetState's owned doomcomStorage,
    // value-initialised here as the old doom_malloc + doom_memset(0) block was.
    auto& net = Doom::netState();
    net.doomcomStorage = DoomCom {};
    net.doomcom = &net.doomcomStorage;

    // set up for network
    i = Doom::checkParm("-dup");
    if (i && i < myargCount() - 1)
    {
        net.doomcom->ticdup = myargv[i + 1][0] - '0';
        if (net.doomcom->ticdup < 1)
            net.doomcom->ticdup = 1;
        if (net.doomcom->ticdup > 9)
            net.doomcom->ticdup = 9;
    }
    else
        net.doomcom->ticdup = 1;

    if (Doom::checkParm("-extratic"))
        net.doomcom->extratics = 1;
    else
        net.doomcom->extratics = 0;

    p = Doom::checkParm("-port");
    if (p && p < myargCount() - 1)
    {
        DOOMPORT = parseInt(myargv[p + 1]);
        // doom_print("using alternate port %i\n", DOOMPORT);
        print("using alternate port ", DOOMPORT, "\n");
    }

    p = Doom::checkParm("-sendport");
    if (p && p < myargCount() - 1)
    {
        DOOMPORT_SEND = parseInt(myargv[p + 1]);
        // doom_print("using alternate send port %i\n", DOOMPORT_SEND);
        print("using alternate send port ", DOOMPORT_SEND, "\n");
    }

    // parse network game options,
    //  -net <consoleplayer> <host> <host> ...
    i = Doom::checkParm("-net");
    if (!i)
    {
        // single player game
        gameSession().netgame = false;
        net.doomcom->id = DOOMCOM_ID;
        net.doomcom->numplayers = net.doomcom->numnodes = 1;
        net.doomcom->deathmatch = false;
        net.doomcom->consoleplayer = 0;
        return;
    }

#if defined(I_NET_ENABLED)
    netsend = PacketSend;
    netget = PacketGet;
    gameSession().netgame = true;

    // parse player number and host list
    net.doomcom->consoleplayer = myargv[i + 1][0] - '1';

    net.doomcom->numnodes = 1; // this node for sure

    i++;
    while (++i < myargCount() && myargv[i][0] != '-')
    {
        sendaddress[net.doomcom->numnodes].sin_family = AF_INET;
        sendaddress[net.doomcom->numnodes].sin_port = htons(DOOMPORT);
        if (myargv[i][0] == '.')
        {
            sendaddress[net.doomcom->numnodes].sin_addr.s_addr =
                inet_addr(myargv[i].c_str() + 1);
        }
        else
        {
            hostentry = gethostbyname(myargv[i].c_str());
            if (!hostentry)
            {
                // fatalError("Error: gethostbyname: couldn't find %s", myargv[i]);

                fatalError("Error: gethostbyname: couldn't find ", myargv[i]);
            }
            sendaddress[net.doomcom->numnodes].sin_addr.s_addr =
                *(int*) hostentry->h_addr_list[0];
        }
        net.doomcom->numnodes++;
    }

    net.doomcom->id = DOOMCOM_ID;
    net.doomcom->numplayers = net.doomcom->numnodes;

    // build message to receive
    insocket = UDPsocket();
    BindToLocalPort(insocket, htons(DOOMPORT));
#if defined(DOOM_WIN32)
    ioctlsocket(insocket, FIONBIO, &trueval);
#else
    ioctl(insocket, FIONBIO, &trueval);
#endif

    sendsocket = UDPsocket();
#endif
}

void netCommand()
{
#if defined(I_NET_ENABLED)
    auto& net = netState();

    // command is a short wire field; hSendPacket/hGetPacket write the enum through
    // the same cast. This block is never compiled here (I_NET_ENABLED is unset in
    // every build in this repository), so nothing gates it but inspection.
    if (static_cast<NetCommandKind>(net.doomcom->command) == NetCommandKind::Send)
    {
        netsend();
    }
    else if (static_cast<NetCommandKind>(net.doomcom->command)
             == NetCommandKind::Get)
    {
        netget();
    }
    else
    {
        // fatalError("Error: Bad net cmd: %i\n", doomcom->command);

        fatalError("Error: Bad net cmd: ", net.doomcom->command);
    }
#endif
}

} // namespace Doom
