// Rewritten out of vanilla hu_lib into namespace Doom.
//
// Heads-up text widgets: a single text line, a scrolling multi-line message list,
// and an editable input line. All state lives in the caller's structs, so this
// unit holds no globals of its own. The vanilla HUlib_ names that used to shim
// these have been retired; callers use the Doom:: names directly. Covered by
// the frame goldens (messages and the level name land in screens[0]).

#include "../doom_config.h"

#include "../doomdef.h"
#include "HudWidgetTypes.h"
#include "../m_swap.h"

#include "HudWidgets.h"

#include "../Render/Draw.h"
#include "../Game/OverlayState.h"
#include "../Render/Video.h"
#include "../Render/ViewWindow.h"

namespace Doom
{

void initWidgets() {}

void clearTextLine(HudTextLine& t)
{
    t.len = 0;
    t.l[0] = 0;
    t.needsupdate = true;
}

void initTextLine(HudTextLine& t, int x, int y, Patch** f, int sc)
{
    t.x = x;
    t.y = y;
    t.f = f;
    t.sc = sc;
    clearTextLine(t);
}

doom_boolean addCharToTextLine(HudTextLine& t, char ch)
{
    if (t.len == HU_MAXLINELENGTH)
        return false;
    else
    {
        t.l[t.len++] = ch;
        t.l[t.len] = 0;
        t.needsupdate = 4;
        return true;
    }
}

doom_boolean delCharFromTextLine(HudTextLine& t)
{
    if (!t.len)
        return false;
    else
    {
        t.l[--t.len] = 0;
        t.needsupdate = 4;
        return true;
    }
}

void drawTextLine(HudTextLine& l, doom_boolean drawcursor)
{
    int i;
    int w;
    int x;
    unsigned char c;

    // draw the new stuff
    x = l.x;
    for (i = 0; i < l.len; i++)
    {
        c = doom_toupper(l.l[i]);
        if (c != ' ' && c >= l.sc && c <= '_')
        {
            w = SHORT(l.f[c - l.sc]->width);
            if (x + w > SCREENWIDTH)
                break;
            Doom::drawPatchDirect(x, l.y, FG, l.f[c - l.sc]);
            x += w;
        }
        else
        {
            x += 4;
            if (x >= SCREENWIDTH)
                break;
        }
    }

    // draw the cursor if requested
    if (drawcursor && x + SHORT(l.f['_' - l.sc]->width) <= SCREENWIDTH)
    {
        Doom::drawPatchDirect(x, l.y, FG, l.f['_' - l.sc]);
    }
}

// sorta called by Doom::eraseHud and just better darn get things straight
void eraseTextLine(HudTextLine& l)
{
    auto& view = viewWindow();

    int lh;
    int y;
    int yoffset;

    // Only erases when NOT in automap and the screen is reduced,
    // and the text must either need updating or refreshing
    // (because of a recent change back from the automap)

    if (!overlayState().automapactive && view.viewwindowx && l.needsupdate)
    {
        lh = SHORT(l.f[0]->height) + 1;
        for (y = l.y, yoffset = y * SCREENWIDTH; y < l.y + lh;
             y++, yoffset += SCREENWIDTH)
        {
            if (y < view.viewwindowy || y >= view.viewwindowy + view.viewheight)
                Doom::videoErase(yoffset, SCREENWIDTH); // erase entire line
            else
            {
                Doom::videoErase(yoffset, view.viewwindowx); // erase left border
                Doom::videoErase(yoffset + view.viewwindowx + view.viewwidth,
                                 view.viewwindowx);
                // erase right border
            }
        }
    }

    if (l.needsupdate)
        l.needsupdate--;
}

void initSText(HudScrollingText& s,
               int x,
               int y,
               int h,
               Patch** font,
               int startchar,
               doom_boolean* on)
{
    int i;

    s.h = h;
    s.on = on;
    s.laston = true;
    s.cl = 0;
    for (i = 0; i < h; i++)
        initTextLine(
            s.l[i], x, y - i * (SHORT(font[0]->height) + 1), font, startchar);
}

void addLineToSText(HudScrollingText& s)
{
    int i;

    // add a clear line
    if (++s.cl == s.h)
        s.cl = 0;
    clearTextLine(s.l[s.cl]);

    // everything needs updating
    for (i = 0; i < s.h; i++)
        s.l[i].needsupdate = 4;
}

void addMessageToSText(HudScrollingText& s, const char* prefix, const char* msg)
{
    addLineToSText(s);
    if (prefix)
        while (*prefix)
            addCharToTextLine(s.l[s.cl], *(prefix++));

    while (*msg)
        addCharToTextLine(s.l[s.cl], *(msg++));
}

void drawSText(HudScrollingText& s)
{
    int i, idx;

    if (!*s.on)
        return; // if not on, don't draw

    // draw everything
    for (i = 0; i < s.h; i++)
    {
        idx = s.cl - i;
        if (idx < 0)
            idx += s.h; // handle queue of lines

        // need a decision made here on whether to skip the draw
        drawTextLine(s.l[idx], false); // no cursor, please
    }
}

void eraseSText(HudScrollingText& s)
{
    int i;

    for (i = 0; i < s.h; i++)
    {
        if (s.laston && !*s.on)
            s.l[i].needsupdate = 4;
        eraseTextLine(s.l[i]);
    }
    s.laston = *s.on;
}

void initIText(
    HudInputText& it, int x, int y, Patch** font, int startchar, doom_boolean* on)
{
    it.lm = 0; // default left margin is start of text
    it.on = on;
    it.laston = true;
    initTextLine(it.l, x, y, font, startchar);
}

// The following deletion routines adhere to the left margin restriction
void delCharFromIText(HudInputText& it)
{
    if (it.l.len != it.lm)
        delCharFromTextLine(it.l);
}

void eraseLineFromIText(HudInputText& it)
{
    while (it.lm != it.l.len)
        delCharFromTextLine(it.l);
}

// Resets left margin as well
void resetIText(HudInputText& it)
{
    it.lm = 0;
    clearTextLine(it.l);
}

void addPrefixToIText(HudInputText& it, char* str)
{
    while (*str)
        addCharToTextLine(it.l, *(str++));
    it.lm = it.l.len;
}

// wrapper function for handling general keyed input.
// returns true if it ate the key
doom_boolean keyInIText(HudInputText& it, unsigned char ch)
{
    if (ch >= ' ' && ch <= '_')
        addCharToTextLine(it.l, static_cast<char>(ch));
    else if (ch == KEY_BACKSPACE)
        delCharFromIText(it);
    else if (ch != KEY_ENTER)
        return false; // did not eat key

    return true; // ate the key
}

void drawIText(HudInputText& it)
{
    if (!*it.on)
        return;
    drawTextLine(it.l, true); // draw the line w/ cursor
}

void eraseIText(HudInputText& it)
{
    if (it.laston && !*it.on)
        it.l.needsupdate = 4;
    eraseTextLine(it.l);
    it.laston = *it.on;
}

} // namespace Doom
