// Rewritten out of vanilla m_cheat into namespace Doom.
//
// Cheat-sequence matching: a rolling scrambled-key comparison against a target
// sequence. m_cheat.cpp shims the cht_ names; the scramble table is file-local.
// The demos never enter a cheat, so this is a faithful transcription driven by
// the status bar / automap responders.

#include "../Host/Platform.h"

#include "CheatTypes.h"

#include "Cheat.h"

#include "../Containers.h"

#include <string>

namespace Doom
{

static int firsttime = 1;
static Array<unsigned char, 256> cheat_xlate_table;

// Was SCRAMBLE(a): a bit-permutation macro used to build cheat_xlate_table.
// ((((a)&1)<<7) + (((a)&2)<<5) + ((a)&4) + (((a)&8)<<1)
//  + (((a)&16)>>1) + ((a)&32) + (((a)&64)>>5) + (((a)&128)>>7))
static constexpr unsigned char scramble(int a)
{
    return static_cast<unsigned char>(((a & 1) << 7) + ((a & 2) << 5) + (a & 4)
                                      + ((a & 8) << 1) + ((a & 16) >> 1) + (a & 32)
                                      + ((a & 64) >> 5) + ((a & 128) >> 7));
}

//
// Called in st_stuff module, which handles the input.
// Returns a 1 if the cheat was successful, 0 if failed.
//
int checkCheat(CheatSequence& cht, char key)
{
    int rc = 0;

    if (firsttime)
    {
        firsttime = 0;
        for (int i = 0; i < 256; i++)
            cheat_xlate_table[i] = scramble(i);
    }

    auto& sequence = cht.sequence;
    auto& position = cht.position;

    if (sequence[position] == 0)
        sequence[position++] = static_cast<unsigned char>(key);
    else if (cheat_xlate_table[static_cast<unsigned char>(key)]
             == sequence[position])
        position++;
    else
        position = 0;

    if (sequence[position] == 1)
        position++;
    else if (sequence[position] == 0xff) // end of sequence character
    {
        position = 0;
        rc = 1;
    }

    return rc;
}

// The parameter the player typed after the cheat's `1` marker, as text - and it
// clears the slots on the way out, exactly as vanilla did, so the sequence is
// ready to match again.
std::string getParam(CheatSequence& cht)
{
    auto& sequence = cht.sequence;

    auto index = 0;
    while (sequence[index++] != 1)
    {
    }

    auto parameter = std::string {};
    unsigned char c = 0;

    do
    {
        c = sequence[index];
        parameter += static_cast<char>(c);
        sequence[index++] = 0;
    } while (c && sequence[index] != 0xff);

    return parameter;
}

} // namespace Doom
