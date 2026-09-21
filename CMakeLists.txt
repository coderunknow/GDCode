cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(GDCode VERSION 0.2.0)

# Embed the whole prompt in the binary: players need neither repo files nor a
# network connection. Reconfigure automatically when the canonical text changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS docs/AI_CODING_PROMPT.md)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/docs/AI_CODING_PROMPT.md" GDCODE_AI_PROMPT)
configure_file(src/content/AiCodingPrompt.hpp.in generated/AiCodingPrompt.hpp @ONLY)


# ----------------------------------------------------------------------------
# Two build modes, one tree:
#
#   * Mod mode (GEODE_SDK set, i.e. `geode build` or CI): builds the Geode mod
#     from src/*.cpp and links the compiler core into it.
#   * Sandbox mode (no GEODE_SDK): builds only the compiler core, its
#     test-suite and a small CLI. This is how the compiler is verified on any
#     host without the game.
# ----------------------------------------------------------------------------
set(GDCODE_MOD_MODE OFF)
if (DEFINED ENV{GEODE_SDK})
    set(GDCODE_MOD_MODE ON)
endif()

if (GDCODE_MOD_MODE)
    message(STATUS "GDCode: Geode SDK found at $ENV{GEODE_SDK} -- building the mod")

    file(GLOB_RECURSE MOD_SOURCES CONFIGURE_DEPENDS src/*.cpp)
    add_library(${PROJECT_NAME} SHARED ${MOD_SOURCES})
    target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

    add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

    # The compiler core (pure C++, zero Geode / GD dependencies) is added after
    # the SDK on purpose: the SDK sets platform settings in this scope (e.g. the
    # iOS deployment target) that the core's objects must be built with too.
    add_subdirectory(core)
    # Plain (keyword-less) signature on purpose: setup_geode_mod() links
    # geode-sdk with the plain signature and CMake forbids mixing the two forms
    # on one target.
    target_link_libraries(${PROJECT_NAME} gdcode_core)

    setup_geode_mod(${PROJECT_NAME})
else()
    message(STATUS "GDCode: GEODE_SDK not set -- sandbox mode (core + tests + cli)")

    # Compiler core: pure C++, zero Geode / GD dependencies.
    add_subdirectory(core)

    option(GDCODE_BUILD_TESTS "Build the compiler core test-suite" ON)
    if (GDCODE_BUILD_TESTS)
        enable_testing()
        add_subdirectory(tests)
    endif()

    add_executable(gdcode-cli cli/main.cpp)
    target_link_libraries(gdcode-cli PRIVATE gdcode_core)
endif()
