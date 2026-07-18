// Rewritten out of vanilla st_lib into namespace Doom.
//
// Status-bar widgets: a difference-drawn number, a number with a trailing percent
// glyph, a multi-icon (the arms/faces), and a binary on/off icon. The vanilla
// STlib_ names that used to shim these have been retired; the minus-sign patch
// is the only global, now a Doom::StatusWidgetGraphics member owned by the
// Engine (reached by a reference alias). Covered by the frame goldens (the bar
// lands in screens[0]).

#include "../doom_config.h"

#include "../doomdef.h"
#include "../m_swap.h"
#include "../r_local.h"
#include "../st_lib.h"
#include "../st_stuff.h"
#include "../v_video.h"
#include "../Wad/WadFile.h"

#include "StatusWidgets.h"
#include "StatusWidgetGraphics.h"

#include "../Render/Video.h"
#include "../Host/System.h"
namespace Doom
{

// Hack display negative frags: the STTMINUS lump. A Doom::StatusWidgetGraphics member owned by the
// Engine now; this is a reference onto it (initStatusWidgets writes it, so it must be a reference -
// a plain pointer would clobber the reference's storage).
Patch*& sttminus = statusWidgetGraphics().sttminus;

void drawNum(StatusNumber& n);

void initStatusWidgets()
{
    sttminus = static_cast<Patch*>(Doom::cacheLumpName("STTMINUS"));
}

void initNum(StatusNumber& n,
             int x,
             int y,
             Patch** pl,
             int* num,
             doom_boolean* on,
             int width)
{
    n.x = x;
    n.y = y;
    n.oldnum = 0;
    n.width = width;
    n.num = num;
    n.on = on;
    n.p = pl;
}

//
// A fairly efficient way to draw a number
//  based on differences from the old number.
// Note: worth the trouble?
//
void drawNum(StatusNumber& n)
{
    int numdigits = n.width;
    int num = *n.num;

    int w = SHORT(n.p[0]->width);
    int h = SHORT(n.p[0]->height);
    int x = n.x;

    int neg;

    n.oldnum = *n.num;

    neg = num < 0;

    if (neg)
    {
        if (numdigits == 2 && num < -9)
            num = -9;
        else if (numdigits == 3 && num < -99)
            num = -99;

        num = -num;
    }

    // clear the area
    x = n.x - numdigits * w;

    if (n.y - ST_Y < 0)
        fatalError("Error: drawNum: n->y - ST_Y < 0");

    Doom::copyRect(x, n.y - ST_Y, STLIB_BG, w * numdigits, h, x, n.y, STLIB_FG);

    // if non-number, do not draw it
    if (num == 1994)
        return;

    x = n.x;

    // in the special case of 0, you draw 0
    if (!num)
        Doom::drawPatch(x - w, n.y, STLIB_FG, n.p[0]);

    // draw the new number
    while (num && numdigits--)
    {
        x -= w;
        Doom::drawPatch(x, n.y, STLIB_FG, n.p[num % 10]);
        num /= 10;
    }

    // draw a minus sign if necessary
    if (neg)
        Doom::drawPatch(x - 8, n.y, STLIB_FG, sttminus);
}

void updateNum(StatusNumber& n, doom_boolean refresh)
{
    (void) refresh;
    if (*n.on)
        drawNum(n);
}

void initPercent(StatusPercent& p,
                 int x,
                 int y,
                 Patch** pl,
                 int* num,
                 doom_boolean* on,
                 Patch* percent)
{
    initNum(p.n, x, y, pl, num, on, 3);
    p.p = percent;
}

void updatePercent(StatusPercent& per, int refresh)
{
    if (refresh && *per.n.on)
        Doom::drawPatch(per.n.x, per.n.y, STLIB_FG, per.p);

    updateNum(per.n, refresh);
}

void initMultIcon(
    StatusMultIcon& i, int x, int y, Patch** il, int* inum, doom_boolean* on)
{
    i.x = x;
    i.y = y;
    i.oldinum = -1;
    i.inum = inum;
    i.on = on;
    i.p = il;
}

void updateMultIcon(StatusMultIcon& mi, doom_boolean refresh)
{
    int w;
    int h;
    int x;
    int y;

    if (*mi.on && (mi.oldinum != *mi.inum || refresh) && (*mi.inum != -1))
    {
        if (mi.oldinum != -1)
        {
            x = mi.x - SHORT(mi.p[mi.oldinum]->leftoffset);
            y = mi.y - SHORT(mi.p[mi.oldinum]->topoffset);
            w = SHORT(mi.p[mi.oldinum]->width);
            h = SHORT(mi.p[mi.oldinum]->height);

            if (y - ST_Y < 0)
                fatalError("Error: updateMultIcon: y - ST_Y < 0");

            Doom::copyRect(x, y - ST_Y, STLIB_BG, w, h, x, y, STLIB_FG);
        }
        Doom::drawPatch(mi.x, mi.y, STLIB_FG, mi.p[*mi.inum]);
        mi.oldinum = *mi.inum;
    }
}

void initBinIcon(
    StatusBinIcon& b, int x, int y, Patch* i, doom_boolean* val, doom_boolean* on)
{
    b.x = x;
    b.y = y;
    b.oldval = 0;
    b.val = val;
    b.on = on;
    b.p = i;
}

void updateBinIcon(StatusBinIcon& bi, doom_boolean refresh)
{
    int x;
    int y;
    int w;
    int h;

    if (*bi.on && (bi.oldval != *bi.val || refresh))
    {
        x = bi.x - SHORT(bi.p->leftoffset);
        y = bi.y - SHORT(bi.p->topoffset);
        w = SHORT(bi.p->width);
        h = SHORT(bi.p->height);

        if (y - ST_Y < 0)
            fatalError("Error: updateBinIcon: y - ST_Y < 0");

        if (*bi.val)
            Doom::drawPatch(bi.x, bi.y, STLIB_FG, bi.p);
        else
            Doom::copyRect(x, y - ST_Y, STLIB_BG, w, h, x, y, STLIB_FG);

        bi.oldval = *bi.val;
    }
}

} // namespace Doom
