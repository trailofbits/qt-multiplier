#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

cmake_minimum_required(VERSION 3.19)
project("qt-multiplier")

include("${CMAKE_SOURCE_DIR}/../cmake/version.cmake")

set(QT_MULTIPLIER_DATA_PATH "" CACHE PATH "This is where qt-multiplier has been installed")
set(MULTIPLIER_DATA_PATH "" CACHE PATH "This is where multiplier has been installed")
set(QT_REDIST_PATH "" CACHE PATH "Path to the Qt SDK redist folder")

set(CPACK_PACKAGE_VERSION "${QTMULTIPLIER_VERSION}" CACHE STRING "Package version")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(QT_MULTIPLIER_PACKAGE_RELEASE "1.linux")
else()
  set(QT_MULTIPLIER_PACKAGE_RELEASE "1.macos")
endif()

if(NOT EXISTS "${QT_MULTIPLIER_DATA_PATH}/bin/multiplier")
  message(FATAL_ERROR "Invalid QT_MULTIPLIER_DATA_PATH path.")
endif()

if(NOT EXISTS "${MULTIPLIER_DATA_PATH}/bin/mx-index")
  message(FATAL_ERROR "Invalid MULTIPLIER_DATA_PATH path.")
endif()

if(NOT EXISTS "${QT_REDIST_PATH}/usr/local/Qt-6.7.0/bin/qmake")
  message(FATAL_ERROR "Invalid QT_REDIST_PATH path.")
endif()

if(NOT CPACK_GENERATOR)
  message(FATAL_ERROR "The CPACK_GENERATOR variable was not set")
endif()

set(CPACK_PACKAGE_DESCRIPTION "Graphical user interface for Multiplier.")
set(CPACK_PACKAGE_NAME "multiplier")
set(CPACK_PACKAGE_VENDOR "multiplier")
set(CPACK_PACKAGE_CONTACT "peter@trailofbits.com")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://trailofbits.com")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}_${QT_MULTIPLIER_PACKAGE_RELEASE}_${CMAKE_SYSTEM_PROCESSOR}")

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
  set(CPACK_RESOURCE_FILE_LICENSE "${QT_MULTIPLIER_DATA_PATH}/share/multiplier/LICENSE.txt")
else()
  set(CPACK_RESOURCE_FILE_LICENSE "${QT_MULTIPLIER_DATA_PATH}/opt/multiplier/control/LICENSE.txt")
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
