// Rewritten out of vanilla hu_lib into namespace Doom.
//
// Heads-up text widgets: a single text line, a scrolling multi-line message list,
// and an editable input line. All state lives in the caller's structs, so this
// unit holds no globals of its own. The vanilla HUlib_ names that used to shim
// these have been retired; callers use the Doom:: names directly. Covered by
// the frame goldens (messages and the level name land in screens[0]).

#include "../Host/Platform.h"
#include "../Host/Text.h"

#include "../Game/GameDefs.h"
#include "HudWidgetTypes.h"
#include "../Math/Swap.h"

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
    t.l.clear();
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

bool addCharToTextLine(HudTextLine& t, char ch)
{
    if (static_cast<int>(t.l.size()) == HU_MAXLINELENGTH)
        return false;
    else
    {
        t.l.push_back(ch);
        t.needsupdate = 4;
        return true;
    }
}

bool delCharFromTextLine(HudTextLine& t)
{
    if (t.l.empty())
        return false;
    else
    {
        t.l.pop_back();
        t.needsupdate = 4;
        return true;
    }
}

void drawTextLine(HudTextLine& l, bool drawcursor)
{
    // draw the new stuff
    auto x = l.x;

    for (int i = 0; i < static_cast<int>(l.l.size()); i++)
    {
        auto c = static_cast<unsigned char>(toUpper(l.l[i]));
        if (c != ' ' && c >= l.sc && c <= '_')
        {
            int w = littleEndian(l.f[c - l.sc]->width);
            if (x + w > SCREENWIDTH)
                break;
            drawPatchDirect(x, l.y, FG, l.f[c - l.sc]);
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
    if (drawcursor && x + littleEndian(l.f['_' - l.sc]->width) <= SCREENWIDTH)
    {
        drawPatchDirect(x, l.y, FG, l.f['_' - l.sc]);
    }
}

// sorta called by Doom::eraseHud and just better darn get things straight
void eraseTextLine(HudTextLine& l)
{
    auto& view = viewWindow();

    int y;
    int yoffset;

    // Only erases when NOT in automap and the screen is reduced,
    // and the text must either need updating or refreshing
    // (because of a recent change back from the automap)

    if (!overlayState().automapactive && view.viewwindowx && l.needsupdate)
    {
        int lh = littleEndian(l.f[0]->height) + 1;
        for (y = l.y, yoffset = y * SCREENWIDTH; y < l.y + lh;
             y++, yoffset += SCREENWIDTH)
        {
            if (y < view.viewwindowy || y >= view.viewwindowy + view.viewheight)
                videoErase(yoffset, SCREENWIDTH); // erase entire line
            else
            {
                videoErase(yoffset, view.viewwindowx); // erase left border
                videoErase(yoffset + view.viewwindowx + view.viewwidth,
                           view.viewwindowx);
                // erase right border
            }
        }
    }

    if (l.needsupdate)
        l.needsupdate--;
}

void initSText(
    HudScrollingText& s, int x, int y, int h, Patch** font, int startchar, bool* on)
{
    s.h = h;
    s.on = on;
    s.laston = true;
    s.cl = 0;
    for (int i = 0; i < h; i++)
        initTextLine(
            s.l[i], x, y - i * (littleEndian(font[0]->height) + 1), font, startchar);
}

void addLineToSText(HudScrollingText& s)
{
    // add a clear line
    if (++s.cl == s.h)
        s.cl = 0;
    clearTextLine(s.l[s.cl]);

    // everything needs updating
    for (int i = 0; i < s.h; i++)
        s.l[i].needsupdate = 4;
}

void addMessageToSText(HudScrollingText& s,
                       std::string_view prefix,
                       std::string_view msg)
{
    addLineToSText(s);

    for (auto character: prefix)
        addCharToTextLine(s.l[s.cl], character);

    for (auto character: msg)
        addCharToTextLine(s.l[s.cl], character);
}

void drawSText(HudScrollingText& s)
{
    if (!*s.on)
        return; // if not on, don't draw

    // draw everything
    for (int i = 0; i < s.h; i++)
    {
        int idx = s.cl - i;
        if (idx < 0)
            idx += s.h; // handle queue of lines

        // need a decision made here on whether to skip the draw
        drawTextLine(s.l[idx], false); // no cursor, please
    }
}

void eraseSText(HudScrollingText& s)
{
    for (int i = 0; i < s.h; i++)
    {
        if (s.laston && !*s.on)
            s.l[i].needsupdate = 4;
        eraseTextLine(s.l[i]);
    }
    s.laston = *s.on;
}

void initIText(HudInputText& it, int x, int y, Patch** font, int startchar, bool* on)
{
    it.lm = 0; // default left margin is start of text
    it.on = on;
    it.laston = true;
    initTextLine(it.l, x, y, font, startchar);
}

// The following deletion routines adhere to the left margin restriction
void delCharFromIText(HudInputText& it)
{
    if (static_cast<int>(it.l.l.size()) != it.lm)
        delCharFromTextLine(it.l);
}

void eraseLineFromIText(HudInputText& it)
{
    while (it.lm != static_cast<int>(it.l.l.size()))
        delCharFromTextLine(it.l);
}

// Resets left margin as well
void resetIText(HudInputText& it)
{
    it.lm = 0;
    clearTextLine(it.l);
}

void addPrefixToIText(HudInputText& it, std::string_view str)
{
    for (auto character: str)
        addCharToTextLine(it.l, character);

    it.lm = static_cast<int>(it.l.l.size());
}

// wrapper function for handling general keyed input.
// returns true if it ate the key
bool keyInIText(HudInputText& it, unsigned char ch)
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
