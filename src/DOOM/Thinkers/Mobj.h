#pragma once

// The map object (mobj) - the thinker every actor in the world is: monsters,
// the player, missiles, items, puffs and blood. Moved here (struct and flags) out
// of Sim/MobjTypes.h, which now includes this header and stays the name the rest
// of the engine includes. Its per-tic behaviour is tick() (Thinkers/Mobj.cpp);
// spawnMobj and the rest of the mobj machinery live in Sim/Mobj.{h,cpp}.

// Basics.
#include "../Math/TrigTables.h"
#include "../Math/FixedPoint.h"
#include "../Math/Vec.h"

// We need the Thinker stuff.
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

//
// NOTES: Mobj
//
// mobj_ts are used to tell the refresh where to draw an image,
// tell the world simulation when objects are contacted,
// and tell the sound driver how to position a sound.
//
// The refresh uses the next and prev links to follow
// lists of things in sectors as they are being drawn.
// The sprite, frame, and angle elements determine which Patch
// is used to draw the sprite if it is visible.
// The sprite and frame values are allmost allways set
// from State structures.
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
// to do stereo positioning of any sound effited by the Mobj.
//
// The play simulation uses the blocklinks, x,y,z, radius, height
// to determine when mobj_ts are touching each other,
// touching lines in the map, or hit by trace lines (gunshots,
// lines of sight, etc).
// The Mobj->flags element has various bit flags
// used by the simulation.
//
// Every Mobj is linked into a single sector
// based on its origin coordinates.
// The SubSector is found with pointInSubsector({x, y}),
// and the Sector can be found with subsector->sector.
// The sector links are only used by the rendering code,
// the play simulation does not care about them at all.
//
// Any Mobj that needs to be acted upon by something else
// in the play world (block movement, be shot, etc) will also
// need to be linked into the blockmap.
// If the thing has the MobjFlag::NoBlockmap flag set, it will not use
// the block links. It can still interact with other things,
// but only as the instigator (missiles will run into other
// things, but nothing can run into a missile).
// Each block in the grid is 128*128 units, and knows about
// every Line that it contains a piece of, and every
// interactable Mobj that has its origin contained.
//
// A valid Mobj is a Mobj that has the proper SubSector
// filled in for its xy coordinates and is linked into the
// sector from which the subsector was made, or has the
// MobjFlag::NoSector flag set (the SubSector needs to be valid
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

// Map Object definition.
struct Mobj : Thinker
{
    // Was `Thinker thinker;` as the first member; a mobj now *is* a Thinker.
    // Its per-tic action (vanilla's P_MobjThinker) is tick(), defined in
    // Thinkers/Mobj.cpp.
    void tick() override;

    // The one override of it: every mobj scan in the engine, the port and the probe
    // asks a Thinker for this rather than reading a type tag and casting.
    Mobj* asMobj() override { return this; }

    // State-action methods (vanilla's A_* codepointers): Sim/Info.cpp's state
    // table installs each as &Mobj::name, and setMobjState invokes the one its
    // state names via (mobj.*action.mobj)(). The bodies live in Sim/Enemy.cpp,
    // except bfgSpray, which lives in Sim/Weapon.cpp with the rest of the BFG.
    void look();
    void chase();
    void faceTarget();
    void posAttack();
    void sPosAttack();
    void cPosAttack();
    void cPosRefire();
    void spidRefire();
    void bspiAttack();
    void troopAttack();
    void sargAttack();
    void headAttack();
    void cyberAttack();
    void bruisAttack();
    void skelMissile();
    void traceTarget(); // vanilla A_Tracer; renamed off the `tracer` member
    void skelWhoosh();
    void skelFist();
    void vileChase();
    void vileStart();
    void startFire();
    void fireCrackle();
    void fire();
    void vileTarget();
    void vileAttack();
    void fatRaise();
    void fatAttack1();
    void fatAttack2();
    void fatAttack3();
    void skullAttack();
    void painAttack();
    void painDie();
    void keenDie();
    void scream();
    void xScream();
    void pain();
    void fall();
    void explode();
    void bossDeath();
    void hoof();
    void metal();
    void babyMetal();
    void bfgSpray();
    void brainAwake();
    void brainPain();
    void brainScream();
    void brainExplode();
    void brainDie();
    void brainSpit();
    void spawnSound();
    void spawnFly();
    void playerScream();

    // Monster-AI helpers the action methods drive (vanilla p_enemy internals):
    // stepping in the current direction, choosing a new one, target acquisition and
    // the range checks. Bodies in Sim/Enemy.cpp.
    bool checkMeleeRange();
    bool checkMissileRange();
    bool move();
    bool tryWalk();
    void newChaseDir();
    bool lookForPlayers(bool allaround);
    void painShootSkull(Angle angle);

    // Hitscan helpers for the player's weapons, run on the shooter mobj (vanilla
    // P_BulletSlope / P_GunShot). Bodies in Sim/Weapon.cpp.
    void computeBulletSlope();
    void gunShot(bool accurate);

    // Core mobj machinery (vanilla p_mobj): the state driver, the per-tic movement
    // steps, removal and missile spawning. Bodies in Sim/Mobj.cpp. spawnMobj and the
    // other factories stay free functions there (they have no mobj to be a method of).
    bool setState(StateNum stateToUse);
    void explodeMissile();
    void xyMovement();
    void zMovement();
    void nightmareRespawn();
    void remove();
    Mobj* spawnMissile(Mobj* dest, MobjType type);
    void spawnPlayerMissile(MobjType type);
    void checkMissileSpawn();

    // Movement clipping (vanilla p_map core): does this thing fit at (x, y), and the
    // commit of a move if it does. Bodies in Sim/Movement.cpp. The PIT_* blockmap
    // callbacks stay free functions there (the iterator takes their address).
    bool checkPosition(Vec2 target);
    bool tryMove(Vec2 target);
    bool teleportMove(Vec2 target);
    bool thingHeightClip();

    // Hitscan, splash and sliding (vanilla p_map, past the movement core). Bodies in
    // Sim/MapAction.cpp; aimLineAttack stays a free function there (its t1 may null).
    void slideMove();
    void lineAttack(Angle angle, Fixed distance, Fixed slope, int damage);
    void radiusAttack(Mobj* source, int damage);

    // Apply `damage` to this thing from inflictor/source (either may be null for
    // environmental damage): thrust, armor, pain and death (vanilla P_DamageMobj).
    // Body in Sim/Interaction.cpp; killMobj stays a free function there.
    void damage(Mobj* inflictor, Mobj* source, int damage);

    // Info for drawing: position.
    //
    // Sim/MapTypes.h's DegenMobj must keep this at the same offset - the sound
    // code casts one to a Mobj* and reads the position off it. Both hold a Vec3
    // as their first data member after the Thinker base, and
    // Tests/Sim/StateClusterTests.cpp puts the cast through its paces.
    Vec3 pos;

    // More list: links in sector (if needed)
    struct Mobj* snext = nullptr;
    struct Mobj* sprev = nullptr;

    //More drawing info: to determine current sprite.
    Angle angle; // orientation
    SpriteNum sprite = SpriteNum::Troo; // used to find Patch and flip value
    int frame = 0; // might be ORed with FF_FULLBRIGHT

    // Interaction info, by BLOCKMAP.
    // Links in blocks (if needed).
    struct Mobj* bnext = nullptr;
    struct Mobj* bprev = nullptr;

    SubSector* subsector = nullptr;

    // The closest interval over all contacted Sectors.
    Fixed floorz;
    Fixed ceilingz;

    // For movement checking.
    Fixed radius;
    Fixed height;

    // Momentums, used to update position.
    Vec3 mom;

    // If == validcount, already checked.
    int validcount = 0;

    MobjType type = MobjType::Player;
    MobjInfo* info = nullptr; // &mobjinfo()[mobj->type]

    int tics = 0; // state tic counter
    State* state = nullptr;
    int flags = 0;
    int health = 0;

    // Movement direction, movement generation (zig-zagging).
    int movedir = 0; // 0-7
    int movecount = 0; // when 0, select a new dir

    // Thing being chased/attacked (or 0),
    // also the originator for missiles.
    struct Mobj* target = nullptr;

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    int reactiontime = 0;

    // If >0, the target will be chased
    // no matter what (even if shot)
    int threshold = 0;

    // Additional info record for player avatars only.
    // Only valid if type == MobjType::Player
    struct Player* player = nullptr;

    // Player number last looked for.
    int lastlook = 0;

    // For nightmare respawn.
    MapThing spawnpoint;

    // Thing being chased/attacked for tracers.
    struct Mobj* tracer = nullptr;

    // Link this thing into its subsector's sector list and its blockmap cell (or
    // neither, for MobjFlag::NoSector / MobjFlag::NoBlockmap), setting subsector
    // from x,y - and the unlink that precedes a position change. Vanilla
    // P_SetThingPosition / P_UnsetThingPosition; bodies in Sim/MapUtil.cpp.
    void setPosition();
    void unsetPosition();
};
} // namespace Doom
