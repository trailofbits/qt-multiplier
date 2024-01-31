#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

set(CPACK_STRIP_FILES ON)
set(CPACK_DEBIAN_PACKAGE_RELEASE "1")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "extra")
set(CPACK_DEBIAN_PACKAGE_SECTION "default")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>=2.35)")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")

foreach(folder_name "bin" "include" "lib" "share")
  install(
    DIRECTORY
      "${QT_MULTIPLIER_DATA_PATH}/${folder_name}"

    DESTINATION
      "."

    USE_SOURCE_PERMISSIONS
  )
endforeach()

set(qt_library_list
  "libQt6Widgets.so.6"
  "libQt6Gui.so.6"
  "libQt6Concurrent.so.6"
  "libQt6Core5Compat.so.6"
  "libQt6Test.so.6"
  "libQt6Core.so.6"
  "libQt6DBus.so.6"
)

foreach(qt_library ${qt_library_list})
  install(
    FILES
      "${QT_REDIST_PATH}/usr/local/Qt-6.5.2/lib/${qt_library}"
      "${QT_REDIST_PATH}/usr/local/Qt-6.5.2/lib/${qt_library}.5.2"

    DESTINATION
      "lib"
  )
endforeach()
