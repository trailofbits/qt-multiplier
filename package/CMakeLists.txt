#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

cmake_minimum_required(VERSION 3.19)
project("qt-multiplier-package")

include("${CMAKE_SOURCE_DIR}/../cmake/version.cmake")

# BUNDLE_DIR is the self-contained directory with all binaries, libraries,
# plugins, and resources already assembled by the CI or manually.
# Expected layout:
#   bin/multiplier, bin/mx-*, bin/qt.conf
#   lib/*.so*
#   plugins/{platforms,imageformats,...}
#   share/icons/logo.png
#   share/multiplier/LICENSE.txt
set(BUNDLE_DIR "" CACHE PATH
    "Path to the pre-assembled qt-multiplier bundle directory")

if(NOT EXISTS "${BUNDLE_DIR}/bin/multiplier")
  message(FATAL_ERROR
      "BUNDLE_DIR does not contain bin/multiplier. "
      "Set BUNDLE_DIR to the fully assembled bundle directory.")
endif()

if(NOT CPACK_GENERATOR)
  message(FATAL_ERROR "The CPACK_GENERATOR variable was not set")
endif()

set(CPACK_PACKAGE_DESCRIPTION "Graphical user interface for Multiplier.")
set(CPACK_PACKAGE_NAME "multiplier")
set(CPACK_PACKAGE_VENDOR "Trail of Bits")
set(CPACK_PACKAGE_CONTACT "peter@trailofbits.com")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/trailofbits/qt-multiplier")
set(CPACK_PACKAGE_VERSION "${QTMULTIPLIER_VERSION}" CACHE STRING "Package version")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(QT_MULTIPLIER_PACKAGE_RELEASE "1.linux")
else()
  set(QT_MULTIPLIER_PACKAGE_RELEASE "1.macos")
endif()

set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}_${QT_MULTIPLIER_PACKAGE_RELEASE}_${CMAKE_SYSTEM_PROCESSOR}")

if(EXISTS "${BUNDLE_DIR}/share/multiplier/LICENSE.txt")
  set(CPACK_RESOURCE_FILE_LICENSE "${BUNDLE_DIR}/share/multiplier/LICENSE.txt")
endif()

set(CPACK_PACKAGE_RELOCATABLE ON)
set(CPACK_SET_DESTDIR ON)

include("generators/common.cmake")

set(generator_include "${CMAKE_CURRENT_SOURCE_DIR}/generators/${CPACK_GENERATOR}.cmake")
if(NOT EXISTS "${generator_include}")
  message(FATAL_ERROR "The following generator is not supported: ${CPACK_GENERATOR}")
endif()

include("${generator_include}")
include("CPack")
