// The vanilla random API, now a shim over Doom::Random (Sim/Random.h), which owns
// the table and both indices.
//
// rndindex and prndindex are *references* into that object, so the ninety-odd
// call sites that still read them - and the test probe, which hashes prndindex
// every tic - go on seeing the same four bytes at the same address. There is one
// copy of the state and two names for it, which is what lets the object exist
// before the engine is ready to be handed one.

#include "Sim/Random.h"

#include "doom_config.h"
#include "m_random.h"


const unsigned char* rndtable = Doom::Random::table().data();



