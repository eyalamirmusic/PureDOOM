#pragma once

#include "../i_net.h" // vanilla I_ network interface

namespace Doom
{
// The engine's network seam. PureDOOM is single-player: the real socket code is
// compiled out behind I_NET_ENABLED, so the live surface is just I_InitNetwork
// (allocate doomcom, parse -dup/-port/-net, set up the single-player node) and
// an empty I_NetCmd. i_net.cpp keeps the vanilla I_ names as shims over these;
// everything else (DOOMPORT, netget/netsend, the packet helpers) is file-local
// to Net.cpp.
void I_InitNetwork(void);
void I_NetCmd(void);
} // namespace Doom
