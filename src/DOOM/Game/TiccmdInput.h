#pragma once

namespace Doom
{
// The raw input state a tic's command is built from. Doom::gameResponder accumulates it as events
// arrive - which keys are held (gamekeydown), the mouse and joystick buttons, the mouse and
// joystick movement since the last tic - and Doom::buildTiccmd reads it once a tic to produce the
// ticcmd, zeroing the mouse deltas as it consumes them. turnheld counts how long a turn key
// has been held, for accelerative turning; the dclick* fields detect the double-clicks that
// stand in for use/strafe on a two-button mouse.
//
// All g_game's own file-scope state (no header extern, read by no other file - Game/Game.cpp
// alone), so this is a file-scope-statics migration, not a loose-global one: the vanilla names
// stay in Game.cpp as references onto these members (REFACTOR.md, Step 5, the file-scope-statics
// sweep). None is config-backed - the key/mouse/joy *bindings* (key_*, mouseb*, joyb*) are, and
// stay loose globals until the config rework; this is the input *state* they drive, not the
// bindings. The interior view pointers mousebuttons/joybuttons (= &mousearray[1], to allow a
// [-1] index) stay in Game.cpp, pointing into the mousearray/joyarray references.
//
// mousex/mousey reach the simulation through the ticcmd, but a demo overrides the built command
// with G_ReadDemoTiccmd, and the headless suite feeds no mouse, so they are zero throughout the
// replay - the move is golden-neutral either way (a reference reads the identical value).
struct TiccmdInput
{
    // NUMKEYS: gamekeydown is indexed by key code
    static constexpr int keyCount = 256;

    bool gamekeydown[keyCount] = {}; // which game keys are currently held
    int turnheld = 0; // tics a turn key has been held, for accelerative turning

    bool mousearray[4] = {}; // mouse button state (mousebuttons views &[1])

    // mouse movement accumulated since the last ticcmd (Doom::gameResponder assigns, Doom::buildTiccmd
    // consumes and zeroes once a tic)
    int mousex = 0;
    int mousey = 0;

    // double-click detection: primary (fire) and secondary (strafe/use)
    int dclicktime = 0;
    int dclickstate = 0;
    int dclicks = 0;
    int dclicktime2 = 0;
    int dclickstate2 = 0;
    int dclicks2 = 0;

    int joyxmove = 0; // joystick movement, repeated each tic
    int joyymove = 0;
    bool joyarray[5] = {}; // joystick button state (joybuttons views &[1])
};

// The one TiccmdInput, a view onto the Engine's member - the same pattern as the other Game/
// clusters (gameClock(), corpseQueue(), ...).
TiccmdInput& ticcmdInput();
} // namespace Doom
