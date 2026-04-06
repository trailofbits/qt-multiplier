#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

#
# Qt vendor build support for qt-multiplier.
#
# Supports two modes:
#
# 1. System Qt (MXQT_USE_SYSTEM_QT=ON, default):
#    Uses find_package() to locate an existing Qt installation.
#
# 2. Vendor from source (MXQT_USE_SYSTEM_QT=OFF):
#    Clones and builds Qt from the official GitHub mirror.
#    Uses a "superbuild" pattern:
#      First cmake invocation:  Downloads and builds Qt (MXQT_QT_AVAILABLE=FALSE)
#      Second cmake invocation: Configures qt-multiplier with the built Qt
#

include(ExternalProject)

# ==============================================================================
# mxqt_setup_qt
# ==============================================================================
# Main entry point. Call from the root CMakeLists.txt before any targets
# that depend on Qt.
#
# After this function returns:
#   - MXQT_QT_AVAILABLE is TRUE if Qt is ready to use
#   - If TRUE, Qt6::* targets are available for linking
#
function(mxqt_setup_qt)
    if(MXQT_USE_SYSTEM_QT)
        _mxqt_find_system_qt()
    else()
        _mxqt_setup_qt_from_source()
    endif()

    # Propagate variables set by the inner functions to the caller's scope
    set(MXQT_QT_AVAILABLE "${MXQT_QT_AVAILABLE}" PARENT_SCOPE)
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
    set(CMAKE_AUTOMOC "${CMAKE_AUTOMOC}" PARENT_SCOPE)
    set(CMAKE_AUTOUIC "${CMAKE_AUTOUIC}" PARENT_SCOPE)
    set(CMAKE_AUTORCC "${CMAKE_AUTORCC}" PARENT_SCOPE)
endfunction()

# ==============================================================================
# _mxqt_find_system_qt
# ==============================================================================
function(_mxqt_find_system_qt)
    message(STATUS "qt-multiplier: Looking for system Qt installation...")

    find_package(Qt6 REQUIRED COMPONENTS
        Core Gui Widgets Concurrent Core5Compat Sql Test
    )

    if(Qt6_FOUND)
        message(STATUS "qt-multiplier: Found Qt ${Qt6_VERSION} at ${Qt6_DIR}")
        set(MXQT_QT_AVAILABLE TRUE PARENT_SCOPE)
    else()
        message(FATAL_ERROR
            "qt-multiplier: Qt 6 not found. Install Qt 6, set Qt6_DIR, or set "
            "MXQT_USE_SYSTEM_QT=OFF to build from source."
        )
    endif()
endfunction()

# ==============================================================================
# _mxqt_setup_qt_from_source
# ==============================================================================
function(_mxqt_setup_qt_from_source)
    set(MXQT_QT_SOURCE_DIR "${CMAKE_BINARY_DIR}/qt-src" CACHE PATH "Qt source directory")
    set(MXQT_QT_BUILD_DIR "${CMAKE_BINARY_DIR}/qt-build" CACHE PATH "Qt build directory")
    set(MXQT_QT_INSTALL_DIR "${CMAKE_BINARY_DIR}/qt-install" CACHE PATH "Qt install directory")

    set(QT_CMAKE_DIR "${MXQT_QT_INSTALL_DIR}/lib/cmake/Qt6")

    if(EXISTS "${QT_CMAKE_DIR}/Qt6Config.cmake")
        message(STATUS "qt-multiplier: Found pre-built vendored Qt at ${MXQT_QT_INSTALL_DIR}")

        # Point find_package at the vendored Qt
        set(Qt6_DIR "${QT_CMAKE_DIR}" CACHE PATH "Path to Qt6Config.cmake" FORCE)

        # Add to prefix path so all Qt component find_package calls work
        list(PREPEND CMAKE_PREFIX_PATH "${MXQT_QT_INSTALL_DIR}")
        set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)

        find_package(Qt6 REQUIRED COMPONENTS
            Core Gui Widgets Concurrent Core5Compat Sql Test
        )

        if(Qt6_FOUND)
            message(STATUS "qt-multiplier: Using vendored Qt ${Qt6_VERSION}")
            set(MXQT_QT_AVAILABLE TRUE PARENT_SCOPE)

            set(CMAKE_AUTOMOC ON PARENT_SCOPE)
            set(CMAKE_AUTOUIC ON PARENT_SCOPE)
            set(CMAKE_AUTORCC ON PARENT_SCOPE)
        endif()
    else()
        message(STATUS "qt-multiplier: Qt not yet built, configuring vendor build from source...")
        _mxqt_configure_qt_external_project()
        set(MXQT_QT_AVAILABLE FALSE PARENT_SCOPE)
    endif()
endfunction()

# ==============================================================================
# _mxqt_configure_qt_external_project
# ==============================================================================
function(_mxqt_configure_qt_external_project)
    # Check for required tools
    find_program(GIT_EXECUTABLE git REQUIRED)
    find_program(NINJA_EXECUTABLE ninja REQUIRED)
    find_program(PYTHON_EXECUTABLE NAMES python3 python REQUIRED)

    message(STATUS "qt-multiplier: Git:    ${GIT_EXECUTABLE}")
    message(STATUS "qt-multiplier: Ninja:  ${NINJA_EXECUTABLE}")
    message(STATUS "qt-multiplier: Python: ${PYTHON_EXECUTABLE}")

    # Determine parallel jobs
    _mxqt_get_parallel_jobs(PARALLEL_JOBS)
    message(STATUS "qt-multiplier: Using ${PARALLEL_JOBS} parallel jobs for Qt build")

    # Build the configure arguments
    _mxqt_build_qt_configure_args(QT_CONFIGURE_ARGS)

    # Build and install commands
    set(BUILD_COMMAND ${CMAKE_COMMAND} --build . --parallel ${PARALLEL_JOBS})
    set(INSTALL_COMMAND ${CMAKE_COMMAND} --install .)

    ExternalProject_Add(qt6_external
        PREFIX "${CMAKE_BINARY_DIR}/qt-prefix"

        GIT_REPOSITORY "https://github.com/qt/qt5.git"
        GIT_TAG "${MXQT_QT_VERSION}"
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        # Don't init submodules - Qt's configure -submodules handles this
        GIT_SUBMODULES ""

        SOURCE_DIR "${MXQT_QT_SOURCE_DIR}"
        BINARY_DIR "${MXQT_QT_BUILD_DIR}"
        INSTALL_DIR "${MXQT_QT_INSTALL_DIR}"

        # Qt 6.7+ handles submodule init via configure -submodules
        UPDATE_COMMAND ""

        CONFIGURE_COMMAND <SOURCE_DIR>/configure
                          -prefix <INSTALL_DIR>
                          ${QT_CONFIGURE_ARGS}

        BUILD_COMMAND ${BUILD_COMMAND}
        INSTALL_COMMAND ${INSTALL_COMMAND}

        LOG_DOWNLOAD TRUE
        LOG_UPDATE TRUE
        LOG_CONFIGURE TRUE
        LOG_BUILD TRUE
        LOG_INSTALL TRUE
        LOG_OUTPUT_ON_FAILURE TRUE

        TIMEOUT 0
        BUILD_ALWAYS FALSE
    )

    add_custom_target(build_qt DEPENDS qt6_external)

    message(STATUS "")
    message(STATUS "=============================================================")
    message(STATUS "Qt 6 will be built from source as a vendored dependency.")
    message(STATUS "=============================================================")
    message(STATUS "  Version:     ${MXQT_QT_VERSION}")
    message(STATUS "  Source:      ${MXQT_QT_SOURCE_DIR}")
    message(STATUS "  Build:       ${MXQT_QT_BUILD_DIR}")
    message(STATUS "  Install:     ${MXQT_QT_INSTALL_DIR}")
    message(STATUS "  Parallel:    ${PARALLEL_JOBS} jobs")
    message(STATUS "")
    message(STATUS "Step 1: Build Qt:")
    message(STATUS "  cmake --build ${CMAKE_BINARY_DIR} --target qt6_external")
    message(STATUS "")
    message(STATUS "Step 2: Re-configure to pick up built Qt:")
    message(STATUS "  cmake ${CMAKE_BINARY_DIR}")
    message(STATUS "")
    message(STATUS "Step 3: Build qt-multiplier:")
    message(STATUS "  cmake --build ${CMAKE_BINARY_DIR}")
    message(STATUS "=============================================================")
    message(STATUS "")
endfunction()

# ==============================================================================
# _mxqt_build_qt_configure_args
# ==============================================================================
function(_mxqt_build_qt_configure_args OUTPUT_VAR)
    set(ARGS "")

    list(APPEND ARGS "-opensource" "-confirm-license")
    list(APPEND ARGS "-init-submodules")
    list(APPEND ARGS "-nomake" "examples")
    list(APPEND ARGS "-nomake" "tests")

    # Build type
    if(MXQT_QT_BUILD_TYPE STREQUAL "Debug")
        list(APPEND ARGS "-debug")
    elseif(MXQT_QT_BUILD_TYPE STREQUAL "RelWithDebInfo")
        list(APPEND ARGS "-release" "-force-debug-info")
    else()
        list(APPEND ARGS "-release")
    endif()

    # Submodule specification (Qt 6.7+)
    set(MODULE_LIST "")
    foreach(MODULE ${MXQT_QT_MODULES})
        if(MODULE_LIST)
            set(MODULE_LIST "${MODULE_LIST},${MODULE}")
        else()
            set(MODULE_LIST "${MODULE}")
        endif()
    endforeach()
    if(MODULE_LIST)
        list(APPEND ARGS "-submodules" "${MODULE_LIST}")
    endif()

    # Extra user-specified arguments
    foreach(ARG ${MXQT_QT_CONFIGURE_ARGS})
        list(APPEND ARGS "${ARG}")
    endforeach()

    # Generator: Ninja
    list(APPEND ARGS "--" "-GNinja")

    # Pass compiler through to Qt build
    if(CMAKE_C_COMPILER)
        list(APPEND ARGS "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}")
    endif()
    if(CMAKE_CXX_COMPILER)
        list(APPEND ARGS "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
    endif()

    # macOS specific
    if(APPLE)
        if(CMAKE_OSX_ARCHITECTURES)
            string(REPLACE ";" "\\;" ARCHS "${CMAKE_OSX_ARCHITECTURES}")
            list(APPEND ARGS "-DCMAKE_OSX_ARCHITECTURES=${ARCHS}")
        endif()
        if(CMAKE_OSX_DEPLOYMENT_TARGET)
            list(APPEND ARGS "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
        else()
            list(APPEND ARGS "-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0")
        endif()
    endif()

    set(${OUTPUT_VAR} ${ARGS} PARENT_SCOPE)
endfunction()

# ==============================================================================
# _mxqt_get_parallel_jobs
# ==============================================================================
function(_mxqt_get_parallel_jobs OUTPUT_VAR)
    if(DEFINED MXQT_QT_PARALLEL_JOBS AND MXQT_QT_PARALLEL_JOBS GREATER 0)
        set(${OUTPUT_VAR} ${MXQT_QT_PARALLEL_JOBS} PARENT_SCOPE)
    else()
        include(ProcessorCount)
        ProcessorCount(N)
        if(N EQUAL 0)
            set(N 4)
        endif()
        set(${OUTPUT_VAR} ${N} PARENT_SCOPE)
    endif()
endfunction()
