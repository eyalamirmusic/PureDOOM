#pragma once

namespace Doom
{
// Netcode + the tic run loop; d_net.cpp keeps the vanilla names as shims.
void netUpdate();
void tryRunTics();
void checkNetGame();
void quitNetGame();
} // namespace Doom
