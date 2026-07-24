# The DOOM engine itself.
#
# This fork owns these sources and modifies them. The app and the test suite
# both link this one library, so a change to the simulation reaches both and
# cannot go stale in one of them. It is the only way to build the engine: the
# generated single header is gone, and with it the rule that no two files may
# share a file-scope name.

# The sources are listed explicitly rather than globbed: a glob makes the set of
# files a build-time discovery that CMake has to re-run (CONFIGURE_DEPENDS) and
# that no diff can review, and it silently sweeps in a stray file the moment it
# lands in a directory. Listing them makes adding, moving or deleting a file a
# reviewable edit here - which is what the two buckets below are *for*.
#
# The split is between the two flag sets applied further down. Vanilla's files
# were the flat 1993 C; a file moved into a subdirectory the moment it became C++
# someone wrote, and picked up the strict warnings there. That move has finished:
# the vanilla bucket is now two top-level headers and no translation unit at all,
# so everything compiled is under the strict flags. The machinery stays - it costs
# nothing and documents the rule.
set(DOOM_VANILLA_SOURCES
        DOOM.h
        doomtype.h)

# Containers.h is flat but is not vanilla: it is the container vocabulary the
# subdirectories share, written for this refactor, so it belongs with the
# rewritten sources. (Which bucket a header sits in changes no compilation - the
# flags are a no-op on a header that no translation unit is - it keeps the lists
# meaning what their names say.)
set(DOOM_REWRITTEN_SOURCES
        Containers.h

        Math/Angle.h
        Math/BBox.cpp
        Math/BBox.h
        Math/Fixed.cpp
        Math/Fixed.h
        Math/FixedPoint.h
        Math/FunctionRef.h
        Math/Swap.h
        Math/Trig.cpp
        Math/Trig.h
        Math/TrigTables.h
        Math/Vec2.h

        Sim/ActionFunc.h
        Sim/ActionScratch.h
        Sim/ActiveSpecials.h
        Sim/AnimatedSurfaces.h
        Sim/Blockmap.h
        Sim/Ceilings.cpp
        Sim/Ceilings.h
        Sim/Clip.h
        Sim/Doors.cpp
        Sim/Doors.h
        Sim/EndLevelTimer.h
        Sim/Enemy.cpp
        Sim/Enemy.h
        Sim/EnemyAI.h
        Sim/Floors.cpp
        Sim/Floors.h
        Sim/Info.cpp
        Sim/Info.h
        Sim/Interaction.cpp
        Sim/Interaction.h
        Sim/ItemRespawnQueue.h
        Sim/Items.cpp
        Sim/ItemTypes.h
        Sim/Level.h
        Sim/LevelPool.h
        Sim/Lights.cpp
        Sim/Lights.h
        Sim/MapAction.cpp
        Sim/MapAction.h
        Sim/MapGeometry.h
        Sim/MapTypes.h
        Sim/MapUtil.cpp
        Sim/MapUtil.h
        Sim/Mobj.cpp
        Sim/Mobj.h
        Sim/MobjTypes.h
        Sim/Movement.cpp
        Sim/Movement.h
        Sim/Plats.cpp
        Sim/Plats.h
        Sim/Player.cpp
        Sim/Player.h
        Sim/PlayerScratch.h
        Sim/Random.cpp
        Sim/Random.h
        Sim/SaveGame.cpp
        Sim/SaveGame.h
        Sim/Setup.cpp
        Sim/Setup.h
        Sim/Sight.cpp
        Sim/Sight.h
        Sim/SimDefs.h
        Sim/SoundTarget.h
        Sim/Specials.cpp
        Sim/Specials.h
        Sim/SpecialTypes.h
        Sim/Switches.cpp
        Sim/Switches.h
        Sim/SwitchList.h
        Sim/Teleport.cpp
        Sim/Teleport.h
        Sim/Thinker.h
        Sim/ThinkerList.h
        Sim/Tick.cpp
        Sim/Tick.h
        Sim/ValidCount.h
        Sim/Weapon.cpp
        Sim/Weapon.h
        Sim/WeaponScratch.h
        Sim/WeaponTypes.h

        Thinkers/Ceiling.cpp
        Thinkers/Ceiling.h
        Thinkers/Door.cpp
        Thinkers/Door.h
        Thinkers/FireFlicker.cpp
        Thinkers/FireFlicker.h
        Thinkers/FloorMove.cpp
        Thinkers/FloorMove.h
        Thinkers/Glow.cpp
        Thinkers/Glow.h
        Thinkers/LightFlash.cpp
        Thinkers/LightFlash.h
        Thinkers/Mobj.cpp
        Thinkers/Mobj.h
        Thinkers/Plat.cpp
        Thinkers/Plat.h
        Thinkers/Strobe.cpp
        Thinkers/Strobe.h

        Render/BSP.cpp
        Render/BSP.h
        Render/BSPScratch.h
        Render/CompositeCache.h
        Render/Data.cpp
        Render/Data.h
        Render/Draw.cpp
        Render/Draw.h
        Render/Drawers.h
        Render/DrawState.h
        Render/DrawTables.h
        Render/GraphicsData.h
        Render/Lighting.h
        Render/Main.cpp
        Render/Main.h
        Render/Planes.cpp
        Render/Planes.h
        Render/PlaneScratch.h
        Render/RenderMainState.h
        Render/RenderScratch.h
        Render/RenderTypes.h
        Render/Segs.cpp
        Render/Segs.h
        Render/SegState.h
        Render/Sky.cpp
        Render/Sky.h
        Render/SolidSegs.h
        Render/SpriteScratch.h
        Render/SpriteState.h
        Render/Things.cpp
        Render/Things.h
        Render/Video.cpp
        Render/Video.h
        Render/VideoState.h
        Render/ViewPoint.h
        Render/ViewProjection.h
        Render/ViewWindow.h
        Render/WallScratch.h

        UI/Automap.cpp
        UI/Automap.h
        UI/AutomapTypes.h
        UI/AutomapView.h
        UI/Cheat.cpp
        UI/Cheat.h
        UI/CheatTypes.h
        UI/Finale.cpp
        UI/Finale.h
        UI/FinaleState.h
        UI/Hud.cpp
        UI/Hud.h
        UI/HudChat.h
        UI/HudFlags.h
        UI/HudFont.h
        UI/HudMessage.h
        UI/HudState.h
        UI/HudWidgets.cpp
        UI/HudWidgets.h
        UI/HudWidgetTypes.h
        UI/Intermission.cpp
        UI/Intermission.h
        UI/IntermissionState.h
        UI/IntermissionTypes.h
        UI/Menu.cpp
        UI/Menu.h
        UI/MenuSettings.h
        UI/MenuState.h
        UI/StatusBar.cpp
        UI/StatusBar.h
        UI/StatusBarFace.h
        UI/StatusBarGraphics.h
        UI/StatusBarState.h
        UI/StatusBarTypes.h
        UI/StatusBarWidgets.h
        UI/StatusWidgetGraphics.h
        UI/StatusWidgets.cpp
        UI/StatusWidgets.h
        UI/StatusWidgetTypes.h
        UI/Wipe.cpp
        UI/Wipe.h
        UI/WipeState.h

        Game/AmmoLimits.h
        Game/Args.cpp
        Game/Args.h
        Game/AttractMode.h
        Game/Config.cpp
        Game/Config.h
        Game/ConfigPaths.h
        Game/ConfigTypes.h
        Game/CorpseQueue.h
        Game/DeferredNewGame.h
        Game/DemoState.h
        Game/DisplayState.h
        Game/DoomMain.cpp
        Game/DoomMain.h
        Game/EngineParams.h
        Game/Event.h
        Game/EventQueue.h
        Game/Game.cpp
        Game/Game.h
        Game/GameClock.h
        Game/GameDefs.h
        Game/GameFlow.h
        Game/GameSession.h
        Game/GameVersion.h
        Game/InputConfig.h
        Game/IntermissionInfo.h
        Game/LaunchOptions.h
        Game/LevelStats.h
        Game/MapSpawns.h
        Game/MovementSpeeds.h
        Game/Net.cpp
        Game/Net.h
        Game/NetState.h
        Game/NetTypes.h
        Game/OverlayState.h
        Game/ParTimes.h
        Game/PendingCommands.h
        Game/PlayerState.h
        Game/PlayerTypes.h
        Game/RefreshFlags.h
        Game/SaveGameState.h
        Game/SkyState.h
        Game/Sound.cpp
        Game/Sound.h
        Game/SoundData.cpp
        Game/SoundData.h
        Game/SoundSettings.h
        Game/SoundState.h
        Game/StartupDefaults.h
        Game/Strings.h
        Game/StringsEnglish.h
        Game/StringsFrench.h
        Game/Ticcmd.h
        Game/TiccmdInput.h
        Game/TimeDemo.h

        Host/Api.cpp
        Host/Diagnostics.h
        Host/Host.cpp
        Host/Host.h
        Host/Net.cpp
        Host/Net.h
        Host/Platform.h
        Host/Sound.cpp
        Host/Sound.h
        Host/System.cpp
        Host/System.h
        Host/Text.cpp
        Host/Text.h
        Host/Video.cpp
        Host/Video.h

        Wad/MapFormat.h
        Wad/WadFile.cpp
        Wad/WadFile.h

        Engine/Engine.cpp
        Engine/Engine.h)

add_library(doom-engine STATIC ${DOOM_VANILLA_SOURCES} ${DOOM_REWRITTEN_SOURCES})
add_library(DOOM::engine ALIAS doom-engine)

target_compile_features(doom-engine PUBLIC cxx_std_20)

# eacp's EA::Vector / EA::Array, used by the rewritten engine code. Header-only
# and INTERFACE-only; added at the root ahead of this subdirectory. PUBLIC so the
# rewritten headers that expose these types (Sim/Level.h, Math/Trig.h, ...) carry
# the include path to the app and the tests.
target_link_libraries(doom-engine PUBLIC ea_data_structures)

# eacp-core, for the platform work the engine would otherwise hand-roll per OS.
# PRIVATE: this is an implementation choice, not part of the engine's interface -
# DOOM.h stays a plain extern "C" header with no eacp type anywhere in it, and an
# embedder still overrides any of it through the doom_set_* callbacks. A consumer
# that wants eacp itself links eacp itself.
target_link_libraries(doom-engine PRIVATE eacp-core)

# The DOOM_IMPLEMENT_* gates went with the extern "C" API: Doom::Host's
# constructor (Host/Host.cpp) installs working defaults unconditionally, and an
# embedder overrides members of Doom::host() instead of pre-setting pointers.
# (Every build here defined all six, so the unconditional defaults change
# nothing. DOOM_IMPLEMENT_SOCKETS was never read by any line of source; the
# socket code gates on I_NET_ENABLED, which nothing defines.)

# Consumers include <DOOM/DOOM.h>. The engine's own files include each other by
# bare name, which resolves next to them.
target_include_directories(doom-engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Which spelling a flag takes is decided by the compiler's *driver*, not by who
# wrote the compiler. clang-cl is CMAKE_CXX_COMPILER_ID "Clang" behind an MSVC
# front end, so keying this on the compiler ID - as it was - handed GCC-style
# flags to an MSVC-style driver. That is not merely ignored: under clang-cl,
# `-Wall` is MSVC's /Wall, which clang implements as -Weverything. The engine
# reported ~44,000 warnings on Windows/ARM64 from that one substitution, nearly
# all of them -Wold-style-cast, -Wpadded and -Wunsafe-buffer-usage, and most of
# them raised inside eacp's headers rather than in any code here.
#
# CMAKE_CXX_COMPILER_FRONTEND_VARIANT is the question actually being asked: what
# command line does this thing accept? It is "MSVC" for both cl and clang-cl, and
# "GNU" for gcc, clang and Apple Clang.
if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(DOOM_WARNINGS_OFF /w)

    # /W4 is close to -Wall -Wextra but not equal to it: it also asks four
    # questions whose GCC/Clang equivalents this project has deliberately not
    # enabled. Turning those off is what makes "zero warnings" mean the same
    # thing on every toolchain, rather than a number that depends on which
    # compiler ran. Each is disabled for a stated reason, not for being noisy:
    #
    #   C4244/C4267  narrowing conversion. The GNU spelling is -Wconversion,
    #                which is in neither -Wall nor -Wextra. DOOM assigns int
    #                expressions into short and byte fields everywhere and the
    #                truncation is the behaviour the demos depend on; 121 of
    #                these are that idiom. Enabling -Wconversion everywhere is a
    #                real decision someone could make, and it should be made for
    #                all compilers at once rather than arrived at by accident on
    #                one of them.
    #   C4459/C4457  declaration hides a global / a parameter. That is -Wshadow,
    #                also in neither -Wall nor -Wextra.
    #   C4702        unreachable code. -Wunreachable-code, which Clang keeps out
    #                of -Wall/-Wextra too.
    #   C4805        mixing bool and int in one operation. No GNU equivalent at
    #                any level. The one instance of it that turned out to be
    #                worth acting on - SpriteFrame::rotate, a tri-state int being
    #                compared against `true` and `false` - has been fixed at the
    #                source rather than silenced here.
    #
    # Raising the bar is a separate, deliberate piece of work (the -Werror item
    # in REFACTOR.md's "What is left"): turn these back on together with
    # -Wconversion and -Wshadow on the GNU side, and fix what all of them find.
    set(DOOM_WARNINGS_ON /W4 /wd4244 /wd4267 /wd4459 /wd4457 /wd4702 /wd4805)
else ()
    set(DOOM_WARNINGS_OFF -w)
    set(DOOM_WARNINGS_ON -Wall -Wextra -Wpedantic)

    # The Xcode generator passes this on its own (it is an Xcode project
    # default), which is how 15 long->int narrowings surfaced there while every
    # Ninja build sat at zero. Pinning it here makes the two generators agree on
    # what zero means. Clang-only: GCC's nearest spelling is -Wconversion, which
    # is the wider, deliberate decision that item reserves.
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND DOOM_WARNINGS_ON -Wshorten-64-to-32)
    endif ()
endif ()

# 1993 C compiled as C++: it warns copiously and none of it is news. The blanket
# is per-file, and a file leaves it the moment it is genuinely rewritten - which
# is why the rewritten sources below are compiled with warnings on instead. New
# code is held to eacp's standard from its first line; vanilla is not held to
# anything until someone rewrites it.
set_source_files_properties(${DOOM_VANILLA_SOURCES} DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES COMPILE_OPTIONS "${DOOM_WARNINGS_OFF}")

set_source_files_properties(${DOOM_REWRITTEN_SOURCES} DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES COMPILE_OPTIONS "${DOOM_WARNINGS_ON}")

# The simulation is not strictly integer-only: FixedDiv2 divides through a double
# (Math/Fixed), and the demos replay because that is the same IEEE-754 operation
# every time. Nothing may fuse it into an FMA, which rounds differently. No
# contraction is possible there today - a divide and a multiply, with no add
# between them - but determinism the goldens rest on should be stated rather than
# left to luck. eacp pins the same flag on its numeric kernels for the same reason.
#
# The same driver split applies, and here it had teeth: clang-cl took the flag as
# `-ffp-contract=off`, did not recognise it, and said so once per translation unit
# ("unknown argument ignored in clang-cl"). The engine was building *without* the
# guarantee on the one Windows toolchain most likely to contract, and the warning
# saying so was lost among the other 44,000. /clang: hands it to the clang front
# end, which is where it was always meant to go. Real MSVC has no equivalent knob
# but defaults to /fp:precise, which does not contract; state it rather than
# inherit it, since /fp:fast in a stray toolchain file would silently desync.
if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(doom-engine PRIVATE /clang:-ffp-contract=off)
    else ()
        target_compile_options(doom-engine PRIVATE /fp:precise)
    endif ()
else ()
    target_compile_options(doom-engine PRIVATE -ffp-contract=off)
endif ()
