cmake_minimum_required(VERSION 3.15)
project(DieHard LANGUAGES CXX)

#
# ─── OPTIONS ──────────────────────────────────────────────────────────
#

set(CMAKE_OSX_ARCHITECTURES arm64 arm64e x86_64)

# By default, build both diehard (traditional) and dieharder
option(BUILD_DIEHARDER     "Enable building the DieHarder library"       ON)
option(BUILD_REPLICATED    "Enable building replicated variant of DieHarder" OFF)

# Write a version map to enable replacing direct libc allocation calls.
set(VERS_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/vers.map)
file(WRITE ${VERS_SCRIPT}
"GLIBC_2.2.5 {
global:
    __libc_malloc;
    __libc_free;
    __libc_realloc;
    __libc_calloc;
    __libc_memalign;
    malloc;
    malloc_usable_size;
    cfree;
    free;
    realloc;
    calloc;
    reallocarray;
    memalign;
    aligned_alloc;
    valloc;
    pvalloc;
    posix_memalign;
    strdup;
    strndup;
local:
    *;
  };
")

# Default to C++14 (adjust as desired)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure that the compiler doesn't replace anything with malloc/free
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden -fno-builtin-malloc -fno-builtin-free")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -fno-builtin-malloc -fno-builtin-free")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-builtin-malloc -fno-builtin-free")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-builtin-malloc -fno-builtin-free")

# Generate position-independent code; required for shared libraries on many platforms
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

#
# ─── INCLUDE DIRECTORIES ──────────────────────────────────────────────
#

include_directories(
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/src/include
  ${PROJECT_SOURCE_DIR}/src/include/layers
  ${PROJECT_SOURCE_DIR}/src/include/util
  ${PROJECT_SOURCE_DIR}/src/include/math
  ${PROJECT_SOURCE_DIR}/src/include/static
  ${PROJECT_SOURCE_DIR}/src/include/rng
  ${PROJECT_SOURCE_DIR}/src/include/hoard
  ${PROJECT_SOURCE_DIR}/src/include/superblocks
)

include(FetchContent)
FetchContent_Declare(
  Heap-Layers
  GIT_REPOSITORY https://github.com/emeryberger/Heap-Layers.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(Heap-Layers)
include_directories(${heap-layers_SOURCE_DIR})


include(FetchContent)
FetchContent_Declare(
  printf
  GIT_REPOSITORY https://github.com/emeryberger/printf.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(printf)
include_directories(${printf_SOURCE_DIR})

set(UNIX_SOURCES
  ${heap-layers_SOURCE_DIR}/wrappers/gnuwrapper.cpp
  src/source/libdieharder.cpp
  ${printf_SOURCE_DIR}/printf.cpp
)

set(MACOS_SOURCES
  ${heap-layers_SOURCE_DIR}/wrappers/macwrapper.cpp
  src/source/libdieharder.cpp
  ${printf_SOURCE_DIR}/printf.cpp
)

if(APPLE)
  set(DIEHARD_SOURCES ${MACOS_SOURCES})
else()
  set(DIEHARD_SOURCES ${UNIX_SOURCES})
endif()

set(CMAKE_BUILD_TYPE RelWithDebInfo)


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

# For Linux, link with the version script declared above.
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_options(diehard PRIVATE "LINKER:--version-script=${VERS_SCRIPT}")
  target_link_options(dieharder PRIVATE "LINKER:--version-script=${VERS_SCRIPT}")
endif()

install(TARGETS diehard dieharder
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

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
