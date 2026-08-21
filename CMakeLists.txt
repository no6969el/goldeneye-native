cmake_minimum_required(VERSION 3.16)
project(goldeneye-native LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

option(GE_VR_WITH_OPENXR "Build against the OpenXR SDK" ON)
option(GE_VR_BUILD_TESTS "Build the math parity tests"  ON)

# ---------------------------------------------------------------------------
# ge_gbi — the display-list interpreter. Independent of OpenXR on purpose: it
# is what the FLAT-SCREEN port needs, and phase 3 ships without any VR at all.
# ---------------------------------------------------------------------------
add_library(ge_gbi STATIC
    src/gbi/gbi.cpp
    src/gbi/gbi_interp.cpp
    src/rhi/vertex_pipeline.cpp
)
target_include_directories(ge_gbi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
    target_compile_options(ge_gbi PRIVATE /W4)
else()
    target_compile_options(ge_gbi PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# ge_ultra — the libultra shim: fibers, scheduler, message queues, flat RDRAM.
# No OpenXR, no renderer. This is what has to boot before anything else runs.
# ---------------------------------------------------------------------------
add_library(ge_ultra STATIC
    src/ultra/fiber.cpp
    src/ultra/os_thread.cpp
    src/ultra/rdram.cpp
    src/ultra/os_io.cpp
    src/ultra/os_ai.cpp
    src/ultra/audio/acmd_interp.cpp
)
target_include_directories(ge_ultra PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
    target_compile_options(ge_ultra PRIVATE /W4)
else()
    target_compile_options(ge_ultra PRIVATE -Wall -Wextra)
    find_package(Threads REQUIRED)
    target_link_libraries(ge_ultra PUBLIC Threads::Threads)
endif()

add_library(ge_vr STATIC
    src/xr_math.cpp
    src/xr_session.cpp
    src/xr_input.cpp
    src/ge_vr_bridge.cpp
)

target_include_directories(ge_vr
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ---------------------------------------------------------------------------
# OpenXR
#
# Optional on purpose. The bridge and the math must build and be testable on a
# machine with no runtime and no SDK — which is every CI box — otherwise the
# only place the projection math can be verified is on a headset, and that is
# not a place you can run a regression suite.
# ---------------------------------------------------------------------------
set(GE_VR_HAVE_OPENXR 0)

if(GE_VR_WITH_OPENXR)
    find_package(OpenXR QUIET)
    if(NOT OpenXR_FOUND)
        include(FetchContent)
        FetchContent_Declare(OpenXR
            GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
            GIT_TAG        release-1.1.36
            GIT_SHALLOW    TRUE
        )
        set(DYNAMIC_LOADER OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(OpenXR)
    endif()

    if(TARGET openxr_loader)
        target_link_libraries(ge_vr PUBLIC openxr_loader)
        set(GE_VR_HAVE_OPENXR 1)
    else()
        message(WARNING
            "OpenXR not available; building ge_vr with the null backend. "
            "The port will run flat-screen only.")
    endif()
endif()

target_compile_definitions(ge_vr PUBLIC GE_VR_HAVE_OPENXR=${GE_VR_HAVE_OPENXR})

if(MSVC)
    target_compile_options(ge_vr PRIVATE /W4)
else()
    target_compile_options(ge_vr PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(GE_VR_BUILD_TESTS)
    enable_testing()
    add_executable(ge_vr_tests tests/test_math.cpp)
    target_link_libraries(ge_vr_tests PRIVATE ge_vr)
    target_include_directories(ge_vr_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME ge_vr_tests COMMAND ge_vr_tests)

    add_executable(ge_gbi_tests tests/test_gbi.cpp)
    target_link_libraries(ge_gbi_tests PRIVATE ge_gbi)
    add_test(NAME ge_gbi_tests COMMAND ge_gbi_tests)

    add_executable(ge_ultra_tests tests/test_ultra.cpp)
    target_link_libraries(ge_ultra_tests PRIVATE ge_ultra ge_gbi)
    add_test(NAME ge_ultra_tests COMMAND ge_ultra_tests)

    add_executable(ge_io_tests tests/test_io.cpp)
    target_link_libraries(ge_io_tests PRIVATE ge_ultra)
    add_test(NAME ge_io_tests COMMAND ge_io_tests)

    add_executable(ge_audio_tests tests/test_audio.cpp)
    target_link_libraries(ge_audio_tests PRIVATE ge_ultra)
    add_test(NAME ge_audio_tests COMMAND ge_audio_tests)

    # ge_addr_tests is unlike the others: it tests a property of the LINK, not
    # of any function, so it has to be linked the way the game is linked. The
    # image base is what keeps host addresses out of the ROM offset space; with
    # it dropped the port loads the wrong third of the ROM and says nothing.
    #
    # Guarded on ge_assets.c existing, because that file is generated from the
    # user's own decomp checkout (tools/gen_asset_symbols.py) and is not in the
    # repository -- it would contain asset sizes derived from cartridge data.
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/host/ge_assets.c)
        add_executable(ge_addr_tests
            tests/test_asset_addr.c
            src/host/ge_assets.c
            src/host/ge_assets_load.c)
        target_include_directories(ge_addr_tests
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/host)
        if(NOT MSVC)
            target_link_options(ge_addr_tests PRIVATE
                -no-pie -Wl,-Ttext-segment=0x20000000)
        else()
            # MSVC's x64 default base of 0x140000000 is above 4 GB, which the
            # u32 devAddr in osPiStartDma truncates. Set explicitly for that
            # reason as much as for this one.
            target_link_options(ge_addr_tests PRIVATE /BASE:0x20000000 /FIXED)
        endif()
        add_test(NAME ge_addr_tests COMMAND ge_addr_tests)
    else()
        message(STATUS
            "ge_addr_tests skipped: src/host/ge_assets.c not generated yet "
            "(run tools/gen_asset_symbols.py against your decomp checkout)")
    endif()
endif()

# ---------------------------------------------------------------------------
# dl_validate — runs the interpreter over real display lists extracted from a
# ROM. Not a unit test: needs a corpus built by tools/extract_display_lists.py.
# ---------------------------------------------------------------------------
add_executable(dl_validate tools/dl_validate.cpp)
target_link_libraries(dl_validate PRIVATE ge_gbi ge_ultra)

add_executable(audio_validate tools/audio_validate.cpp)
target_link_libraries(audio_validate PRIVATE ge_ultra)

add_executable(geom_validate tools/geom_validate.cpp)
target_link_libraries(geom_validate PRIVATE ge_gbi)

add_executable(room_render tools/room_render.cpp)
target_link_libraries(room_render PRIVATE ge_gbi ge_ultra)

# ---------------------------------------------------------------------------
# RT64 backend — optional. Point RT64_DIR at an RT64 source tree that has
# already been built (its build/ holds rt64.a and the contrib static libs).
#
#   cmake -S . -B build -DRT64_DIR=/path/to/rt64
#
# Off by default: RT64 pulls in SDL2, Vulkan, X11 and GTK3, and the rest of the
# port must stay buildable without any of them.
# ---------------------------------------------------------------------------
set(RT64_DIR "" CACHE PATH "Path to a built RT64 source tree")

if(RT64_DIR)
    find_package(SDL2 REQUIRED)
    add_library(ge_rt64 STATIC src/rhi/rt64_backend.cpp)
    target_include_directories(ge_rt64 PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${RT64_DIR}/src
        ${RT64_DIR}/include
        ${RT64_DIR}/src/contrib
        ${RT64_DIR}/src/contrib/hlslpp/include
        ${RT64_DIR}/src/contrib/plume
        ${SDL2_INCLUDE_DIRS}
    )
    target_link_libraries(ge_rt64 PUBLIC ge_ultra)
    # These mirror RT64's own CMakeLists (lines 298-307). HLSL_CPU in
    # particular is load-bearing: RT64's src/shared/*.h are shared between HLSL
    # shaders and C++, and without it they compile as shader source and produce
    # a wall of "'float3' does not name a type" plus a bizarre collision with
    # POSIX select() in rt64_hlsl.h.
    target_compile_definitions(ge_rt64 PUBLIC
        HLSL_CPU
        FFX_GCC
        IMGUI_IMPL_VULKAN_NO_PROTOTYPES
        PLUME_SDL_VULKAN_ENABLED
        RT64_SDL_WINDOW_VULKAN
    )

    add_executable(rt64_probe tools/rt64_probe.cpp)
    target_link_libraries(rt64_probe PRIVATE ge_rt64 ge_gbi)
    target_include_directories(rt64_probe PRIVATE ${SDL2_INCLUDE_DIRS})
    # rt64.a plus its contrib static libraries, then the system libraries.
    #
    # GLOB_RECURSE, not GLOB: the contrib archives are nested several levels
    # down (src/contrib/plume/libplume.a and friends) and file(GLOB) does not
    # treat ** as recursive, so a plain glob silently finds only rt64.a and the
    # link fails on symbols that are actually present.
    file(GLOB_RECURSE RT64_CONTRIB_LIBS "${RT64_DIR}/build/*.a")
    list(REMOVE_ITEM RT64_CONTRIB_LIBS "${RT64_DIR}/build/rt64.a")

    # Static link order matters: rt64.a references the contrib archives, which
    # in turn reference the system libraries. Repeat rt64.a after the contrib
    # set to close the cycle between plume and rt64.
    target_link_libraries(rt64_probe PRIVATE
        ${RT64_DIR}/build/rt64.a
        ${RT64_CONTRIB_LIBS}
        ${RT64_DIR}/build/rt64.a
        SDL2 vulkan X11 zstd dl pthread)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    target_link_libraries(rt64_probe PRIVATE ${GTK3_LIBRARIES})
    target_link_directories(rt64_probe PRIVATE ${GTK3_LIBRARY_DIRS})
endif()
