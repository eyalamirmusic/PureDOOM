// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
//
// $Log:$
//
// DESCRIPTION:
//        Main loop menu stuff.
//        Default Config File.
//        PCX Screenshots.
//
//-----------------------------------------------------------------------------

// Rewritten out of vanilla m_misc into namespace Doom.
//
// Config load/save, the raw file I/O, the screenshot writer and M_DrawText.
// m_misc.cpp shims the M_ names. The defaults[] table (binding config keys to
// the engine's option globals) stays at file scope here, above the namespace,
// alongside the externs the still-loose entries take addresses of; defaultfile
// is a reference onto its Engine member (Game/ConfigPaths.h). mLoadDefaults reads
// the test config, so the frame goldens pin it.

#include "../doom_config.h"

#include "../doomdef.h"
#include "../doomstat.h" // State.
#include "../dstrings.h" // Data.
#include "../hu_stuff.h"
#include "../i_system.h"
#include "../i_video.h"
#include "../m_argv.h"
#include "../m_misc.h"
#include "../m_swap.h"
#include "../v_video.h"
#include "../w_wad.h"

#include "Config.h"
#include "SoundSettings.h"
#include "../Engine/Engine.h"

#include <ea_data_structures/Structures/Vector.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

//
// SCREEN SHOTS
//
struct pcx_t
{
    char manufacturer;
    char version;
    char encoding;
    char bits_per_pixel;

    unsigned short xmin;
    unsigned short ymin;
    unsigned short xmax;
    unsigned short ymax;

    unsigned short hres;
    unsigned short vres;

    unsigned char palette[48];

    char reserved;
    char color_planes;
    unsigned short bytes_per_line;
    unsigned short palette_type;

    char filler[58];
    unsigned char data; // unbounded
};

//
// M_DrawText
// Returns the final X coordinate
// HU_Init must have been called to init the font
//
// hu_font is a Doom::HudFont member (Engine); a reference-to-array onto it (HU_Start writes it).
extern patch_t* (&hu_font)[HU_FONTSIZE];

//
// DEFAULTS
//
// The keyboard/mouse/joystick bindings are Doom::InputConfig members (Engine) now, so their
// defaults[] entries are bound to those members at runtime by bindEngineDefaults() rather than
// capturing their addresses here at static-init. Config.cpp no longer needs to name them.

extern int& viewwidth;
extern int& viewheight;

// mouseSensitivity / showMessages / detailLevel / screenblocks are Engine members now
// (UI/MenuSettings.h), reached through references (doomstat.h / their owner files). Like the
// sound params below, their defaults[] entries are bound to those members at runtime by
// bindEngineDefaults() rather than capturing their addresses here at static-init.

extern char* chat_macros[];

extern byte scantokey[128];

// usemouse/usejoystick/crosshair/always_run are Doom::InputConfig members (Engine); references.
int& usemouse = Doom::inputConfig().usemouse;
int& usejoystick = Doom::inputConfig().usejoystick;
int& crosshair = Doom::inputConfig().crosshair;
int& always_run = Doom::inputConfig().always_run;

// The config keys are string literals bound to option globals by address;
// the -Wwritable-strings that legitimate 1993 table raises is benign here.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwritable-strings"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
default_t defaults[] = {
    // These config-backed globals are Engine members reached through references, so their
    // location is bound to the member at runtime (bindEngineDefaults) rather than captured
    // here: a static &member would take the address of a reference before the Engine exists
    // and race its binding across translation units (it segfaulted every test when tried).
    {"mouse_sensitivity", 0, 5},
    {"sfx_volume", 0, 8},
    {"music_volume", 0, 8},
    {"show_messages", 0, 1},

    // The control bindings are bound to their Doom::InputConfig members at runtime
    // (bindEngineDefaults); a static &member here would race that binding across TUs.
    {"key_right", 0, KEY_RIGHTARROW},
    {"key_left", 0, KEY_LEFTARROW},
    {"key_up", 0, KEY_UPARROW},
    {"key_down", 0, KEY_DOWNARROW},
    {"key_strafeleft", 0, ','},
    {"key_straferight", 0, '.'},

    {"key_fire", 0, KEY_RCTRL},
    {"key_use", 0, ' '},
    {"key_strafe", 0, KEY_RALT},
    {"key_speed", 0, KEY_RSHIFT},

    {"use_mouse", 0, 1},
    {"mouseb_fire", 0, 0},
    {"mouseb_strafe", 0, 1},
    {"mouseb_forward", 0, 2},
    {"mouse_move", 0, 0},

    {"use_joystick", 0, 0},
    {"joyb_fire", 0, 0},
    {"joyb_strafe", 0, 1},
    {"joyb_use", 0, 3},
    {"joyb_speed", 0, 2},

    {"screenblocks", 0, 9},
    {"detaillevel", 0, 0},
    {"crosshair", 0, 0},
    {"always_run", 0, 0},

    {"snd_channels", 0, 3}, // bound to soundSettings().numChannels at runtime

    {"usegamma", 0, 0}, // bound to menuSettings().usegamma at runtime

    {"chatmacro0", 0, STRING_VALUE, 0, 0, &chat_macros[0], HUSTR_CHATMACRO0},
    {"chatmacro1", 0, STRING_VALUE, 0, 0, &chat_macros[1], HUSTR_CHATMACRO1},
    {"chatmacro2", 0, STRING_VALUE, 0, 0, &chat_macros[2], HUSTR_CHATMACRO2},
    {"chatmacro3", 0, STRING_VALUE, 0, 0, &chat_macros[3], HUSTR_CHATMACRO3},
    {"chatmacro4", 0, STRING_VALUE, 0, 0, &chat_macros[4], HUSTR_CHATMACRO4},
    {"chatmacro5", 0, STRING_VALUE, 0, 0, &chat_macros[5], HUSTR_CHATMACRO5},
    {"chatmacro6", 0, STRING_VALUE, 0, 0, &chat_macros[6], HUSTR_CHATMACRO6},
    {"chatmacro7", 0, STRING_VALUE, 0, 0, &chat_macros[7], HUSTR_CHATMACRO7},
    {"chatmacro8", 0, STRING_VALUE, 0, 0, &chat_macros[8], HUSTR_CHATMACRO8},
    {"chatmacro9", 0, STRING_VALUE, 0, 0, &chat_macros[9], HUSTR_CHATMACRO9}};

#pragma GCC diagnostic pop
int numdefaults = sizeof(defaults) / sizeof(default_t);
// defaultfile is an Engine member now (Game/ConfigPaths.h); reference onto it.
char*& defaultfile = Doom::configPaths().defaultfile;

namespace Doom
{

int mDrawText(int x, int y, doom_boolean direct, char* string)
{
    int c;
    int w;

    while (*string)
    {
        c = doom_toupper(*string) - HU_FONTSTART;
        string++;
        if (c < 0 || c > HU_FONTSIZE)
        {
            x += 4;
            continue;
        }

        w = SHORT(hu_font[c]->width);
        if (x + w > SCREENWIDTH)
            break;
        if (direct)
            V_DrawPatchDirect(x, y, 0, hu_font[c]);
        else
            V_DrawPatch(x, y, 0, hu_font[c]);
        x += w;
    }

    return x;
}

//
// mWriteFile
//
doom_boolean mWriteFile(char const* name, void* source, int length)
{
    void* handle;
    int count;

    handle = doom_open(name, "wb");

    if (handle == nullptr)
        return false;

    count = doom_write(handle, source, length);
    doom_close(handle);

    if (count < length)
        return false;

    return true;
}

//
// mReadFile
//
int mReadFile(char const* name, byte** buffer)
{
    void* handle;
    int count, length;
    byte* buf;

    handle = doom_open(name, "rb");
    if (handle == nullptr)
    {
        //I_Error("Error: Couldn't read file %s", name);

        doom_strcpy(error_buf, "Error: Couldn't read file ");
        doom_concat(error_buf, name);
        I_Error(error_buf);
    }
    doom_seek(handle, 0, DOOM_SEEK_END);
    length = doom_tell(handle);
    doom_seek(handle, 0, DOOM_SEEK_SET);
    buf = static_cast<byte*>((doom_malloc(length)));
    count = doom_read(handle, buf, length);
    doom_close(handle);

    if (count < length)
    {
        //I_Error("Error: Couldn't read file %s", name);

        doom_strcpy(error_buf, "Error: Couldn't read file ");
        doom_concat(error_buf, name);
        I_Error(error_buf);
    }

    *buffer = buf;
    return length;
}

// Point the defaults[] entries for the config-backed globals that now live on the
// Engine at their members. Done at runtime rather than by capturing &member in the
// static table initializer, because those members are reached through references
// bound at dynamic-init time: a static &member would race that binding across
// translation units (it segfaulted every test when tried). Idempotent, so both
// mLoadDefaults and mSaveDefaults call it before touching a location pointer.
static void bindEngineDefault(const char* name, int* location)
{
    for (int i = 0; i < numdefaults; i++)
        if (!doom_strcmp(defaults[i].name, name))
        {
            defaults[i].location = location;
            return;
        }
}

static void bindEngineDefaults()
{
    Engine& e = engine();

    // The member addresses are taken here, at runtime, not in the static defaults[] table -
    // that is the whole point (a static &member captures the address of a reference before the
    // Engine exists). A local table keeps the pairing readable.
    const struct
    {
        const char* name;
        int* location;
    } binds[] = {
        {"sfx_volume", &e.soundSettings.sfxVolume},
        {"music_volume", &e.soundSettings.musicVolume},
        {"snd_channels", &e.soundSettings.numChannels},
        {"mouse_sensitivity", &e.menuSettings.mouseSensitivity},
        {"show_messages", &e.menuSettings.showMessages},
        {"screenblocks", &e.menuSettings.screenblocks},
        {"detaillevel", &e.menuSettings.detailLevel},
        {"usegamma", &e.menuSettings.usegamma},
        {"key_right", &e.inputConfig.key_right},
        {"key_left", &e.inputConfig.key_left},
        {"key_up", &e.inputConfig.key_up},
        {"key_down", &e.inputConfig.key_down},
        {"key_strafeleft", &e.inputConfig.key_strafeleft},
        {"key_straferight", &e.inputConfig.key_straferight},
        {"key_fire", &e.inputConfig.key_fire},
        {"key_use", &e.inputConfig.key_use},
        {"key_strafe", &e.inputConfig.key_strafe},
        {"key_speed", &e.inputConfig.key_speed},
        {"use_mouse", &e.inputConfig.usemouse},
        {"mouseb_fire", &e.inputConfig.mousebfire},
        {"mouseb_strafe", &e.inputConfig.mousebstrafe},
        {"mouseb_forward", &e.inputConfig.mousebforward},
        {"mouse_move", &e.inputConfig.mousemove},
        {"use_joystick", &e.inputConfig.usejoystick},
        {"joyb_fire", &e.inputConfig.joybfire},
        {"joyb_strafe", &e.inputConfig.joybstrafe},
        {"joyb_use", &e.inputConfig.joybuse},
        {"joyb_speed", &e.inputConfig.joybspeed},
        {"crosshair", &e.inputConfig.crosshair},
        {"always_run", &e.inputConfig.always_run},
    };

    for (const auto& b: binds)
        bindEngineDefault(b.name, b.location);
}

//
// mSaveDefaults
//
void mSaveDefaults()
{
    int v;
    void* f;

    bindEngineDefaults();

    f = doom_open(defaultfile, "w");
    if (!f)
        return; // can't write the file, but don't complain

    for (int i = 0; i < numdefaults; i++)
    {
        if (defaults[i].defaultvalue > -0xfff && defaults[i].defaultvalue < 0xfff)
        {
            v = *defaults[i].location;
            //fprintf(f, "%s\t\t%i\n", defaults[i].name, v);
            doom_fprint(f, defaults[i].name);
            doom_fprint(f, "\t\t");
            doom_fprint(f, doom_itoa(v, 10));
            doom_fprint(f, "\n");
        }
        else
        {
            //fprintf(f, "%s\t\t\"%s\"\n", defaults[i].name,
            //        *(char**)(defaults[i].text_location));
            doom_fprint(f, defaults[i].name);
            doom_fprint(f, "\t\t\"");
            doom_fprint(f, *(char**) (defaults[i].text_location));
            doom_fprint(f, "\"\n");
        }
    }

    doom_close(f);
}

//
// mLoadDefaults
//
void mLoadDefaults()
{
    int i;
    int len;
    void* f;
    char def[80];
    char strparm[100];
    char* newstring;
    int parm;
    doom_boolean isstring;

    bindEngineDefaults();

    // set everything to base values
    // numdefaults = sizeof(defaults)/sizeof(defaults[0]);
    for (i = 0; i < numdefaults; i++)
    {
        if (defaults[i].defaultvalue == 0xFFFF)
            *defaults[i].text_location = defaults[i].default_text_value;
        else
            *defaults[i].location = static_cast<int>(defaults[i].defaultvalue);
    }

    // check for a custom default file
    i = M_CheckParm("-config");
    if (i && i < myargc - 1)
    {
        defaultfile = myargv[i + 1];
        //doom_print("        default file: %s\n", defaultfile);
        doom_print("        default file: ");
        doom_print(defaultfile);
        doom_print("\n");
    }
    else
        defaultfile = basedefault;

    // read the file in, overriding any set defaults
    f = doom_open(defaultfile, "r");
    if (f)
    {
        while (!doom_eof(f))
        {
            // def
            int arg_read = 0;
            char c;
            for (i = 0; i < 79; ++i)
            {
                doom_read(f, &c, 1);
                if (c == ' ' || c == '\n' || c == '\t')
                {
                    if (i > 0)
                        arg_read++;
                    break;
                }
                def[i] = c;
            }
            def[i] = '\0';

            // Ignore spaces
            if (c != '\n')
            {
                while (1)
                {
                    doom_read(f, &c, 1);
                    if (c != ' ' && c != '\t')
                        break;
                }

                // strparam
                i = 0;
                if (c != '\n')
                {
                    for (; i < 260;)
                    {
                        strparm[i++] = c;
                        doom_read(f, &c, 1);
                        if (c == '\n')
                        {
                            if (i > 0)
                                arg_read++;
                            break;
                        }
                    }
                }
                strparm[i] = '\0';
            }

            isstring = false;
            //if (fscanf(f, "%79s %[^\n]\n", def, strparm) == 2)
            if (arg_read == 2)
            {
                if (strparm[0] == '"')
                {
                    // get a string default
                    isstring = true;
                    len = static_cast<int>(doom_strlen(strparm));
                    newstring = static_cast<char*>(doom_malloc(len));
                    strparm[len - 1] = 0;
                    doom_strcpy(newstring, strparm + 1);
                }
                else if (strparm[0] == '0' && strparm[1] == 'x')
                {
                    //sscanf(strparm + 2, "%x", &parm);
                    parm = doom_atox(strparm + 2);
                }
                else
                {
                    //sscanf(strparm, "%i", &parm);
                    parm = doom_atoi(strparm);
                }
                for (i = 0; i < numdefaults; i++)
                    if (!doom_strcmp(def, defaults[i].name))
                    {
                        if (!isstring)
                            *defaults[i].location = parm;
                        else
                            *defaults[i].text_location = newstring;
                        break;
                    }
            }
        }

        doom_close(f);
    }
}

//
// WritePCXfile
//
void WritePCXfile(char* filename, byte* data, int width, int height, byte* palette)
{
    int length;
    byte* pack;

    // RAII scratch: the PCX header + packed image is built into this buffer and
    // written out, then released on return. pcx is a view onto it.
    auto pcxbuf = EA::Vector<byte>(width * height * 2 + 1000);
    auto* pcx = reinterpret_cast<pcx_t*>(pcxbuf.data());

    pcx->manufacturer = 0x0a; // PCX id
    pcx->version = 5; // 256 color
    pcx->encoding = 1; // uncompressed
    pcx->bits_per_pixel = 8; // 256 color
    pcx->xmin = 0;
    pcx->ymin = 0;
    pcx->xmax = SHORT(width - 1);
    pcx->ymax = SHORT(height - 1);
    pcx->hres = SHORT(width);
    pcx->vres = SHORT(height);
    doom_memset(pcx->palette, 0, sizeof(pcx->palette));
    pcx->color_planes = 1; // chunky image
    pcx->bytes_per_line = SHORT(width);
    pcx->palette_type = SHORT(2); // not a grey scale
    doom_memset(pcx->filler, 0, sizeof(pcx->filler));

    // pack the image
    pack = &pcx->data;

    for (int i = 0; i < width * height; i++)
    {
        if ((*data & 0xc0) != 0xc0)
            *pack++ = *data++;
        else
        {
            *pack++ = 0xc1;
            *pack++ = *data++;
        }
    }

    // write the palette
    *pack++ = 0x0c; // palette ID byte
    for (int i = 0; i < 768; i++)
        *pack++ = *palette++;

    // write output file
    length = static_cast<int>(pack - reinterpret_cast<byte*>(pcx));
    mWriteFile(filename, pcx, length);
}

//
// mScreenShot
//
void mScreenShot()
{
    int i;
    byte* linear;
    char lbmname[12];
    void* f;

    // munge planar buffer to linear
    linear = screens[2];
    I_ReadScreen(linear);

    // find a file name to save it to
    doom_strcpy(lbmname, "DOOM00.pcx");

    for (i = 0; i <= 99; i++)
    {
        lbmname[4] = i / 10 + '0';
        lbmname[5] = i % 10 + '0';
        if ((f = doom_open(lbmname, "wb")) == nullptr)
            break; // file doesn't exist
        doom_close(f);
    }
    if (i == 100)
        I_Error("Error: mScreenShot: Couldn't create a PCX");

    // save the pcx file
    WritePCXfile(lbmname,
                 linear,
                 SCREENWIDTH,
                 SCREENHEIGHT,
                 static_cast<byte*>((W_CacheLumpName("PLAYPAL", PU_CACHE))));

    players[consoleplayer].message = "screen shot";
}

} // namespace Doom
