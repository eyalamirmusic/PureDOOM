set(CMAKE_CXX_STANDARD 20)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")
include(CPM)

CPMAddPackage(
        NAME eacp
        GITHUB_REPOSITORY eyalamirmusic/eacp
        GIT_TAG main
        OPTIONS "EACP_BUILD_WEBVIEW OFF")

# eacp only runs eacp_default_setup() when it is the top-level project, so as
# a CPM dependency it never sets the bundle plist template that
# set_default_target_setting() stamps onto app targets. See the gap log in
# CLAUDE.md.
if (APPLE AND NOT IOS)
    set(EACP_MACOS_PLIST "${eacp_SOURCE_DIR}/CMake/macOSBundleInfo.plist.in"
            CACHE INTERNAL "eacp macOS bundle Info.plist template")
endif ()

add_executable(PureDoomEACP Main.cpp DoomImpl.c)
target_link_libraries(PureDoomEACP PRIVATE eacp-sprites)
target_compile_definitions(PureDoomEACP PRIVATE
        PUREDOOM_ROOT_DIR="${CMAKE_SOURCE_DIR}")

set_source_files_properties(DoomImpl.c PROPERTIES
        COMPILE_OPTIONS "$<IF:$<C_COMPILER_ID:MSVC>,/w,-w>")

set(BUNDLE_ID "com.eacp.puredoom")

set_target_properties(PureDoomEACP PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "PureDoomEACP"
        MACOSX_BUNDLE_GUI_IDENTIFIER ${BUNDLE_ID}
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${BUNDLE_ID})

eacp_set_gui_subsystem(PureDoomEACP)
set_default_target_setting(PureDoomEACP)
