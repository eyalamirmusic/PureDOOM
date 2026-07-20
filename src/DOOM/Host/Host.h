#pragma once

#include "../DOOM.h"

// Doom::Host and Doom::host() are declared in DOOM.h now - they are the public
// interface an embedder assigns its platform hooks through. Host.cpp holds the
// singleton and the built-in defaults its constructor installs.
//
// Unlike the Engine's state - which *is* the world, and which a test wants a
// fresh copy of - these are host/platform state: set once by the embedder and
// the same whichever world is running. The Host must NOT be reset with the
// Engine; it lives in its own singleton, parallel to engine() but deliberately
// separate. The doom_print / doom_malloc / ... names the engine calls through
// are references onto these members (Platform.h).
