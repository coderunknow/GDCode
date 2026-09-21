# GDCode compiler core.
#
# Pure, portable C++ with ZERO Geode / Geometry Dash dependencies. Everything
# in this library is unit-testable on any host platform, which is how compiler
# correctness is verified (sandbox + CI) even though the actual game binary can
# only be exercised inside Geometry Dash itself.

file(GLOB GDCORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(gdcode_core STATIC ${GDCORE_SOURCES})

target_include_directories(gdcode_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(gdcode_core PUBLIC cxx_std_20)
# The core is linked into the mod's shared library on every platform.
set_target_properties(gdcode_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if (MSVC)
    target_compile_options(gdcode_core PRIVATE /W4)
else()
    target_compile_options(gdcode_core PRIVATE -Wall -Wextra)
endif()
