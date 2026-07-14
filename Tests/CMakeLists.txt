set(CMAKE_CXX_STANDARD 20)
set(CMAKE_FOLDER Tests)

find_package(NanoTest REQUIRED)

# The window into the running simulation. A library of its own so every test
# binary links the same shim rather than recompiling it.
add_library(doom-sim-probe STATIC SimProbe.cpp SimProbe.h)
target_link_libraries(doom-sim-probe PUBLIC doom-engine)
target_include_directories(doom-sim-probe PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# The probe boots the engine with -config Tests/doom-tests.cfg. Without it,
# M_LoadDefaults reads the developer's own ~/.doomrc, and screenblocks alone
# (10 in a real config, 9 by default) changes every rendered frame - so the frame
# goldens would only agree with the machine that recorded them.
target_compile_definitions(doom-sim-probe PUBLIC
        PUREDOOM_TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# NanoTest registers one ctest case per test and re-runs the binary with
# --test <name>, so each of these gets a fresh process and therefore a fresh
# doom_init. That matters more than it sounds: the engine is a few hundred
# globals and a zone allocator, and tests that shared a process would have to be
# ordered by hand.
nano_add_executable(SimTests
        NO_MAIN
        SOURCES
        TestMain.cpp
        DemoReplay.h
        Sim/DemoTests.cpp
        Sim/WadTests.cpp
        TARGETS doom-sim-probe)

target_compile_definitions(SimTests PRIVATE
        PUREDOOM_ROOT_DIR="${CMAKE_SOURCE_DIR}")

# The arithmetic underneath the simulation. Pure functions over tables the engine
# ships with, so these boot nothing and are free to share a process - which is
# why they are their own binary rather than more cases in SimTests.
nano_add_executable(PrimitiveTests
        SOURCES Sim/PrimitiveTests.cpp
        TARGETS doom-sim-probe)

# Re-records the goldens - one demo per process, because the engine only boots
# once. Run it when a change to the simulation was *intended*; the diff on
# Goldens/ is then the reviewable record of what it changed.
set(RECORD_GOLDENS_COMMANDS "")
foreach (case IN ITEMS demo1 demo2 demo3 wadDirectory)
    list(APPEND RECORD_GOLDENS_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E env DOOM_UPDATE_GOLDENS=1
            $<TARGET_FILE:SimTests> --test Sim/${case})
endforeach ()

add_custom_target(record-goldens
        ${RECORD_GOLDENS_COMMANDS}
        DEPENDS SimTests
        VERBATIM
        COMMENT "Re-recording DOOM simulation goldens")
