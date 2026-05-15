cmake_minimum_required(VERSION 3.15)

project(imnodes)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)


# determine whether this is a standalone project or included by other projects
if (NOT DEFINED IMNODES_STANDALONE_PROJECT)
    if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
        set(IMNODES_STANDALONE_PROJECT ON)
    else()
        set(IMNODES_STANDALONE_PROJECT OFF)
    endif ()
endif()

# cmake options
option(IMNODES_EXAMPLES "Build examples and test programs" ${IMNODES_STANDALONE_PROJECT})

# Path to a Dear ImGui source tree (NOT an installed package — we compile imgui
# from source so we can control the IMGUI_ENABLE_TEST_ENGINE flag, and so that
# the imgui version stays in sync with the imgui_test_engine version below.
# Override with -DIMGUI_DIR=/path/to/imgui at configure time.
set(IMGUI_DIR "/usr/local/dearimgui" CACHE PATH "Path to Dear ImGui sources")

# Path to a Dear ImGui Test Engine source tree. Required only if
# IMNODES_TEST_ENGINE is ON (default). Override with -DIMGUI_TEST_ENGINE_DIR=...
set(IMGUI_TEST_ENGINE_DIR "/usr/local/imgui-test-engine"
    CACHE PATH "Path to Dear ImGui Test Engine sources")

option(IMNODES_TEST_ENGINE
    "Build imgui_test_engine and the automated drag/click tests" ON)

if(MSVC)
    add_compile_definitions(SDL_MAIN_HANDLED)
    add_compile_options(/WX)
    # replace existing /W to avoid warning
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        add_compile_options(/W4)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# -----------------------------------------------------------------------------
# Dear ImGui (built from source).
#
# Previously this project consumed imgui via vcpkg. We switched to source build
# because imgui_test_engine requires IMGUI_ENABLE_TEST_ENGINE to be defined
# when imgui itself is compiled (the macro inserts hook calls into widget
# code); vcpkg's binary cache does not let us flip that flag. We always define
# the macro so a single imgui build is reused by all targets — when the test
# engine is not started at runtime, the hooks are cheap no-ops.
# -----------------------------------------------------------------------------

if(NOT DEFINED IMNODES_IMGUI_TARGET_NAME)
    if(NOT EXISTS "${IMGUI_DIR}/imgui.cpp")
        message(FATAL_ERROR
            "IMGUI_DIR='${IMGUI_DIR}' does not contain imgui.cpp. "
            "Set -DIMGUI_DIR=/path/to/imgui or install Dear ImGui sources there.")
    endif()
    # Need SDL2 here (not just inside the examples block) because the imgui
    # SDL2 backend's translation unit includes <SDL.h>.
    find_package(SDL2 CONFIG REQUIRED)

    add_library(imgui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
    target_include_directories(imgui PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends)
    target_link_libraries(imgui PUBLIC SDL2::SDL2)
    if(IMNODES_TEST_ENGINE)
        target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_TEST_ENGINE)
    endif()
    # The imgui sources predate (or simply don't care about) the warning
    # policy we apply to the rest of the project. We trust upstream and
    # silence -Werror just for this target.
    if(NOT MSVC)
        target_compile_options(imgui PRIVATE -w)
    endif()
    add_library(imgui::imgui ALIAS imgui)
    set(IMNODES_IMGUI_TARGET_NAME imgui::imgui)
endif()

# -----------------------------------------------------------------------------
# Dear ImGui Test Engine (built from source, but unconditionally if the option
# is on — because when IMGUI_ENABLE_TEST_ENGINE is defined on imgui, imgui.cpp
# emits references to ImGuiTestEngineHook_* / ImGuiTestEngine_FindItemDebugLabel
# that live in this library. Every executable linking imgui therefore must
# also link this library, even if it never starts the engine at runtime — the
# hooks check whether an engine is active and become cheap no-ops otherwise.
# -----------------------------------------------------------------------------

if(IMNODES_TEST_ENGINE)
    if(NOT EXISTS "${IMGUI_TEST_ENGINE_DIR}/imgui_test_engine/imgui_te_engine.cpp")
        message(FATAL_ERROR
            "IMGUI_TEST_ENGINE_DIR='${IMGUI_TEST_ENGINE_DIR}' does not "
            "contain imgui_test_engine sources. Set "
            "-DIMGUI_TEST_ENGINE_DIR=/path/to/imgui_test_engine or "
            "configure with -DIMNODES_TEST_ENGINE=OFF to skip.")
    endif()

    add_library(imgui_test_engine STATIC
        ${IMGUI_TEST_ENGINE_DIR}/imgui_test_engine/imgui_te_engine.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_test_engine/imgui_te_context.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_test_engine/imgui_te_coroutine.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_test_engine/imgui_te_exporters.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_test_engine/imgui_te_perftool.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_test_engine/imgui_te_ui.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_test_engine/imgui_te_utils.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_test_engine/imgui_capture_tool.cpp)
    target_include_directories(imgui_test_engine PUBLIC
        ${IMGUI_TEST_ENGINE_DIR})
    target_compile_definitions(imgui_test_engine PUBLIC
        # Use std::thread for the coroutine backing — avoids hand-wiring a
        # coroutine implementation into our app.
        IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL=1
        # Use std::function for ImGuiTest::TestFunc etc., so we can hand it
        # lambdas with captures.
        IMGUI_TEST_ENGINE_ENABLE_STD_FUNCTION=1
        # Disable the bundled screen-capture / video pipeline. We capture
        # via the same libpng path render_test uses.
        IMGUI_TEST_ENGINE_ENABLE_CAPTURE=0)
    target_link_libraries(imgui_test_engine PUBLIC imgui::imgui)
    if(NOT MSVC)
        target_compile_options(imgui_test_engine PRIVATE -w)
    endif()
endif()

# -----------------------------------------------------------------------------
# Imnodes
# -----------------------------------------------------------------------------

add_library(imnodes)
target_sources(imnodes PRIVATE
    imnodes.h
    imnodes_internal.h
    imnodes.cpp)
target_include_directories(imnodes PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(imnodes PUBLIC ${IMNODES_IMGUI_TARGET_NAME})
# See test_engine block above: imgui's hook symbol references mean every imgui
# consumer must also pull in imgui_test_engine. Putting the link here means
# every example exe gets it automatically by linking imnodes.
if(IMNODES_TEST_ENGINE)
    target_link_libraries(imnodes PUBLIC imgui_test_engine)
endif()

# -----------------------------------------------------------------------------
# Examples and tests
# -----------------------------------------------------------------------------

if(IMNODES_EXAMPLES)
    # SDL2 was already found above (needed by imgui's SDL2 backend), so no
    # redundant find_package here.

    # Per-platform GL linkage used by every executable below.
    function(_imnodes_link_gl target)
        if(APPLE)
            target_link_libraries(${target} PRIVATE "-framework OpenGL")
        elseif(MSVC)
            target_link_libraries(${target} PRIVATE "opengl32")
        else()
            target_link_libraries(${target} PRIVATE X11 Xext GL)
        endif()
    endfunction()

    foreach(example_name colornode multieditor saveload hello)
        if(example_name STREQUAL "colornode")
            set(_src ${CMAKE_CURRENT_SOURCE_DIR}/example/color_node_editor.cpp)
        elseif(example_name STREQUAL "multieditor")
            set(_src ${CMAKE_CURRENT_SOURCE_DIR}/example/multi_editor.cpp)
        elseif(example_name STREQUAL "saveload")
            set(_src ${CMAKE_CURRENT_SOURCE_DIR}/example/save_load.cpp)
        elseif(example_name STREQUAL "hello")
            set(_src ${CMAKE_CURRENT_SOURCE_DIR}/example/hello.cpp)
        endif()
        add_executable(${example_name}
            ${CMAKE_CURRENT_SOURCE_DIR}/imnodes.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/example/main.cpp
            ${_src})
        target_link_libraries(${example_name} PRIVATE imnodes SDL2::SDL2)
        _imnodes_link_gl(${example_name})
    endforeach()

    # Headless / interactive render test. Renders a small node graph and writes
    # a PNG, so agents can iterate on imnodes visuals without needing an
    # interactive window.
    find_package(PNG REQUIRED)
    add_executable(render_test
        ${CMAKE_CURRENT_SOURCE_DIR}/imnodes.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test/render_test.cpp)
    target_link_libraries(render_test PRIVATE imnodes SDL2::SDL2 PNG::PNG)
    _imnodes_link_gl(render_test)

    # Automated drag test using imgui_test_engine.
    if(IMNODES_TEST_ENGINE)
        add_executable(node_drag_test
            ${CMAKE_CURRENT_SOURCE_DIR}/imnodes.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/node_drag_test.cpp)
        target_link_libraries(node_drag_test PRIVATE
            imnodes SDL2::SDL2 PNG::PNG)
        _imnodes_link_gl(node_drag_test)
    endif()
endif()
