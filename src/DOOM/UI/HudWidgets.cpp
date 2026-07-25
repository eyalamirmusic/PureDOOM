// Rewritten out of vanilla hu_lib into namespace Doom.
//
// Heads-up text widgets: a single text line, a scrolling multi-line message list,
// and an editable input line. All state lives in the widgets themselves, so this
// unit holds no globals of its own - the vanilla HUlib_ free functions are methods
// on HudTextLine / HudScrollingText / HudInputText, declared in HudWidgetTypes.h.
// Covered by the frame goldens (messages and the level name land in screens[0]).

#include "../Host/Platform.h"
#include "../Host/Text.h"

#include "../Game/GameDefs.h"
#include "HudWidgetTypes.h"
#include "../Math/Swap.h"

#include "../Render/Draw.h"
#include "../Game/OverlayState.h"
#include "../Render/Video.h"
#include "../Render/ViewWindow.h"

namespace Doom
{

void HudTextLine::clear()
{
    l.clear();
    needsupdate = true;
}

void HudTextLine::init(int xToUse, int yToUse, Patch** fToUse, int scToUse)
{
    x = xToUse;
    y = yToUse;
    f = fToUse;
    sc = scToUse;
    clear();
}

bool HudTextLine::addChar(char ch)
{
    if (static_cast<int>(l.size()) == HU_MAXLINELENGTH)
        return false;

    l.push_back(ch);
    needsupdate = 4;
    return true;
}

bool HudTextLine::delChar()
{
    if (l.empty())
        return false;

    l.pop_back();
    needsupdate = 4;
    return true;
}

void HudTextLine::draw(bool drawcursor)
{
    // draw the new stuff
    auto drawX = x;

    for (char character: l)
    {
        auto c = static_cast<unsigned char>(toUpper(character));
        if (c != ' ' && c >= sc && c <= '_')
        {
            auto w = littleEndian(f[c - sc]->width);
            if (drawX + w > SCREENWIDTH)
                break;
            drawPatchDirect(drawX, y, FG, f[c - sc]);
            drawX += w;
        }
        else
        {
            drawX += 4;
            if (drawX >= SCREENWIDTH)
                break;
        }
    }

    // draw the cursor if requested
    if (drawcursor && drawX + littleEndian(f['_' - sc]->width) <= SCREENWIDTH)
    {
        drawPatchDirect(drawX, y, FG, f['_' - sc]);
    }
}

// sorta called by eraseHud and just better darn get things straight
void HudTextLine::erase()
{
    auto& view = viewWindow();

    // Only erases when NOT in automap and the screen is reduced,
    // and the text must either need updating or refreshing
    // (because of a recent change back from the automap)

    if (!overlayState().automapactive && view.viewwindowx && needsupdate)
    {
        auto lh = littleEndian(f[0]->height) + 1;
        for (auto row = y, yoffset = y * SCREENWIDTH; row < y + lh;
             row++, yoffset += SCREENWIDTH)
        {
            if (row < view.viewwindowy || row >= view.viewwindowy + view.viewheight)
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

    if (needsupdate)
        needsupdate--;
}

void HudScrollingText::init(
    int x, int y, int hToUse, Patch** font, int startchar, bool* onToUse)
{
    h = hToUse;
    on = onToUse;
    laston = true;
    cl = 0;
    for (auto i = 0; i < h; i++)
        l[i].init(x, y - i * (littleEndian(font[0]->height) + 1), font, startchar);
}

void HudScrollingText::addLine()
{
    // add a clear line
    if (++cl == h)
        cl = 0;
    l[cl].clear();

    // everything needs updating
    for (auto i = 0; i < h; i++)
        l[i].needsupdate = 4;
}

void HudScrollingText::addMessage(std::string_view prefix, std::string_view msg)
{
    addLine();

    for (auto character: prefix)
        l[cl].addChar(character);

    for (auto character: msg)
        l[cl].addChar(character);
}

void HudScrollingText::draw()
{
    if (!*on)
        return; // if not on, don't draw

    // draw everything
    for (auto i = 0; i < h; i++)
    {
        auto idx = cl - i;
        if (idx < 0)
            idx += h; // handle queue of lines

        // need a decision made here on whether to skip the draw
        l[idx].draw(false); // no cursor, please
    }
}

void HudScrollingText::erase()
{
    for (auto i = 0; i < h; i++)
    {
        if (laston && !*on)
            l[i].needsupdate = 4;
        l[i].erase();
    }
    laston = *on;
}

void HudInputText::init(int x, int y, Patch** font, int startchar, bool* onToUse)
{
    lm = 0; // default left margin is start of text
    on = onToUse;
    laston = true;
    l.init(x, y, font, startchar);
}

// The following deletion routines adhere to the left margin restriction
void HudInputText::delChar()
{
    if (static_cast<int>(l.l.size()) != lm)
        l.delChar();
}

// Resets left margin as well
void HudInputText::reset()
{
    lm = 0;
    l.clear();
}

// wrapper function for handling general keyed input.
// returns true if it ate the key
bool HudInputText::keyIn(unsigned char ch)
{
    if (ch >= ' ' && ch <= '_')
        l.addChar(static_cast<char>(ch));
    else if (ch == KEY_BACKSPACE)
        delChar();
    else if (ch != KEY_ENTER)
        return false; // did not eat key

    return true; // ate the key
}

void HudInputText::draw()
{
    if (!*on)
        return;
    l.draw(true); // draw the line w/ cursor
}

void HudInputText::erase()
{
    if (laston && !*on)
        l.needsupdate = 4;
    l.erase();
    laston = *on;
}

} // namespace Doom
