#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

if(MXQT_ENABLE_INSTALL AND MXQT_GENERATE_LIBRARY_MANIFEST)
  set(library_list
    "multiplier"
  )

  find_package(Git REQUIRED)

  foreach(library ${library_list})
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${library}"
      OUTPUT_VARIABLE "library_version"
    )

    set(library_manifest
      "${library_manifest}${library}: ${library_version}\n"
    )
  endforeach()

  set(library_manifest_path "${CMAKE_BINARY_DIR}/library_manifest.txt")
  file(WRITE "${library_manifest_path}" "${library_manifest}")

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(install_destination "share/multiplier")
  else()
    set(install_destination "control")
  endif()

  install(
    FILES "${library_manifest_path}"
    DESTINATION "${install_destination}"
  )
endif()
