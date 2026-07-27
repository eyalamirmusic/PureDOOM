# eacp itself is added at the root, where the tests can reach eacp-core too.

# MakeASound is added here rather than at the root because audio is the *host's*
# job, not the engine's: the engine mixes into a buffer and hands out MIDI
# messages (Doom::soundBuffer / Doom::tickMidi), and it is this app that owns the
# output device and the MIDI port. The engine-only test loop
# (-DPUREDOOM_BUILD_EACP_EXAMPLE=OFF) therefore never fetches miniaudio, RtMidi
# or Miro.
#
# CPM dedupes by NAME, so MakeASound's own EADataStructures (reached through
# Miro) resolves to the one the root already added.
include(CPM)
CPMAddPackage(
        NAME MakeASound
        GITHUB_REPOSITORY eyalamirmusic/MakeASound
        GIT_TAG main)

# eacp only runs eacp_default_setup() when it is the top-level project, so as
# a CPM dependency it never sets the bundle plist template that
# set_default_target_setting() stamps onto app targets. See the gap log in
# CLAUDE.md.
if (APPLE AND NOT IOS)
    set(EACP_MACOS_PLIST "${eacp_SOURCE_DIR}/CMake/macOSBundleInfo.plist.in"
            CACHE INTERNAL "eacp macOS bundle Info.plist template")
endif ()

add_executable(PureDoomEACP
        Main.cpp
        EngineAccess.cpp
        App.h
        Audio.h
        Audio.cpp
        AutomapShader.h
        Genmidi.h
        Genmidi.cpp
        OplPlayer.h
        OplPlayer.cpp
        Common.h
        DoomShader.h
        EngineAccess.h
        HudShader.h
        Input.h
        Layout.h
        OverlayShader.h
        ScreenShader.h
        Textures.h
        View.h
        View.cpp
        WipeShader.h
        WorldShader.h)

# The engine is a library, and the same one the tests link - so a change to the
# simulation reaches the game and the suite together, and neither can be running
# code the other is not.
# nuked-opl3 is added at the root, where the tests reach it too - the music
# player is drivable headlessly, so it has a gate the rest of the audio path
# does not.
target_link_libraries(PureDoomEACP PRIVATE
        eacp-gpu doom-engine MakeASound nuked-opl3)
target_compile_definitions(PureDoomEACP PRIVATE
        PUREDOOM_ROOT_DIR="${CMAKE_SOURCE_DIR}")

set(BUNDLE_ID "com.eacp.puredoom")

set_target_properties(PureDoomEACP PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "PureDoomEACP"
        MACOSX_BUNDLE_GUI_IDENTIFIER ${BUNDLE_ID}
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${BUNDLE_ID})

eacp_set_gui_subsystem(PureDoomEACP)
set_default_target_setting(PureDoomEACP)
