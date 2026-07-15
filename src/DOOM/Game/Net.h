#pragma once

namespace Doom
{
// Netcode + the tic run loop; d_net.cpp keeps the vanilla names as shims.
void netUpdate(void);
void tryRunTics(void);
void dCheckNetGame(void);
void dQuitNetGame(void);
} // namespace Doom
