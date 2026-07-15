// Rewritten out of vanilla m_cheat into namespace Doom.
//
// Cheat-sequence matching: a rolling scrambled-key comparison against a target
// sequence. m_cheat.cpp shims the cht_ names; the scramble table is file-local.
// The demos never enter a cheat, so this is a faithful transcription driven by
// the status bar / automap responders.

#include "../doom_config.h"

#include "../m_cheat.h"

#include "Cheat.h"

namespace Doom
{

static int firsttime = 1;
static unsigned char cheat_xlate_table[256];

//
// Called in st_stuff module, which handles the input.
// Returns a 1 if the cheat was successful, 0 if failed.
//
int checkCheat(cheatseq_t* cht, char key)
{
    int i;
    int rc = 0;

    if (firsttime)
    {
        firsttime = 0;
        for (i = 0; i < 256; i++)
            cheat_xlate_table[i] = SCRAMBLE(i);
    }

    if (!cht->p)
        cht->p = cht->sequence; // initialize if first time

    if (*cht->p == 0)
        *(cht->p++) = key;
    else if (cheat_xlate_table[(unsigned char) key] == *cht->p)
        cht->p++;
    else
        cht->p = cht->sequence;

    if (*cht->p == 1)
        cht->p++;
    else if (*cht->p == 0xff) // end of sequence character
    {
        cht->p = cht->sequence;
        rc = 1;
    }

    return rc;
}

void getParam(cheatseq_t* cht, char* buffer)
{
    unsigned char *p, c;

    p = cht->sequence;
    while (*(p++) != 1)
    {
    }

    do
    {
        c = *p;
        *(buffer++) = c;
        *(p++) = 0;
    } while (c && *p != 0xff);

    if (*p == 0xff)
        *buffer = 0;
}

} // namespace Doom
