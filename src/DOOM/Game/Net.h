#pragma once

namespace Doom
{
// Netcode + the tic run loop; d_net.cpp keeps the vanilla names as shims.
void netUpdate();
void tryRunTics();
void dCheckNetGame();
void dQuitNetGame();
} // namespace Doom
