#pragma once

#include "StatusWidgetTypes.h" // StatusNumber, StatusPercent, StatusBinIcon, StatusMultIcon

namespace Doom
{
// The status bar's STlib widgets - the drawable elements of the bar. ST_createWidgets binds each
// to its data source (a player field) and its patches; ST_updateWidgets refreshes them and
// ST_drawWidgets paints them each tic. w_ready is the ready-weapon ammo count, w_ammo/w_maxammo the
// four ammo-type counts and their caps, w_health/w_armor the percentages, w_arms the six
// weapon-ownership icons over w_armsbg (shown only outside deathmatch), w_faces the animated face,
// w_keyboxes the three key slots, and w_frags the deathmatch frags summary that replaces the arms.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// UI/StatusBar's own file-local statics (internal linkage, read by no other file). They were reached
// through file-scope reference aliases (the arrays as references-to-array) until the file-local-alias
// sweep (REFACTOR.md, Step 9 strand (a)) retired them - updateWidgets, drawWidgets and createWidgets
// each hoist a single `auto& widgets = statusBarWidgets();` and reach every member through it,
// including their addresses (`&widgets.w_ready`, `&widgets.w_arms[i]`) taken by the STlib calls,
// which still resolve to the members. The bar is drawn into screens[0] every tic and the demos pick
// up ammo, take damage and get weapons, so this is live frame-golden-covered - the byte-identical
// goldens are a live confirmation.
struct StatusBarWidgets
{
    StatusNumber w_ready = {}; // ready-weapon ammo count
    StatusNumber w_frags = {}; // deathmatch frags summary (replaces the arms)
    StatusPercent w_health = {}; // health percent
    StatusBinIcon w_armsbg = {}; // arms background (single-player only)
    StatusMultIcon w_arms[6] = {}; // the six weapon-ownership icons
    StatusMultIcon w_faces = {}; // the animated face
    StatusMultIcon w_keyboxes[3] = {}; // the three key slots
    StatusPercent w_armor = {}; // armor percent
    StatusNumber w_ammo[4] = {}; // the four ammo-type counts
    StatusNumber w_maxammo[4] = {}; // the four ammo-type caps

    // The "n/a" sentinel w_ready.num points at when the ready weapon uses no ammo (fist/chainsaw).
    // updateWidgets' own function-local static (the "later function-local pass"); it must persist,
    // since the widget holds its address and reads it when drawing. Never reassigned - a constant in
    // practice - so its stable member address is what the widget needs, now reached as
    // widgets.largeammo.
    int largeammo = 1994;
};

// The one StatusBarWidgets, a view onto the Engine's member - the same pattern as the other
// clusters (statusBarFace(), hudMessage(), ...).
StatusBarWidgets& statusBarWidgets();
} // namespace Doom
