#pragma once

// The map object (mobj) - the thinker every actor in the world is: monsters,
// the player, missiles, items, puffs and blood. Moved here (struct and flags) out
// of Sim/MobjTypes.h, which now includes this header and stays the name the rest
// of the engine includes. Its per-tic behaviour is tick() (Thinkers/Mobj.cpp);
// spawnMobj and the rest of the mobj machinery live in Sim/Mobj.{h,cpp}.

// Basics.
#include "../Math/TrigTables.h"
#include "../Math/FixedPoint.h"

// We need the Doom::Thinker stuff.
#include "../Sim/ActionFunc.h"

// We need the WAD data structure for Map things,
// from the THINGS lump.
#include "../Wad/MapFormat.h"

// States are tied to finite states are
//  tied to animation frames.
// Needs precompiled tables/data structures.
#include "../Sim/Info.h"

// Held only by pointer here; r_defs.h has the definition.
namespace Doom
{
struct SubSector;
} // namespace Doom

//
// NOTES: Doom::Mobj
//
// mobj_ts are used to tell the refresh where to draw an image,
// tell the world simulation when objects are contacted,
// and tell the sound driver how to position a sound.
//
// The refresh uses the next and prev links to follow
// lists of things in sectors as they are being drawn.
// The sprite, frame, and angle elements determine which Doom::Patch
// is used to draw the sprite if it is visible.
// The sprite and frame values are allmost allways set
// from Doom::State structures.
// The statescr.exe utility generates the states.h and states.c
// files that contain the sprite/frame numbers from the
// statescr.txt source file.
// The xyz origin point represents a point at the bottom middle
// of the sprite (between the feet of a biped).
// This is the default origin position for patch_ts grabbed
// with lumpy.exe.
// A walking creature will have its z equal to the floor
// it is standing on.
//
// The sound code uses the x,y, and subsector fields
// to do stereo positioning of any sound effited by the Doom::Mobj.
//
// The play simulation uses the blocklinks, x,y,z, radius, height
// to determine when mobj_ts are touching each other,
// touching lines in the map, or hit by trace lines (gunshots,
// lines of sight, etc).
// The Doom::Mobj->flags element has various bit flags
// used by the simulation.
//
// Every Doom::Mobj is linked into a single sector
// based on its origin coordinates.
// The Doom::SubSector is found with Doom::pointInSubsector(x,y),
// and the Doom::Sector can be found with subsector->sector.
// The sector links are only used by the rendering code,
// the play simulation does not care about them at all.
//
// Any Doom::Mobj that needs to be acted upon by something else
// in the play world (block movement, be shot, etc) will also
// need to be linked into the blockmap.
// If the thing has the MobjFlag::NoBlockmap flag set, it will not use
// the block links. It can still interact with other things,
// but only as the instigator (missiles will run into other
// things, but nothing can run into a missile).
// Each block in the grid is 128*128 units, and knows about
// every Doom::Line that it contains a piece of, and every
// interactable Doom::Mobj that has its origin contained.
//
// A valid Doom::Mobj is a Doom::Mobj that has the proper Doom::SubSector
// filled in for its xy coordinates and is linked into the
// sector from which the subsector was made, or has the
// MobjFlag::NoSector flag set (the Doom::SubSector needs to be valid
// even if MobjFlag::NoSector is set), and is linked into a blockmap
// block or has the MobjFlag::NoBlockmap flag set.
// Links should only be modified by the P_[Un]SetThingPosition()
// functions.
// Do not change the MobjFlag::No* flags while a thing is valid.
//
// Any questions?
//

//
// Misc. mobj flags
//
namespace Doom
{
enum class MobjFlag
{
    // Call P_SpecialThing when touched.
    Special = 1,
    // Blocks.
    Solid = 2,
    // Can be hit.
    Shootable = 4,
    // Don't use the sector links (invisible but touchable).
    NoSector = 8,
    // Don't use the blocklinks (inert but displayable)
    NoBlockmap = 16,

    // Not to be activated by sound, deaf monster.
    Ambush = 32,
    // Will try to attack right back.
    JustHit = 64,
    // Will take at least one step before attacking.
    JustAttacked = 128,
    // On level spawning (initial position),
    //  hang from ceiling instead of stand on floor.
    SpawnCeiling = 256,
    // Don't apply gravity (every tic),
    //  that is, object will float, keeping current height
    //  or changing it actively.
    NoGravity = 512,

    // Movement flags.
    // This allows jumps from high places.
    DropOff = 0x400,
    // For players, will pick up items.
    Pickup = 0x800,
    // Player cheat. ???
    NoClip = 0x1000,
    // Player: keep info about sliding along walls.
    Slide = 0x2000,
    // Allow moves to any height, no gravity.
    // For active floaters, e.g. cacodemons, pain elementals.
    Float = 0x4000,
    // Don't cross lines
    //   ??? or look at heights on teleport.
    Teleport = 0x8000,
    // Don't hit same species, explode on block.
    // Player missiles as well as fireballs of various kinds.
    Missile = 0x10000,
    // Dropped by a demon, not level spawned.
    // E.g. ammo clips dropped by dying former humans.
    Dropped = 0x20000,
    // Use fuzzy draw (shadow demons or spectres),
    //  temporary player invisibility powerup.
    Shadow = 0x40000,
    // Flag: don't bleed when shot (use puff),
    //  barrels and shootable furniture shall not bleed.
    NoBlood = 0x80000,
    // Don't stop moving halfway off a step,
    //  that is, have dead bodies slide down all the way.
    Corpse = 0x100000,
    // Floating to a height for a move, ???
    //  don't auto float to target's height.
    InFloat = 0x200000,

    // On kill, count this enemy object
    //  towards intermission kill total.
    // Happy gathering.
    CountKill = 0x400000,

    // On picking up, count this item object
    //  towards intermission item total.
    CountItem = 0x800000,

    // Special handling: skull in flight.
    // Neither a cacodemon nor a missile.
    SkullFly = 0x1000000,

    // Don't spawn this object
    //  in death match mode (e.g. key cards).
    NotDmatch = 0x2000000
};

// Not flags, and so deliberately not enumerators: together these are a two-bit
// bitfield packed into the high end of the same word, selecting a player's colour
// translation table. Extracted as a field, never tested as a flag.
constexpr int mobjTranslationMask = 0xc000000;
constexpr int mobjTranslationShift = 26;

} // namespace Doom

// Map Object definition.
namespace Doom
{
struct Mobj : Thinker
{
    // Was `Thinker thinker;` as the first member; a mobj now *is* a Thinker.
    // Its per-tic action (vanilla's P_MobjThinker) is tick(), defined in
    // Thinkers/Mobj.cpp.
    void tick() override;
    ThinkerKind kind() const override { return ThinkerKind::Mobj; }

    // Info for drawing: position.
    Fixed x;
    Fixed y;
    Fixed z;

    // More list: links in sector (if needed)
    struct Mobj* snext;
    struct Mobj* sprev;

    //More drawing info: to determine current sprite.
    Angle angle; // orientation
    SpriteNum sprite; // used to find Patch and flip value
    int frame; // might be ORed with FF_FULLBRIGHT

    // Interaction info, by BLOCKMAP.
    // Links in blocks (if needed).
    struct Mobj* bnext;
    struct Mobj* bprev;

    SubSector* subsector;

    // The closest interval over all contacted Sectors.
    Fixed floorz;
    Fixed ceilingz;

    // For movement checking.
    Fixed radius;
    Fixed height;

    // Momentums, used to update position.
    Fixed momx;
    Fixed momy;
    Fixed momz;

    // If == validcount, already checked.
    int validcount;

    MobjType type;
    MobjInfo* info; // &mobjinfo()[mobj->type]

    int tics; // state tic counter
    State* state;
    int flags;
    int health;

    // Movement direction, movement generation (zig-zagging).
    int movedir; // 0-7
    int movecount; // when 0, select a new dir

    // Thing being chased/attacked (or 0),
    // also the originator for missiles.
    struct Mobj* target;

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    int reactiontime;

    // If >0, the target will be chased
    // no matter what (even if shot)
    int threshold;

    // Additional info record for player avatars only.
    // Only valid if type == MobjType::Player
    struct Player* player;

    // Player number last looked for.
    int lastlook;

    // For nightmare respawn.
    MapThing spawnpoint;

    // Thing being chased/attacked for tracers.
    struct Mobj* tracer;
};
} // namespace Doom
