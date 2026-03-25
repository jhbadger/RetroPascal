cmake_minimum_required(VERSION 3.14)
project(pascal_ide CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Build type: default to Release ────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Build type: Debug | Release | RelWithDebInfo | MinSizeRel" FORCE)
    message(STATUS "Build type not set — defaulting to Release")
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ── Optimisation & stripping flags ────────────────────────────────────────
# Release: -O2, strip all symbols from the final binary
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    # Compiler: optimise aggressively
    add_compile_options(-O2)

    # Linker: strip debug symbols from the installed/final binary
    if(APPLE)
        # macOS: -dead_strip removes unused code; strip -S removes debug symbols
        add_link_options(-Wl,-dead_strip)
        # Post-link strip is done via CMAKE_INSTALL_STRIP (see below) or a
        # custom command; ld on macOS doesn't accept --strip-all.
    else()
        # Linux / Termux: --strip-all removes all symbols
        add_link_options(-Wl,--strip-all)
    endif()
endif()

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

# ── SDL2 detection (optional — graphics support) ─────────────────────────
# Try multiple methods so it works on macOS (Homebrew), Linux, and Termux.
set(PASCAL_HAS_SDL2 FALSE)

# Method 1: sdl2-config script (Homebrew on macOS, manual installs)
find_program(SDL2_CONFIG sdl2-config)
if(SDL2_CONFIG)
    execute_process(COMMAND ${SDL2_CONFIG} --cflags
        OUTPUT_VARIABLE SDL2_RAW_CFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${SDL2_CONFIG} --libs
        OUTPUT_VARIABLE SDL2_RAW_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${SDL2_CONFIG} --prefix
        OUTPUT_VARIABLE SDL2_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(SDL2_RAW_CFLAGS AND SDL2_RAW_LIBS)
        message(STATUS "SDL2 found via sdl2-config — building with graphics support")
        message(STATUS "  SDL2 prefix: ${SDL2_PREFIX}")
        message(STATUS "  SDL2 cflags: ${SDL2_RAW_CFLAGS}")
        set(PASCAL_HAS_SDL2 TRUE)
        set(PASCAL_SDL2_METHOD "sdl2-config")
        # Extract include directories from cflags (-I/some/path)
        string(REGEX MATCHALL "-I([^ ]+)" SDL2_INCLUDE_MATCHES "${SDL2_RAW_CFLAGS}")
        set(SDL2_INCLUDE_DIRS_LIST "")
        foreach(m ${SDL2_INCLUDE_MATCHES})
            string(REGEX REPLACE "^-I" "" inc "${m}")
            list(APPEND SDL2_INCLUDE_DIRS_LIST "${inc}")
        endforeach()
        # Also add prefix/include as a fallback
        if(SDL2_PREFIX)
            list(APPEND SDL2_INCLUDE_DIRS_LIST "${SDL2_PREFIX}/include")
        endif()
        set(SDL2_INCLUDE_DIRS_FROM_CONFIG "${SDL2_INCLUDE_DIRS_LIST}")
        set(SDL2_LDFLAGS_FROM_CONFIG "${SDL2_RAW_LIBS}")
    endif()
endif()

# Method 2: CMake find_package
if(NOT PASCAL_HAS_SDL2)
    find_package(SDL2 QUIET)
    if(SDL2_FOUND OR TARGET SDL2::SDL2)
        message(STATUS "SDL2 found via find_package — building with graphics support")
        set(PASCAL_HAS_SDL2 TRUE)
        set(SDL2_CFLAGS "")
        set(SDL2_LDFLAGS "")
    endif()
endif()

# Method 3: pkg-config (Linux, Termux)
if(NOT PASCAL_HAS_SDL2)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2PC QUIET sdl2)
        if(SDL2PC_FOUND)
            message(STATUS "SDL2 found via pkg-config — building with graphics support")
            set(PASCAL_HAS_SDL2 TRUE)
            set(SDL2_CFLAGS "${SDL2PC_CFLAGS}")
            set(SDL2_LDFLAGS "${SDL2PC_LDFLAGS}")
        endif()
    endif()
endif()

if(NOT PASCAL_HAS_SDL2)
    message(STATUS "SDL2 not found — building pascal interpreter without graphics support")
endif()

# ── pascal interpreter ────────────────────────────────────────────────────
add_executable(pascal pascal_interpreter.cpp pascal_graphics.h)

if(PASCAL_HAS_SDL2)
    target_compile_definitions(pascal PRIVATE HAVE_SDL2)
    if(TARGET SDL2::SDL2)
        # CMake imported target (best case)
        target_link_libraries(pascal PRIVATE SDL2::SDL2)
    elseif(SDL2_FOUND AND SDL2_INCLUDE_DIRS)
        # find_package result without imported target
        target_include_directories(pascal PRIVATE ${SDL2_INCLUDE_DIRS})
        target_link_libraries(pascal PRIVATE ${SDL2_LIBRARIES})
    elseif(SDL2PC_FOUND)
        # pkg-config result
        target_include_directories(pascal PRIVATE ${SDL2PC_INCLUDE_DIRS})
        target_link_libraries(pascal PRIVATE ${SDL2PC_LINK_LIBRARIES})
    else()
        # sdl2-config path: use extracted include dirs and raw link flags
        message(STATUS "Applying SDL2 include dirs: ${SDL2_INCLUDE_DIRS_FROM_CONFIG}")
        target_include_directories(pascal PRIVATE ${SDL2_INCLUDE_DIRS_FROM_CONFIG})
        separate_arguments(SDL2_LDFLAGS_LIST UNIX_COMMAND "${SDL2_LDFLAGS_FROM_CONFIG}")
        target_link_options(pascal PRIVATE ${SDL2_LDFLAGS_LIST})
    endif()
endif()

# ── pascal_ide executable ─────────────────────────────────────────────────
add_executable(pascal_ide pascal_ide.cpp)

target_include_directories(pascal_ide PRIVATE ${TVISION_INCLUDE_DIRS})
target_link_libraries(pascal_ide PRIVATE ${TVISION_LIBRARIES})

# On Linux, tvision needs pthread
find_package(Threads REQUIRED)
target_link_libraries(pascal_ide PRIVATE Threads::Threads)

# ── Copy pascal next to pascal_ide after build ────────────────────────────
# The IDE looks for ./pascal in the same directory as itself.
add_custom_command(TARGET pascal_ide POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:pascal>
        $<TARGET_FILE_DIR:pascal_ide>/pascal
    COMMENT "Copying pascal interpreter next to pascal_ide"
)

# ── Install ───────────────────────────────────────────────────────────────
install(TARGETS pascal pascal_ide DESTINATION bin)

# Strip installed binaries (works on all platforms via cmake --install --strip)
set(CMAKE_INSTALL_STRIP TRUE)

# On macOS, also strip the built binary in-place after linking in Release mode
# (since ld doesn't support --strip-all, we run `strip` manually)
if(APPLE AND (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel"))
    add_custom_command(TARGET pascal POST_BUILD
        COMMAND strip -S $<TARGET_FILE:pascal>
        COMMENT "Stripping debug symbols from pascal")
    add_custom_command(TARGET pascal_ide POST_BUILD
        COMMAND strip -S $<TARGET_FILE:pascal_ide>
        COMMENT "Stripping debug symbols from pascal_ide")
endif()
