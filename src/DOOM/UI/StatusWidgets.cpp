// Rewritten out of vanilla st_lib into namespace Doom.
//
// Status-bar widgets: a difference-drawn number, a number with a trailing percent
// glyph, a multi-icon (the arms/faces), and a binary on/off icon. st_stuff.cpp
// shims the STlib_ names; the minus-sign patch is the only global, now a
// Doom::StatusWidgetGraphics member owned by the Engine (reached by a reference
// alias). Covered by the frame goldens (the bar lands in screens[0]).

#include "../doom_config.h"

#include "../doomdef.h"
#include "../i_system.h"
#include "../m_swap.h"
#include "../r_local.h"
#include "../st_lib.h"
#include "../st_stuff.h"
#include "../v_video.h"
#include "../w_wad.h"

#include "StatusWidgets.h"
#include "StatusWidgetGraphics.h"

namespace Doom
{

// Hack display negative frags: the STTMINUS lump. A Doom::StatusWidgetGraphics member owned by the
// Engine now; this is a reference onto it (initStatusWidgets writes it, so it must be a reference -
// a plain pointer would clobber the reference's storage).
patch_t*& sttminus = statusWidgetGraphics().sttminus;

void drawNum(st_number_t& n);

void initStatusWidgets()
{
    sttminus = static_cast<patch_t*>(W_CacheLumpName("STTMINUS", PU_STATIC));
}

void initNum(st_number_t& n,
             int x,
             int y,
             patch_t** pl,
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
void drawNum(st_number_t& n)
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
        I_Error("Error: drawNum: n->y - ST_Y < 0");

    V_CopyRect(x, n.y - ST_Y, STLIB_BG, w * numdigits, h, x, n.y, STLIB_FG);

    // if non-number, do not draw it
    if (num == 1994)
        return;

    x = n.x;

    // in the special case of 0, you draw 0
    if (!num)
        V_DrawPatch(x - w, n.y, STLIB_FG, n.p[0]);

    // draw the new number
    while (num && numdigits--)
    {
        x -= w;
        V_DrawPatch(x, n.y, STLIB_FG, n.p[num % 10]);
        num /= 10;
    }

    // draw a minus sign if necessary
    if (neg)
        V_DrawPatch(x - 8, n.y, STLIB_FG, sttminus);
}

void updateNum(st_number_t& n, doom_boolean refresh)
{
    (void) refresh;
    if (*n.on)
        drawNum(n);
}

void initPercent(st_percent_t& p,
                 int x,
                 int y,
                 patch_t** pl,
                 int* num,
                 doom_boolean* on,
                 patch_t* percent)
{
    initNum(p.n, x, y, pl, num, on, 3);
    p.p = percent;
}

void updatePercent(st_percent_t& per, int refresh)
{
    if (refresh && *per.n.on)
        V_DrawPatch(per.n.x, per.n.y, STLIB_FG, per.p);

    updateNum(per.n, refresh);
}

void initMultIcon(
    st_multicon_t& i, int x, int y, patch_t** il, int* inum, doom_boolean* on)
{
    i.x = x;
    i.y = y;
    i.oldinum = -1;
    i.inum = inum;
    i.on = on;
    i.p = il;
}

void updateMultIcon(st_multicon_t& mi, doom_boolean refresh)
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
                I_Error("Error: updateMultIcon: y - ST_Y < 0");

            V_CopyRect(x, y - ST_Y, STLIB_BG, w, h, x, y, STLIB_FG);
        }
        V_DrawPatch(mi.x, mi.y, STLIB_FG, mi.p[*mi.inum]);
        mi.oldinum = *mi.inum;
    }
}

void initBinIcon(
    st_binicon_t& b, int x, int y, patch_t* i, doom_boolean* val, doom_boolean* on)
{
    b.x = x;
    b.y = y;
    b.oldval = 0;
    b.val = val;
    b.on = on;
    b.p = i;
}

void updateBinIcon(st_binicon_t& bi, doom_boolean refresh)
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
            I_Error("Error: updateBinIcon: y - ST_Y < 0");

        if (*bi.val)
            V_DrawPatch(bi.x, bi.y, STLIB_FG, bi.p);
        else
            V_CopyRect(x, y - ST_Y, STLIB_BG, w, h, x, y, STLIB_FG);

        bi.oldval = *bi.val;
    }
}

} // namespace Doom
