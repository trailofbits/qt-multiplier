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
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>=2.35), libglx0 (>=1.4.0), libxcb1 (>=1.14), libxcb-xinput0 (>=1.14), libinput-bin (>=1.20.0), libx11-xcb1 (>=1.7.5), libxcb-util1 (>=0.4.0), libxcb-composite0 (>= 1.14), libxcb-cursor0 (>= 0.1.1), libxcb-dpms0 (>= 1.14), libxcb-ewmh2 (>= 0.4.1), libxcb-imdkit1 (>= 1.0.3), libxcb-record0 (>= 1.14), libxcb-screensaver0 (>= 1.14), libxcb-xf86dri0 (>= 1.14), libxcb-xinerama0 (>= 1.14), libxcb-xrm0 (>= 1.0), libxcb-xtest0 (>= 1.14), libxcb-xvmc0 (>= 1.14), python3.11 (>=3.11)")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")

#
# Multiplier files
#

set(multiplier_data_file_list
  # LLVM libraries
  "lib/libLTO.${dyn_lib_ext}"
  "lib/libLTO.${dyn_lib_ext}.17"
  "lib/libRemarks.${dyn_lib_ext}"
  "lib/libRemarks.${dyn_lib_ext}.17"

  # Multiplier files
  "lib/libmultiplier.${dyn_lib_ext}"
  "lib/python3.11"
  "bin/mx-count-sourceir"
  "bin/mx-dump-files"
  "bin/mx-find-calls-in-macro-expansions"
  "bin/mx-find-divergent-candidates"
  "bin/mx-find-flexible-user-copies"
  "bin/mx-find-linked-structures"
  "bin/mx-find-sketchy-casts"
  "bin/mx-find-sketchy-strchr"
  "bin/mx-find-symbol"
  "bin/mx-harness"
  "bin/mx-highlight-entity"
  "bin/mx-index"
  "bin/mx-list-files"
  "bin/mx-list-fragments"
  "bin/mx-list-functions"
  "bin/mx-list-macros"
  "bin/mx-list-redeclarations"
  "bin/mx-list-structures"
  "bin/mx-list-variables"
  "bin/mx-print-call-graph"
  "bin/mx-print-call-hierarchy"
  "bin/mx-print-file"
  "bin/mx-print-fragment"
  "bin/mx-print-include-graph"
  "bin/mx-print-reference-graph"
  "bin/mx-print-token-graph"
  "bin/mx-print-token-tree"
  "bin/mx-print-type-token-graph"
  "bin/mx-regex-query"
  "bin/mx-taint-entity"

  # 3rd parties
  "include/gap"
)

foreach(multiplier_data_file ${multiplier_data_file_list})
  get_filename_component(destination_path "${multiplier_data_file}" DIRECTORY)

  if(IS_DIRECTORY "${MULTIPLIER_DATA_PATH}/${multiplier_data_file}")
    install(
      DIRECTORY
        "${MULTIPLIER_DATA_PATH}/${multiplier_data_file}"

      DESTINATION
        "/opt/multiplier/${destination_path}"

      USE_SOURCE_PERMISSIONS
    )

  else()
    if("${destination_path}" MATCHES "bin|lib")
      set(permissions "${standard_executable_permissions}")
    else()
      set(permissions "${standard_resource_permissions}")
    endif()

    install(
      FILES
        "${MULTIPLIER_DATA_PATH}/${multiplier_data_file}"

      DESTINATION
        "/opt/multiplier/${destination_path}"

      PERMISSIONS
        ${permissions}
    )
  endif()
endforeach()

#
# Qt SDK files
#

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
      "${QT_REDIST_PATH}/usr/local/Qt-6.7.0/lib/${qt_library}"
      "${QT_REDIST_PATH}/usr/local/Qt-6.7.0/lib/${qt_library}.7.0"

    DESTINATION
      "/opt/multiplier/lib"
  )
endforeach()

install(
  DIRECTORY
    "${QT_REDIST_PATH}/usr/local/Qt-6.7.0/plugins"

  DESTINATION
    "/opt/multiplier"

  USE_SOURCE_PERMISSIONS
)

#
# qt-multiplier files
#

install(
  FILES
    "${QT_MULTIPLIER_DATA_PATH}/lib/libmx_qt-ads.so"
    "${QT_MULTIPLIER_DATA_PATH}/lib/libmx_phantomstyle_library.so"
    "${QT_MULTIPLIER_DATA_PATH}/share/multiplier/LICENSE.txt"
    "${QT_MULTIPLIER_DATA_PATH}/share/multiplier/library_manifest.txt"

  DESTINATION
    "/opt/multiplier/lib"
)

install(
  FILES
    "${QT_MULTIPLIER_DATA_PATH}/bin/multiplier"

  DESTINATION
    "/opt/multiplier/bin"

  PERMISSIONS
    ${standard_executable_permissions}
)

# `multiplier` symlink in /usr/local/bin
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E create_symlink "/opt/multiplier/bin/multiplier" "${CMAKE_CURRENT_BINARY_DIR}/multiplier"
)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/multiplier"

  DESTINATION
    "/usr/local/bin"
)

# The qt-multiplier .desktop file to make it appear in menus
install(
  FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/data/linux/multiplier.desktop"

  DESTINATION
    "/usr/share/applications"
)

install(
  FILES
    "${QT_MULTIPLIER_DATA_PATH}/share/icons/logo.png"

  DESTINATION
    "/opt/multiplier/share/icons"
)
