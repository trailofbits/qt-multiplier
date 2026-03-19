#
# dependencies/qt/CMakeLists.txt
#
# Standalone CMake configuration for building Qt 6 from source.
#
# Can be used in two ways:
#
# 1. As part of the qt-multiplier superbuild (automatic):
#    The main CMakeLists.txt sets up an ExternalProject when
#    MXQT_USE_SYSTEM_QT=OFF and Qt is not yet built.
#
# 2. Standalone (manual):
#    cmake -B build-qt -S dependencies/qt \
#          -DMXQT_QT_VERSION=v6.8.2 \
#          -DMXQT_QT_INSTALL_PREFIX=/path/to/qt-install
#    cmake --build build-qt
#
# The build process:
#   1. Clones Qt from https://github.com/qt/qt5 (qt5 repo contains Qt 6)
#   2. Configures with -submodules to only build what we need
#   3. Builds using Ninja
#   4. Installs to the specified prefix
#
# Requirements:
#   - Git, CMake 3.19+, Ninja, Python 3
#   - C++17 compiler (Clang recommended)
#

cmake_minimum_required(VERSION 3.19)

project(QtBuild
    DESCRIPTION "Qt 6 build from source for qt-multiplier"
    LANGUAGES C CXX
)

# ==============================================================================
# Options
# ==============================================================================

set(MXQT_QT_VERSION "v6.8.2" CACHE STRING
    "Qt version tag to checkout (e.g., v6.8.2, v6.7.0)"
)

set(MXQT_QT_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/qt-install" CACHE PATH
    "Directory where Qt will be installed"
)

# qt-multiplier needs: Core, Gui, Widgets, Concurrent, Core5Compat, Test
# These come from: qtbase (Core, Gui, Widgets, Concurrent, Test) + qt5compat (Core5Compat)
set(MXQT_QT_MODULES
    "qtbase"
    "qt5compat"
    CACHE STRING
    "List of Qt submodules to build"
)

set(MXQT_QT_BUILD_TYPE "Release" CACHE STRING
    "Build type for Qt (Release, Debug, RelWithDebInfo)"
)

set(MXQT_QT_EXTRA_CONFIGURE_ARGS "" CACHE STRING
    "Additional arguments to pass to Qt's configure script"
)

set(MXQT_QT_PARALLEL_JOBS 0 CACHE STRING
    "Number of parallel build jobs (0 = auto-detect)"
)

# ==============================================================================
# Find Required Programs
# ==============================================================================

find_program(GIT_EXECUTABLE git REQUIRED)
find_program(NINJA_EXECUTABLE ninja REQUIRED)
find_program(PYTHON_EXECUTABLE NAMES python3 python REQUIRED)

message(STATUS "QtBuild: Git:    ${GIT_EXECUTABLE}")
message(STATUS "QtBuild: Ninja:  ${NINJA_EXECUTABLE}")
message(STATUS "QtBuild: Python: ${PYTHON_EXECUTABLE}")

# ==============================================================================
# Determine Parallel Jobs
# ==============================================================================

if(MXQT_QT_PARALLEL_JOBS GREATER 0)
    set(PARALLEL_JOBS ${MXQT_QT_PARALLEL_JOBS})
else()
    include(ProcessorCount)
    ProcessorCount(PARALLEL_JOBS)
    if(PARALLEL_JOBS EQUAL 0)
        set(PARALLEL_JOBS 4)
    endif()
endif()

message(STATUS "QtBuild: Using ${PARALLEL_JOBS} parallel jobs")

# ==============================================================================
# Directory Setup
# ==============================================================================

set(QT_SOURCE_DIR "${CMAKE_BINARY_DIR}/qt-src")
set(QT_BUILD_DIR "${CMAKE_BINARY_DIR}/qt-build")
set(QT_INSTALL_DIR "${MXQT_QT_INSTALL_PREFIX}")

# ==============================================================================
# Build Configure Arguments
# ==============================================================================

set(QT_CONFIGURE_ARGS
    "-prefix" "${QT_INSTALL_DIR}"
    "-opensource"
    "-confirm-license"
    "-init-submodules"
    "-nomake" "examples"
    "-nomake" "tests"
)

# Build type mapping
if(MXQT_QT_BUILD_TYPE STREQUAL "Debug")
    list(APPEND QT_CONFIGURE_ARGS "-debug")
elseif(MXQT_QT_BUILD_TYPE STREQUAL "RelWithDebInfo")
    list(APPEND QT_CONFIGURE_ARGS "-release" "-force-debug-info")
else()
    list(APPEND QT_CONFIGURE_ARGS "-release")
endif()

# Submodules specification (Qt 6.7+ feature)
list(JOIN MXQT_QT_MODULES "," QT_MODULE_STRING)
list(APPEND QT_CONFIGURE_ARGS "-submodules" "${QT_MODULE_STRING}")

# Extra user-specified arguments
if(MXQT_QT_EXTRA_CONFIGURE_ARGS)
    foreach(ARG ${MXQT_QT_EXTRA_CONFIGURE_ARGS})
        list(APPEND QT_CONFIGURE_ARGS "${ARG}")
    endforeach()
endif()

# CMake generator and compiler passthrough
list(APPEND QT_CONFIGURE_ARGS "--" "-GNinja")

# Pass compiler if explicitly set
if(CMAKE_C_COMPILER)
    list(APPEND QT_CONFIGURE_ARGS "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}")
endif()
if(CMAKE_CXX_COMPILER)
    list(APPEND QT_CONFIGURE_ARGS "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
endif()

# macOS-specific options
if(APPLE)
    if(CMAKE_OSX_ARCHITECTURES)
        list(APPEND QT_CONFIGURE_ARGS "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}")
    endif()

    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        list(APPEND QT_CONFIGURE_ARGS "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    else()
        list(APPEND QT_CONFIGURE_ARGS "-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0")
    endif()
endif()

# ==============================================================================
# External Project Definition
# ==============================================================================

include(ExternalProject)

ExternalProject_Add(qt6
    PREFIX "${CMAKE_BINARY_DIR}/qt-prefix"

    # Git clone
    GIT_REPOSITORY "https://github.com/qt/qt5.git"
    GIT_TAG "${MXQT_QT_VERSION}"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    # Don't init submodules - Qt's configure -submodules handles this
    GIT_SUBMODULES ""

    # Directories
    SOURCE_DIR "${QT_SOURCE_DIR}"
    BINARY_DIR "${QT_BUILD_DIR}"
    INSTALL_DIR "${QT_INSTALL_DIR}"

    # The configure script with -submodules handles submodule init in Qt 6.7+
    UPDATE_COMMAND ""

    # Configure
    CONFIGURE_COMMAND "${QT_SOURCE_DIR}/configure" ${QT_CONFIGURE_ARGS}

    # Build
    BUILD_COMMAND ${CMAKE_COMMAND} --build . --parallel ${PARALLEL_JOBS}

    # Install
    INSTALL_COMMAND ${CMAKE_COMMAND} --install .

    # Logging
    LOG_DOWNLOAD TRUE
    LOG_UPDATE TRUE
    LOG_CONFIGURE TRUE
    LOG_BUILD TRUE
    LOG_INSTALL TRUE
    LOG_OUTPUT_ON_FAILURE TRUE

    # No timeout for large downloads
    TIMEOUT 0

    # Don't rebuild every time
    BUILD_ALWAYS FALSE
)

# ==============================================================================
# Convenience Targets
# ==============================================================================

add_custom_target(qt6-download
    COMMAND ${CMAKE_COMMAND} --build . --target qt6
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Downloading Qt sources..."
)

# ==============================================================================
# Output Configuration File
# ==============================================================================

set(QT_CONFIG_FILE "${CMAKE_BINARY_DIR}/qt6-config.cmake")

file(WRITE "${QT_CONFIG_FILE}"
"# Qt 6 Configuration
# Generated by qt-multiplier Qt vendor build
# Use with: cmake -DQt6_DIR=\${MXQT_BUILT_QT6_DIR} ...

set(MXQT_BUILT_QT6_DIR \"${QT_INSTALL_DIR}/lib/cmake/Qt6\")
set(MXQT_BUILT_QT6_VERSION \"${MXQT_QT_VERSION}\")
set(MXQT_BUILT_QT6_PREFIX \"${QT_INSTALL_DIR}\")

list(APPEND CMAKE_PREFIX_PATH \"\${MXQT_BUILT_QT6_PREFIX}\")
")

# ==============================================================================
# Print Summary
# ==============================================================================

message(STATUS "")
message(STATUS "=============================================================")
message(STATUS "Qt 6 Build Configuration")
message(STATUS "=============================================================")
message(STATUS "  Version:      ${MXQT_QT_VERSION}")
message(STATUS "  Build Type:   ${MXQT_QT_BUILD_TYPE}")
message(STATUS "  Source:       ${QT_SOURCE_DIR}")
message(STATUS "  Build:        ${QT_BUILD_DIR}")
message(STATUS "  Install:      ${QT_INSTALL_DIR}")
message(STATUS "  Modules:      ${QT_MODULE_STRING}")
message(STATUS "  Parallel:     ${PARALLEL_JOBS} jobs")
message(STATUS "")
message(STATUS "Build Qt with:")
message(STATUS "  cmake --build ${CMAKE_BINARY_DIR}")
message(STATUS "")
message(STATUS "After build, use Qt with:")
message(STATUS "  cmake -DQt6_DIR=${QT_INSTALL_DIR}/lib/cmake/Qt6")
message(STATUS "=============================================================")
message(STATUS "")
