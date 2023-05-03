#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

if(NOT EXISTS "${QT_MULTIPLIER_DATA_PATH}/usr/local/bin/multiplier")
  message(FATAL_ERROR "Invalid QT_MULTIPLIER_DATA_PATH path")
endif()

set(CPACK_STRIP_FILES ON)
set(CPACK_DEBIAN_PACKAGE_RELEASE "1")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "extra")
set(CPACK_DEBIAN_PACKAGE_SECTION "default")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>=2.35), libqt6gui6 (>=6.2.4), libgl1-mesa-dri (>=22.2.5), libglvnd0 (>=1.4.0)")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")

install(
  FILES
    "${QT_MULTIPLIER_DATA_PATH}/usr/local/bin/multiplier"

  DESTINATION
    "bin"

  PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ             GROUP_EXECUTE
    WORLD_READ             WORLD_EXECUTE 
)

install(
  FILES
  "${QT_MULTIPLIER_DATA_PATH}/usr/local/share/multiplier/LICENSE.txt"
  "${QT_MULTIPLIER_DATA_PATH}/usr/local/share/multiplier/library_manifest.txt"

  DESTINATION
    "share/multiplier"
)

foreach("folder_name" "bin" "lib" "include")
  install(
    DIRECTORY
      "${MULTIPLIER_DATA_PATH}/${folder_name}"

    DESTINATION
      "."

    USE_SOURCE_PERMISSIONS
  )
endforeach()
