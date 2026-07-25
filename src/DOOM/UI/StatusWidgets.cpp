// Rewritten out of vanilla st_lib into namespace Doom.
//
// Status-bar widgets: a difference-drawn number, a number with a trailing percent
// glyph, a multi-icon (the arms/faces), and a binary on/off icon. The vanilla
// STlib_ free functions are methods on StatusNumber / StatusPercent /
// StatusMultIcon / StatusBinIcon, declared in StatusWidgetTypes.h; the minus-sign
// patch is the only global, now a StatusWidgetGraphics member owned by the
// Engine (reached by a reference alias). Covered by the frame goldens (the bar
// lands in screens[0]).

#include "../Host/Platform.h"

#include "../Game/GameDefs.h"
#include "../Math/Swap.h"
#include "StatusWidgetTypes.h"
#include "StatusBarTypes.h"
#include "../Wad/WadFile.h"

#include "StatusWidgets.h"
#include "StatusWidgetGraphics.h"

#include "../Render/Video.h"
#include "../Host/System.h"
namespace Doom
{

// Hack display negative frags: the STTMINUS lump. A StatusWidgetGraphics member owned by the
// Engine now; this is a reference onto it (initStatusWidgets writes it, so it must be a reference -
// a plain pointer would clobber the reference's storage).
Patch*& sttminus = statusWidgetGraphics().sttminus;

void initStatusWidgets()
{
    sttminus = static_cast<Patch*>(cacheLumpName("STTMINUS"));
}

void StatusNumber::init(
    Vec2i posToUse, Patch** pl, int* numToUse, bool* onToUse, int widthToUse)
{
    pos = posToUse;
    oldnum = 0;
    width = widthToUse;
    num = numToUse;
    on = onToUse;
    p = pl;
}

//
// A fairly efficient way to draw a number
//  based on differences from the old number.
// Note: worth the trouble?
//
void StatusNumber::draw()
{
    auto numdigits = width;
    int value = *num;

    auto w = littleEndian(p[0]->width);
    auto h = littleEndian(p[0]->height);

    oldnum = *num;

    auto neg = value < 0;

    if (neg)
    {
        if (numdigits == 2 && value < -9)
            value = -9;
        else if (numdigits == 3 && value < -99)
            value = -99;

        value = -value;
    }

    // clear the area
    auto drawX = pos.x - numdigits * w;

    if (pos.y - ST_Y < 0)
        fatalError("Error: StatusNumber::draw: y - ST_Y < 0");

    copyRect({drawX, pos.y - ST_Y},
             STLIB_BG,
             {w * numdigits, h},
             {drawX, pos.y},
             STLIB_FG);

    // if non-number, do not draw it
    if (value == 1994)
        return;

    drawX = pos.x;

    // in the special case of 0, you draw 0
    if (!value)
        drawPatch({drawX - w, pos.y}, STLIB_FG, p[0]);

    // draw the new number
    while (value && numdigits--)
    {
        drawX -= w;
        drawPatch({drawX, pos.y}, STLIB_FG, p[value % 10]);
        value /= 10;
    }

    // draw a minus sign if necessary
    if (neg)
        drawPatch({drawX - 8, pos.y}, STLIB_FG, sttminus);
}

void StatusNumber::update([[maybe_unused]] bool refresh)
{
    if (*on)
        draw();
}

void StatusPercent::init(Vec2i at, Patch** pl, int* num, bool* on, Patch* percent)
{
    n.init(at, pl, num, on, 3);
    p = percent;
}

void StatusPercent::update(int refresh)
{
    if (refresh && *n.on)
        drawPatch(n.pos, STLIB_FG, p);

    n.update(refresh);
}

void StatusMultIcon::init(Vec2i posToUse, Patch** il, int* inumToUse, bool* onToUse)
{
    pos = posToUse;
    oldinum = -1;
    inum = inumToUse;
    on = onToUse;
    p = il;
}

void StatusMultIcon::update(bool refresh)
{
    if (*on && (oldinum != *inum || refresh) && (*inum != -1))
    {
        if (oldinum != -1)
        {
            int oldX = pos.x - littleEndian(p[oldinum]->leftoffset);
            int oldY = pos.y - littleEndian(p[oldinum]->topoffset);
            auto w = littleEndian(p[oldinum]->width);
            auto h = littleEndian(p[oldinum]->height);

            if (oldY - ST_Y < 0)
                fatalError("Error: StatusMultIcon::update: y - ST_Y < 0");

            copyRect({oldX, oldY - ST_Y}, STLIB_BG, {w, h}, {oldX, oldY}, STLIB_FG);
        }
        drawPatch(pos, STLIB_FG, p[*inum]);
        oldinum = *inum;
    }
}

void StatusBinIcon::init(Vec2i posToUse, Patch* i, bool* valToUse, bool* onToUse)
{
    pos = posToUse;
    oldval = 0;
    val = valToUse;
    on = onToUse;
    p = i;
}

void StatusBinIcon::update(bool refresh)
{
    if (*on && (oldval != *val || refresh))
    {
        int drawX = pos.x - littleEndian(p->leftoffset);
        int drawY = pos.y - littleEndian(p->topoffset);
        auto w = littleEndian(p->width);
        auto h = littleEndian(p->height);

        if (drawY - ST_Y < 0)
            fatalError("Error: StatusBinIcon::update: y - ST_Y < 0");

        if (*val)
            drawPatch(pos, STLIB_FG, p);
        else
            copyRect(
                {drawX, drawY - ST_Y}, STLIB_BG, {w, h}, {drawX, drawY}, STLIB_FG);

        oldval = *val;
    }
}

} // namespace Doom
