#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

if(NOT EXISTS "${QT_MULTIPLIER_DATA_PATH}/opt/multiplier/Multiplier.app")
  message(FATAL_ERROR "Invalid QT_MULTIPLIER_DATA_PATH path")
endif()

set(CPACK_DMG_VOLUME_NAME "Multiplier Installer")
set(CMAKE_INSTALL_PREFIX "/" CACHE STRING "Install prefix" FORCE)

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
      "/Multiplier.app/Contents/Developer/usr"

    USE_SOURCE_PERMISSIONS
  )
endforeach()
