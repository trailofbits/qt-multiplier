#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

option(MXQT_ENABLE_TESTS "Set to true to enable tests" true)
option(MXQT_ENABLE_MACDEPLOYQT "Set to true to build portable binaries" true)
option(MXQT_ENABLE_INSTALL "Set to true to enable the install directives" true)
option(MXQT_GENERATE_LIBRARY_MANIFEST "Set to true to generate the library manifest" true)
option(MXQT_EVAL_COPY "Set to true to enable an 'evaluation copy' label" false)

# ==============================================================================
# Qt Vendor Build Options
# ==============================================================================

# When ON:  Uses find_package(Qt6) to locate a pre-installed Qt.
# When OFF: Qt is built from source using the official GitHub mirror
#           via a superbuild pattern (ExternalProject).
option(MXQT_USE_SYSTEM_QT
    "Use system-installed Qt instead of building from source"
    ON
)

# Git tag to checkout when building Qt from source.
# Must be a valid tag from https://github.com/qt/qt5 (qt5 repo contains Qt 6).
set(MXQT_QT_VERSION "v6.8.2" CACHE STRING
    "Qt version tag to build (e.g., v6.8.2, v6.7.0)"
)

# Qt submodules to build. qt-multiplier needs:
#   qtbase    -> Core, Gui, Widgets, Concurrent, Test
#   qt5compat -> Core5Compat
set(MXQT_QT_MODULES
    "qtbase"
    "qt5compat"
    CACHE STRING
    "List of Qt submodules to build"
)

# Build type for the vendored Qt build.
set(MXQT_QT_BUILD_TYPE "Release" CACHE STRING
    "Build type for vendored Qt (Release, Debug, RelWithDebInfo)"
)

# Additional arguments passed to Qt's configure script.
# The following are added automatically:
#   -opensource -confirm-license -nomake examples -nomake tests -submodules
set(MXQT_QT_CONFIGURE_ARGS "" CACHE STRING
    "Additional arguments passed to Qt configure script"
)

# Number of parallel build jobs for the Qt vendor build.
set(MXQT_QT_PARALLEL_JOBS 0 CACHE STRING
    "Number of parallel Qt build jobs (0 = use all cores)"
)
