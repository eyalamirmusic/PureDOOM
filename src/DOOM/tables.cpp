// The vanilla trig-table API, now a shim over Math/Trig.h, which owns the data.
//
// The tables themselves moved to Math/Trig.cpp - unchanged, digit for digit; the
// whole-table checksums in Tests/Sim/PrimitiveTests.cpp are what say so. What is
// left here is the vanilla *view* of them: the bare pointers the renderer and the
// playsim still index into. There is one copy of the numbers, and both names see
// it.
//
// finecosine is a quarter turn into finesine rather than a table of its own,
// which is why finesine is five quarters long. It used to be defined over in
// r_main.cpp, a long way from the data it points at; it lives beside the table
// now.

#include "Math/Trig.h"

#include "Host/Platform.h"
#include "Math/TrigTables.h"

const fixed_t* finesine = Doom::fineSineTable.data();
const fixed_t* finecosine = Doom::fineSineTable.data() + FINEANGLES / 4;
const fixed_t* finetangent = Doom::fineTangentTable.data();
const angle_t* tantoangle = Doom::tanToAngleTable.data();

