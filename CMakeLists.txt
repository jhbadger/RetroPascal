cmake_minimum_required(VERSION 3.14)
project(pascal_ide CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Find or build tvision ─────────────────────────────────────────────────
# Option A: tvision is already installed system-wide (e.g. via CMake install)
# Option B: tvision source tree is a subdirectory or sibling directory

# Adjust TVISION_DIR to point at the root of your tvision clone.
# If you cloned it alongside this project:  ../tvision
# If it's a submodule in this project:       tvision
set(TVISION_DIR "${CMAKE_SOURCE_DIR}/../tvision" CACHE PATH
    "Path to the tvision source directory")

if(EXISTS "${TVISION_DIR}/CMakeLists.txt")
    # Build tvision as part of this project
    add_subdirectory("${TVISION_DIR}" tvision_build EXCLUDE_FROM_ALL)
    set(TVISION_INCLUDE_DIRS "${TVISION_DIR}/include")
    set(TVISION_LIBRARIES    tvision)
else()
    # Look for a pre-installed tvision
    find_package(tvision QUIET)
    if(tvision_FOUND)
        set(TVISION_INCLUDE_DIRS "")
        set(TVISION_LIBRARIES    tvision::tvision)
    else()
        message(FATAL_ERROR
            "tvision not found.\n"
            "Clone it: git clone https://github.com/magiblot/tvision\n"
            "Then set -DTVISION_DIR=/path/to/tvision")
    endif()
endif()

# ── pascal_ide executable ─────────────────────────────────────────────────
add_executable(pascal_ide pascal_ide.cpp)

target_include_directories(pascal_ide PRIVATE ${TVISION_INCLUDE_DIRS})
target_link_libraries(pascal_ide PRIVATE ${TVISION_LIBRARIES})

# On Linux, tvision needs pthread
find_package(Threads REQUIRED)
target_link_libraries(pascal_ide PRIVATE Threads::Threads)

# ── Install ───────────────────────────────────────────────────────────────
install(TARGETS pascal_ide DESTINATION bin)
