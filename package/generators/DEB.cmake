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
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>=2.35), libglx0 (>=1.4.0), libxcb1 (>=1.14), libxcb-xinput0 (>=1.14), libinput-bin (>=1.20.0), libx11-xcb1 (>=1.7.5), libxcb-util1 (>=0.4.0), libxcb-composite0 (>= 1.14), libxcb-cursor0 (>= 0.1.1), libxcb-dpms0 (>= 1.14), libxcb-ewmh2 (>= 0.4.1), libxcb-imdkit1 (>= 1.0.3), libxcb-record0 (>= 1.14), libxcb-screensaver0 (>= 1.14), libxcb-xf86dri0 (>= 1.14), libxcb-xinerama0 (>= 1.14), libxcb-xrm0 (>= 1.0), libxcb-xtest0 (>= 1.14), libxcb-xvmc0 (>= 1.14), python3.10 (>=3.10.12)")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")

foreach(folder_name "bin" "include" "lib" "share")
  install(
    DIRECTORY
      "${QT_MULTIPLIER_DATA_PATH}/${folder_name}"

    DESTINATION
      "/opt/multiplier"

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
  "libQt6OpenGL.so.6"
  "libQt6XcbQpa.so.6"
)

foreach(qt_library ${qt_library_list})
  install(
    FILES
      "${QT_REDIST_PATH}/usr/local/Qt-6.5.2/lib/${qt_library}"
      "${QT_REDIST_PATH}/usr/local/Qt-6.5.2/lib/${qt_library}.5.2"

    DESTINATION
      "/opt/multiplier/lib"
  )
endforeach()

install(
  DIRECTORY
    "${QT_REDIST_PATH}/usr/local/Qt-6.5.2/plugins"

  DESTINATION
    "/opt/multiplier"

  USE_SOURCE_PERMISSIONS
)

foreach(public_binary "multiplier" "mx-index")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E create_symlink "/opt/multiplier/bin/${public_binary}" "${CMAKE_CURRENT_BINARY_DIR}/${public_binary}"
  )

  install(
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/${public_binary}"

    DESTINATION
      "/usr/local/bin"
  )
endforeach()

install(
  FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/data/linux/multiplier.desktop"

  DESTINATION
    "/usr/share/applications"
)
