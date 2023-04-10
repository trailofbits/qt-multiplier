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

set(QT_MULTIPLIER_DATA_PATH "" CACHE PATH "This is where the Multiplier.app application was installed")
set(MULTIPLIER_DATA_PATH "" CACHE PATH "This is where multiplier has been installed, along with all its dependencies")

set(CPACK_PACKAGE_VERSION "${QTMULTIPLIER_VERSION}" CACHE STRING "Package version")
set(QT_MULTIPLIER_PACKAGE_RELEASE "1.macos")

if(NOT EXISTS "${QT_MULTIPLIER_DATA_PATH}/opt/multiplier/Multiplier.app")
  message(FATAL_ERROR "Invalid QT_MULTIPLIER_DATA_PATH path")
endif()

if(NOT EXISTS "${MULTIPLIER_DATA_PATH}/bin/mx-index")
  message(FATAL_ERROR "Invalid MULTIPLIER_DATA_PATH path")
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
set(CPACK_RESOURCE_FILE_LICENSE "${QT_MULTIPLIER_DATA_PATH}/opt/multiplier/control/LICENSE.txt")

set(CPACK_PACKAGE_RELOCATABLE ON)
set(CPACK_SET_DESTDIR ON)

set(CPACK_DMG_VOLUME_NAME "Multiplier Installer")
set(CMAKE_INSTALL_PREFIX "/" CACHE STRING "Install prefix" FORCE)

if(NOT CPACK_GENERATOR STREQUAL "DragNDrop")
  message(FATAL_ERROR "The following generator is not supported: ${CPACK_GENERATOR}")
endif()

include(CPack)

install(
  DIRECTORY
    "${QT_MULTIPLIER_DATA_PATH}/opt/multiplier/Multiplier.app"

  DESTINATION
    "/"

  USE_SOURCE_PERMISSIONS
)

install(
  FILES
    "${QT_MULTIPLIER_DATA_PATH}/opt/multiplier/control/library_manifest.txt"

  DESTINATION
    "/"
)

foreach("folder_name" "bin" "lib" "include")
  install(
    DIRECTORY
      "${MULTIPLIER_DATA_PATH}/${folder_name}"

    DESTINATION
      "${install_destination}Multiplier.app/Contents/Developer/usr"

    USE_SOURCE_PERMISSIONS
  )
endforeach()
