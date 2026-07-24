// The public interface's implementation (DOOM.h): boot, the per-frame update,
// the framebuffer/sound/midi accessors and the input entry points, plus the
// doom_* reference bridge the engine's ~380 internal call sites go through.
// The platform hook defaults live in Host.cpp.

#include "../DOOM.h"

#include "Host.h" // Doom::host(), the owner of the 13 callbacks below

#include "Platform.h" // the 13 host references / helpers we define, for drift
#include "../Game/DoomMain.h"
#include "../Game/GameDefs.h"
#include "../doomtype.h"
#include "../Game/Args.h"
#include "../Game/ConfigTypes.h"

#include "../Game/OverlayState.h"
#include "../Render/ViewWindow.h"
#include "../Render/VideoState.h" // screens[] and screen_palette
#include "Sound.h"

#include "System.h"
#include "../Game/GameFlow.h"
#include "../Game/InputConfig.h"
#include "../Containers.h"

#include <map>
#include <string>

// The host output buffers, RAII-owned (Step 9): the 8-bit palette-index snapshot the
// embedder reads, and the RGB(A)-expanded frame. Sized once in initGame and returned
// to the embedder as raw pointers into data(), which is stable (never resized after).
static Doom::Vector<unsigned char> screen_buffer;
static Doom::Vector<unsigned char> final_screen_buffer;
static int last_update_time = 0;
static Doom::Array<int, 3> button_states = {0};

void doom_memset(void* ptr, int value, int num)
{
    unsigned char* p = (unsigned char*) (ptr);
    for (int i = 0; i < num; ++i, ++p)
    {
        *p = (unsigned char) value;
    }
}

void* doom_memcpy(void* destination, const void* source, int num)
{
    unsigned char* dst = (unsigned char*) (destination);
    const unsigned char* src = (const unsigned char*) (source);

    for (int i = 0; i < num; ++i, ++dst, ++src)
    {
        *dst = *src;
    }

    return destination;
}

namespace Doom
{
static ConfigDefault* getDefault(std::string_view name)
{
    for (int i = 0; i < numdefaults(); ++i)
    {
        if (name == defaults()[i].name)
            return &defaults()[i];
    }
    return nullptr;
}

void setResolution(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;
    // SCREENWIDTH = width;
    // SCREENHEIGHT = height;
}

void setDefaultInt(std::string_view name, int value)
{
    ConfigDefault* def = getDefault(name);
    if (!def)
        return;
    def->defaultvalue = value;
}

void setDefaultString(std::string_view name, std::string_view value)
{
    ConfigDefault* def = getDefault(name);
    if (!def)
        return;

    // The table holds a non-owning view read for the life of the process; own a
    // copy per name (map nodes never move, so the view stays valid). Assigning the
    // view after `owned` is written is what keeps it pointing at the live buffer if
    // the same name is set twice.
    static auto overrides = std::map<std::string, std::string, std::less<>> {};
    auto& owned = overrides[std::string {name}];
    owned = value;
    def->default_text_value = owned;
}

void initGame(int argc, char** argv, int flags)
{
    // Copied, not aliased: the host makes no promise about how long its argv
    // outlives this call, and findResponseFile may replace the whole vector.
    initGame(std::vector<std::string>(argv, argv + argc), flags);
}

void initGame(const std::vector<std::string>& args, int flags)
{
    screen_buffer.resize(SCREENWIDTH * SCREENHEIGHT);
    final_screen_buffer.resize(SCREENWIDTH * SCREENHEIGHT * 4);
    last_update_time = currentTic();

    myargv() = args;
    host().flags = flags;

    doomMain();
}

void updateGame()
{
    int now = currentTic();
    int delta_time = now - last_update_time;

    while (delta_time-- > 0)
    {
        if (gameFlow().is_wiping_screen)
            updateWipe();
        else
            doomLoop();
    }

    last_update_time = now;
}

void forceUpdateGame()
{
    if (gameFlow().is_wiping_screen)
        updateWipe();
    else
        doomLoop();
}

const unsigned char* framebuffer(int channels)
{
    doom_memcpy(
        screen_buffer.data(), videoState().screens[0], SCREENWIDTH * SCREENHEIGHT);

    // Draw inputConfig().crosshair
    if (inputConfig().crosshair && !overlayState().menuactive
        && gameFlow().gamestate == GameState::Level && !overlayState().automapactive)
    {
        int y;
        if (viewWindow().setblocks == 11)
            y = SCREENHEIGHT / 2 + 8;
        else
            y = SCREENHEIGHT / 2 - 8;
        for (int i = 0; i < 2; ++i)
        {
            screen_buffer[SCREENWIDTH / 2 - 2 - i + y * SCREENWIDTH] = 4;
            screen_buffer[SCREENWIDTH / 2 + 2 + i + y * SCREENWIDTH] = 4;
        }
        for (int i = 0; i < 2; ++i)
        {
            screen_buffer[SCREENWIDTH / 2 + (y - 2 - i) * SCREENWIDTH] = 4;
            screen_buffer[SCREENWIDTH / 2 + (y + 2 + i) * SCREENWIDTH] = 4;
        }
    }

    if (channels == 1)
    {
        return screen_buffer.data();
    }
    else if (channels == 3)
    {
        for (int i = 0, len = SCREENWIDTH * SCREENHEIGHT; i < len; ++i)
        {
            int k = i * 3;
            int kpal = screen_buffer[i] * 3;
            final_screen_buffer[k + 0] = videoState().screen_palette[kpal + 0];
            final_screen_buffer[k + 1] = videoState().screen_palette[kpal + 1];
            final_screen_buffer[k + 2] = videoState().screen_palette[kpal + 2];
        }
        return final_screen_buffer.data();
    }
    else if (channels == 4)
    {
        for (int i = 0, len = SCREENWIDTH * SCREENHEIGHT; i < len; ++i)
        {
            int k = i * 4;
            int kpal = screen_buffer[i] * 3;
            final_screen_buffer[k + 0] = videoState().screen_palette[kpal + 0];
            final_screen_buffer[k + 1] = videoState().screen_palette[kpal + 1];
            final_screen_buffer[k + 2] = videoState().screen_palette[kpal + 2];
            final_screen_buffer[k + 3] = 255;
        }
        return final_screen_buffer.data();
    }
    else
    {
        return nullptr;
    }
}

unsigned long tickMidi()
{
    return tickSong();
}

short* soundBuffer()
{
    updateSound();
    return mixbuffer();
}

void keyDown(Key key)
{
    Event event;
    event.type = EventType::KeyDown;
    event.data1 = static_cast<int>(key);
    postEvent(event);
}

void keyUp(Key key)
{
    Event event;
    event.type = EventType::KeyUp;
    event.data1 = static_cast<int>(key);
    postEvent(event);
}

void buttonDown(MouseButton button)
{
    button_states[toIndex(button)] = 1;

    Event event;
    event.type = EventType::Mouse;
    event.data1 =
        (button_states[0]) | (button_states[1] ? 2 : 0) | (button_states[2] ? 4 : 0);
    event.data2 = event.data3 = 0;
    postEvent(event);
}

void buttonUp(MouseButton button)
{
    button_states[toIndex(button)] = 0;

    Event event;
    event.type = EventType::Mouse;
    event.data1 =
        (button_states[0]) | (button_states[1] ? 2 : 0) | (button_states[2] ? 4 : 0);

    event.data1 = event.data1 ^ (button_states[0] ? 1 : 0)
                  ^ (button_states[1] ? 2 : 0) ^ (button_states[2] ? 4 : 0);

    event.data2 = event.data3 = 0;
    postEvent(event);
}

void mouseMove(int deltaX, int deltaY)
{
    Event event;

    event.type = EventType::Mouse;
    event.data1 =
        (button_states[0]) | (button_states[1] ? 2 : 0) | (button_states[2] ? 4 : 0);
    event.data2 = deltaX;
    event.data3 = -deltaY;

    if (event.data2 || event.data3)
    {
        postEvent(event);
    }
}
} // namespace Doom
