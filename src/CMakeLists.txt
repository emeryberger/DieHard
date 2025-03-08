cmake_minimum_required(VERSION 3.15)
project(DieHard LANGUAGES CXX)

#
# ─── OPTIONS ──────────────────────────────────────────────────────────
#

# By default, build both diehard (traditional) and dieharder
option(BUILD_DIEHARDER     "Enable building the dieharder library"       ON)
option(BUILD_REPLICATED    "Enable building replicated variant of dieharder" OFF)

# Default to C++14 (adjust as desired)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate position-independent code; required for shared libraries on many platforms
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

#
# ─── INCLUDE DIRECTORIES ──────────────────────────────────────────────
#
# These mimic the original -I flags. Update as needed.
#

include_directories(
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/include/layers
  ${PROJECT_SOURCE_DIR}/include/util
  ${PROJECT_SOURCE_DIR}/include/math
  ${PROJECT_SOURCE_DIR}/include/static
  ${PROJECT_SOURCE_DIR}/include/rng
  ${PROJECT_SOURCE_DIR}/include/hoard
  ${PROJECT_SOURCE_DIR}/include/superblocks
  ${PROJECT_SOURCE_DIR}/Heap-Layers
)

#
# ─── FETCH OR PROVIDE HEAP-LAYERS (OPTIONAL EXAMPLE) ─────────────────
#
# If you wish CMake to automatically fetch the Heap-Layers dependency,
# uncomment the FetchContent logic below. Otherwise, ensure you have
# cloned Heap-Layers in the correct location.
#
# include(FetchContent)
# FetchContent_Declare(
#   HeapLayers
#   GIT_REPOSITORY https://github.com/emeryberger/Heap-Layers.git
#   GIT_TAG        main
# )
# FetchContent_MakeAvailable(HeapLayers)
#
# If Heap-Layers is purely header-only for your use-case, you can skip
# add_subdirectory and just include its path (as above).
#

#
# ─── SOURCE FILES ─────────────────────────────────────────────────────
#
# The Makefile differentiated between UNIX and macOS wrapper sources.
# We do so here with a simple platform check. If the code itself handles
# the needed #ifdef logic, you can consolidate further.
#

set(UNIX_SOURCES
  Heap-Layers/wrappers/gnuwrapper.cpp
  source/libdieharder.cpp
)

set(MACOS_SOURCES
  Heap-Layers/wrappers/macwrapper.cpp
  source/libdieharder.cpp
)

if(APPLE)
  set(DIEHARD_SOURCES ${MACOS_SOURCES})
else()
  set(DIEHARD_SOURCES ${UNIX_SOURCES})
endif()

#
# ─── DIEHARD LIBRARY (DIEHARD_DIEHARDER=0) ────────────────────────────
#

add_library(diehard SHARED ${DIEHARD_SOURCES})
target_compile_definitions(diehard
  PRIVATE
    DIEHARD_DIEHARDER=0        # Original Makefile used 0 for DieHard
    DIEHARD_MULTITHREADED=1
    _REENTRANT=1
)
# Link with pthread/dl on Unix-like systems; on Windows, CMake will adjust automatically
target_link_libraries(diehard PRIVATE pthread dl)
# Output the final library name as libdiehard.so/.dylib on most platforms
set_target_properties(diehard PROPERTIES
  OUTPUT_NAME "diehard"
)

#
# ─── DIEHARDER LIBRARY (DIEHARD_DIEHARDER=1) ─────────────────────────
#

if(BUILD_DIEHARDER)
  add_library(dieharder SHARED ${DIEHARD_SOURCES})
  target_compile_definitions(dieharder
    PRIVATE
      DIEHARD_DIEHARDER=1
      DIEHARD_MULTITHREADED=1
      _REENTRANT=1
  )
  target_link_libraries(dieharder PRIVATE pthread dl)
  set_target_properties(dieharder PROPERTIES
    OUTPUT_NAME "dieharder"
  )
endif()

#
# ─── REPLICATED DIEHARDER (libdieharder_r) ───────────────────────────
#
# This is only built if requested and if BUILD_DIEHARDER is on.
#

if(BUILD_REPLICATED AND BUILD_DIEHARDER)
  set(REPLICATED_SRC replicated/replicated.cpp)
  add_library(dieharder_r SHARED ${DIEHARD_SOURCES} ${REPLICATED_SRC})
  target_compile_definitions(dieharder_r
    PRIVATE
      DIEHARD_DIEHARDER=1
      DIEHARD_MULTITHREADED=1
      DIEHARD_REPLICATED=1
      _REENTRANT=1
  )
  target_link_libraries(dieharder_r PRIVATE pthread dl)
  set_target_properties(dieharder_r PROPERTIES
    OUTPUT_NAME "dieharder_r"
  )
endif()

#
# ─── USAGE ────────────────────────────────────────────────────────────
#
# Typical usage:
#   mkdir build && cd build
#   cmake -DCMAKE_BUILD_TYPE=Release ..
#   cmake --build .
#
# If you wish to enable replicated mode:
#   cmake -DBUILD_REPLICATED=ON ..
#
# You now have libdiehard, optionally libdieharder, and optionally
# libdieharder_r, as shared libraries.
#
# For Debug builds:
#   cmake -DCMAKE_BUILD_TYPE=Debug ..
#
# Adjust flags, definitions, or compiler features above as needed.
#
# ──────────────────────────────────────────────────────────────────────
