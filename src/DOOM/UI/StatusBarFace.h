#pragma once

#include "../doomdef.h" // NUMWEAPONS
#include "../doomtype.h" // doom_boolean

namespace Doom
{
// The animated face's selection state (ST_updateFaceWidget). st_faceindex is the face patch shown
// (the w_faces widget binds to it); st_facecount times how long the current expression holds before
// the next is chosen. The rest drive which expression: st_oldhealth is last tic's health (a big
// drop picks a pained face), oldweaponsowned is last tic's weapon set (a new one triggers the evil
// grin), and st_randomnumber is a per-tic M_Random the straight-ahead face varies with.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// UI/StatusBar's own file-local statics, read by no other file. The vanilla names become references
// onto the members (oldweaponsowned as a reference-to-array). The face is drawn into screens[0]
// every tic and the demos take damage, grab weapons, rampage and die, so this is live
// frame-golden-covered - the byte-identical goldens are a live confirmation.
//
// The last four are the face drawer's own function-local statics (the "later function-local pass"),
// distinct from the file-scope statics above but the same face subsystem, so they join the cluster:
// lastcalc/oldhealth cache the pain-offset computation in stcalcPainOffset (recomputed only when
// health changes), and lastattackdown/priority carry the expression state machine across tics in
// stupdateFaceWidget. Vanilla never resets them (they are function-local), so they are members with
// matching defaults reached by a local reference in each function - identical persistence in a
// single-Engine process.
struct StatusBarFace
{
    int st_oldhealth = -1; // last tic's health, for the pained face
    doom_boolean oldweaponsowned[NUMWEAPONS] =
        {}; // last tic's weapons, for the evil grin
    int st_facecount = 0; // tics until the expression may change
    int st_faceindex = 0; // the face patch currently shown (w_faces binds to it)
    int st_randomnumber = 0; // a per-tic M_Random the straight face varies with

    int lastcalc = 0; // stcalcPainOffset: the cached pain offset
    int oldhealth = -1; // stcalcPainOffset: the health it was cached for
    int lastattackdown = -1; // stupdateFaceWidget: the attack-button ouch latch
    int priority = 0; // stupdateFaceWidget: the current expression's precedence
};

// The one StatusBarFace, a view onto the Engine's member - the same pattern as the other clusters
// (hudMessage(), graphicsData(), ...).
StatusBarFace& statusBarFace();
} // namespace Doom
