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
// Config load/save, the raw file I/O and the screenshot writer. The defaults
// table (binding each config key to the Engine member it writes through) is
// file-local here and handed out by defaults(); the config paths are read
// straight off configPaths() (Game/ConfigPaths.h).
// loadDefaults reads the test config, so the frame goldens pin it.

#include "../Host/Platform.h"

#include "GameDefs.h"
#include "MapSpawns.h" // State.
#include "Strings.h" // Data.
#include "../UI/Hud.h"
#include "Args.h"
#include "ConfigTypes.h"
#include "../Math/Swap.h"
#include "../Wad/WadFile.h"

#include "Config.h"
#include "SoundSettings.h"
#include "../Engine/Engine.h"

#include "../Render/Video.h"
#include "Args.h"
#include "ConfigPaths.h"
#include "InputConfig.h"
#include "PlayerState.h"
#include "../Containers.h"

#include "../Host/Video.h"
#include "../Host/System.h"
#ifndef O_BINARY
#define O_BINARY 0
#endif

namespace Doom
{
//
// SCREEN SHOTS
//
struct PcxHeader
{
    char manufacturer = 0;
    char version = 0;
    char encoding = 0;
    char bits_per_pixel = 0;

    unsigned short xmin = 0;
    unsigned short ymin = 0;
    unsigned short xmax = 0;
    unsigned short ymax = 0;

    unsigned short hres = 0;
    unsigned short vres = 0;

    unsigned char palette[48] = {};

    char reserved = 0;
    char color_planes = 0;
    unsigned short bytes_per_line = 0;
    unsigned short palette_type = 0;

    char filler[58] = {};
    unsigned char data = 0; // unbounded
};

//
// DEFAULTS
//
// Every numeric default writes through an Engine member (InputConfig,
// MenuSettings, SoundSettings), so the table below carries the name and the
// value and bindEngineDefaults() supplies the address at runtime.

static ConfigDefault intDefault(int value)
{
    return {.defaultvalue = value};
}

static ConfigDefault textDefault(std::string_view* location, std::string_view value)
{
    return {.defaultvalue = STRING_VALUE,
            .text_location = location,
            .default_text_value = std::string {value}};
}

ConfigDefaults& defaults()
{
    // Built on first use rather than at static-init: the chatmacro entries take the
    // address of a chat_macros() element, and every location is bound to its Engine
    // member later still (bindEngineDefaults).
    static auto table = ConfigDefaults {
        {"mouse_sensitivity", intDefault(5)},
        {"sfx_volume", intDefault(8)},
        {"music_volume", intDefault(8)},
        {"show_messages", intDefault(1)},

        {"key_right", intDefault(KEY_RIGHTARROW)},
        {"key_left", intDefault(KEY_LEFTARROW)},
        {"key_up", intDefault(KEY_UPARROW)},
        {"key_down", intDefault(KEY_DOWNARROW)},
        {"key_strafeleft", intDefault(',')},
        {"key_straferight", intDefault('.')},

        {"key_fire", intDefault(KEY_RCTRL)},
        {"key_use", intDefault(' ')},
        {"key_strafe", intDefault(KEY_RALT)},
        {"key_speed", intDefault(KEY_RSHIFT)},

        {"use_mouse", intDefault(1)},
        {"mouseb_fire", intDefault(0)},
        {"mouseb_strafe", intDefault(1)},
        {"mouseb_forward", intDefault(2)},
        {"mouse_move", intDefault(0)},

        {"use_joystick", intDefault(0)},
        {"joyb_fire", intDefault(0)},
        {"joyb_strafe", intDefault(1)},
        {"joyb_use", intDefault(3)},
        {"joyb_speed", intDefault(2)},

        {"screenblocks", intDefault(9)},
        {"detaillevel", intDefault(0)},
        {"crosshair", intDefault(0)},
        {"always_run", intDefault(0)},

        {"snd_channels", intDefault(3)},
        {"usegamma", intDefault(0)},

        {"chatmacro0", textDefault(&chat_macros()[0], HUSTR_CHATMACRO0)},
        {"chatmacro1", textDefault(&chat_macros()[1], HUSTR_CHATMACRO1)},
        {"chatmacro2", textDefault(&chat_macros()[2], HUSTR_CHATMACRO2)},
        {"chatmacro3", textDefault(&chat_macros()[3], HUSTR_CHATMACRO3)},
        {"chatmacro4", textDefault(&chat_macros()[4], HUSTR_CHATMACRO4)},
        {"chatmacro5", textDefault(&chat_macros()[5], HUSTR_CHATMACRO5)},
        {"chatmacro6", textDefault(&chat_macros()[6], HUSTR_CHATMACRO6)},
        {"chatmacro7", textDefault(&chat_macros()[7], HUSTR_CHATMACRO7)},
        {"chatmacro8", textDefault(&chat_macros()[8], HUSTR_CHATMACRO8)},
        {"chatmacro9", textDefault(&chat_macros()[9], HUSTR_CHATMACRO9)}};

    return table;
}

ConfigDefault* findDefault(std::string_view name)
{
    auto found = defaults().find(name);
    return found == defaults().end() ? nullptr : &found->second;
}

//
// writeFile
//
bool writeFile(std::string_view name, void* source, int length)
{
    void* handle;
    int count;

    handle = host().open(name, "wb");

    if (handle == nullptr)
        return false;

    count = host().write(handle, source, length);
    host().close(handle);

    if (count < length)
        return false;

    return true;
}

//
// readFile
//
int readFile(std::string_view name, Vector<byte>& buffer)
{
    auto* handle = host().open(name, "rb");
    if (handle == nullptr)
    {
        fatalError("Error: Couldn't read file ", name);
    }
    host().seek(handle, 0, SeekOrigin::End);
    auto length = host().tell(handle);
    host().seek(handle, 0, SeekOrigin::Set);

    // resize zeroes where the old doom_malloc did not, which is unobservable: the
    // doom_read below fills all `length` bytes or the read is fatal.
    buffer.resize(length);
    auto count = host().read(handle, buffer.data(), length);
    host().close(handle);

    if (count < length)
    {
        fatalError("Error: Couldn't read file ", name);
    }

    return length;
}

// Point each numeric default at the Engine member it writes through. Done at
// runtime rather than by capturing &member in the table above, because those
// members are reached through references bound at dynamic-init time: a static
// &member would race that binding across translation units (it segfaulted every
// test when tried). Idempotent, so both loadDefaults and saveDefaults call it
// before touching a location pointer.
static void bindEngineDefaults()
{
    auto& e = engine();

    auto bind = [](std::string_view name, int* location)
    {
        if (auto* def = findDefault(name))
            def->location = location;
    };

    bind("sfx_volume", &e.soundSettings.sfxVolume);
    bind("music_volume", &e.soundSettings.musicVolume);
    bind("snd_channels", &e.soundSettings.numChannels);
    bind("mouse_sensitivity", &e.menuSettings.mouseSensitivity);
    bind("show_messages", &e.menuSettings.showMessages);
    bind("screenblocks", &e.menuSettings.screenblocks);
    bind("detaillevel", &e.menuSettings.detailLevel);
    bind("usegamma", &e.menuSettings.usegamma);
    bind("key_right", &e.inputConfig.key_right);
    bind("key_left", &e.inputConfig.key_left);
    bind("key_up", &e.inputConfig.key_up);
    bind("key_down", &e.inputConfig.key_down);
    bind("key_strafeleft", &e.inputConfig.key_strafeleft);
    bind("key_straferight", &e.inputConfig.key_straferight);
    bind("key_fire", &e.inputConfig.key_fire);
    bind("key_use", &e.inputConfig.key_use);
    bind("key_strafe", &e.inputConfig.key_strafe);
    bind("key_speed", &e.inputConfig.key_speed);
    bind("use_mouse", &e.inputConfig.usemouse);
    bind("mouseb_fire", &e.inputConfig.mousebfire);
    bind("mouseb_strafe", &e.inputConfig.mousebstrafe);
    bind("mouseb_forward", &e.inputConfig.mousebforward);
    bind("mouse_move", &e.inputConfig.mousemove);
    bind("use_joystick", &e.inputConfig.usejoystick);
    bind("joyb_fire", &e.inputConfig.joybfire);
    bind("joyb_strafe", &e.inputConfig.joybstrafe);
    bind("joyb_use", &e.inputConfig.joybuse);
    bind("joyb_speed", &e.inputConfig.joybspeed);
    bind("crosshair", &e.inputConfig.crosshair);
    bind("always_run", &e.inputConfig.always_run);
}

//
// saveDefaults
//
void saveDefaults()
{
    bindEngineDefaults();

    auto* f = host().open(configPaths().defaultfile.c_str(), "w");
    if (!f)
        return; // can't write the file, but don't complain

    for (const auto& [name, def]: defaults())
    {
        if (def.defaultvalue > -0xfff && def.defaultvalue < 0xfff)
            printTo(f, name, "\t\t", *def.location, "\n");
        else
            printTo(f, name, "\t\t\"", *def.text_location, "\"\n");
    }

    host().close(f);
}

//
// loadDefaults
//
void loadDefaults()
{
    int i;
    void* f;
    auto key = std::string {};
    auto strparm = std::string {};
    bool isstring;
    auto parm = 0;

    auto& paths = configPaths();

    bindEngineDefaults();

    // set everything to base values
    for (auto& entry: defaults())
        entry.second.resetToDefault();

    // check for a custom default file
    i = checkParm("-config");
    if (i && i < myargCount() - 1)
    {
        paths.defaultfile = myargv()[i + 1];
        //doom_print("        default file: %s\n", defaultfile);
        print("        default file: ", paths.defaultfile, "\n");
    }
    else
        paths.defaultfile = paths.basedefault;

    // read the file in, overriding any set defaults
    f = host().open(paths.defaultfile.c_str(), "r");
    if (f)
    {
        while (!host().eof(f))
        {
            // the key
            auto arg_read = 0;
            char c;
            key.clear();
            for (i = 0; i < 79; ++i)
            {
                host().read(f, &c, 1);
                if (c == ' ' || c == '\n' || c == '\t')
                {
                    if (i > 0)
                        arg_read++;
                    break;
                }
                key += c;
            }

            // Ignore spaces
            strparm.clear();
            if (c != '\n')
            {
                while (1)
                {
                    host().read(f, &c, 1);
                    if (c != ' ' && c != '\t')
                        break;
                }

                // strparam
                if (c != '\n')
                {
                    while (static_cast<int>(strparm.size()) < 260)
                    {
                        strparm += c;
                        host().read(f, &c, 1);
                        if (c == '\n')
                        {
                            if (!strparm.empty())
                                arg_read++;
                            break;
                        }
                    }
                }
            }

            isstring = false;
            //if (fscanf(f, "%79s %[^\n]\n", def, strparm) == 2)
            if (arg_read == 2)
            {
                if (strparm[0] == '"')
                {
                    // get a string default: strip the closing quote
                    isstring = true;
                    strparm.pop_back();
                }
                else if (strparm[0] == '0' && strparm[1] == 'x')
                {
                    //sscanf(strparm + 2, "%x", &parm);
                    parm = parseHex(std::string_view {strparm}.substr(2));
                }
                else
                {
                    //sscanf(strparm, "%i", &parm);
                    parm = parseInt(strparm);
                }
                if (auto* def = findDefault(key))
                {
                    if (isstring)
                        def->setText(std::string_view {strparm}.substr(1));
                    else
                        *def->location = parm;
                }
            }
        }

        host().close(f);
    }
}

//
// WritePCXfile
//
void WritePCXfile(
    std::string_view filename, byte* data, int width, int height, byte* palette)
{
    int length;
    byte* pack;

    // RAII scratch: the PCX header + packed image is built into this buffer and
    // written out, then released on return. pcx is a view onto it.
    auto pcxbuf = Vector<byte>(width * height * 2 + 1000);
    auto* pcx = reinterpret_cast<PcxHeader*>(pcxbuf.data());

    pcx->manufacturer = 0x0a; // PCX id
    pcx->version = 5; // 256 color
    pcx->encoding = 1; // uncompressed
    pcx->bits_per_pixel = 8; // 256 color
    pcx->xmin = 0;
    pcx->ymin = 0;
    pcx->xmax = littleEndian<unsigned short>(width - 1);
    pcx->ymax = littleEndian<unsigned short>(height - 1);
    pcx->hres = littleEndian<unsigned short>(width);
    pcx->vres = littleEndian<unsigned short>(height);
    doom_memset(pcx->palette, 0, sizeof(pcx->palette));
    pcx->color_planes = 1; // chunky image
    pcx->bytes_per_line = littleEndian<unsigned short>(width);
    pcx->palette_type = littleEndian<unsigned short>(2); // not a grey scale
    doom_memset(pcx->filler, 0, sizeof(pcx->filler));

    // pack the image
    pack = &pcx->data;

    for (auto i = 0; i < width * height; i++)
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
    for (auto i = 0; i < 768; i++)
        *pack++ = *palette++;

    // write output file
    length = static_cast<int>(pack - reinterpret_cast<byte*>(pcx));
    writeFile(filename, pcx, length);
}

//
// writeScreenshot
//
void writeScreenshot()
{
    int i;
    byte* linear;
    void* f;

    // munge planar buffer to linear
    linear = videoState().screens[2];
    readScreen(linear);

    // find a file name to save it to
    auto lbmname = std::string {"DOOM00.pcx"};

    for (i = 0; i <= 99; i++)
    {
        lbmname[4] = static_cast<char>(i / 10 + '0');
        lbmname[5] = static_cast<char>(i % 10 + '0');
        if ((f = host().open(lbmname, "wb")) == nullptr)
            break; // file doesn't exist
        host().close(f);
    }
    if (i == 100)
        fatalError("Error: writeScreenshot: Couldn't create a PCX");

    // save the pcx file
    WritePCXfile(lbmname,
                 linear,
                 SCREENWIDTH,
                 SCREENHEIGHT,
                 static_cast<byte*>((cacheLumpName("PLAYPAL"))));

    auto& state = playerState();
    state.players[state.consoleplayer].message = "screen shot";
}

} // namespace Doom
