#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

function(enable_qt_properties target_name)
  if(NOT TARGET "${target_name}")
    message(FATAL_ERROR "qt-multiplier: Target not found: ${target_name}")
  endif()

  set_target_properties("${target_name}"
    PROPERTIES
      AUTOMOC true
      AUTOUIC true
      AUTORCC true
  )
endfunction()

function(enable_appbundle_postbuild_steps target_name)
  get_target_property(is_macos_bundle "${target_name}" MACOSX_BUNDLE)
  if(NOT is_macos_bundle)
    message(FATAL_ERROR "qt-multiplier: Target ${target_name} is not a macOS app bundle")
  endif()

  if(NOT MXQT_ENABLE_MACDEPLOYQT)
    message(WARNING "qt-multiplier: Skipping the macdeployqt post-build steps for target ${target_name}. The binary will not be portable!")
    return()
  endif()

  find_program(macdeployqt_path
    NAME
      "macdeployqt"

    HINTS
      "/usr/local/bin"
  )

  if(MACDEPLOYQT_EXECUTABLE)
    set(macdeployqt_path "${MACDEPLOYQT_EXECUTABLE}")
  endif()

  if(NOT macdeployqt_path)
    message(FATAL_ERROR "qt-multiplier: Failed to locate the macdeployqt executable")
  endif()

  get_target_property(binary_dir "${target_name}" BINARY_DIR)
  if(NOT binary_dir)
    message(FATAL_ERROR "qt-multiplier: Failed to obtain the BINARY_DIR property for target ${target_name}")
  endif()

  get_target_property(binary_suffix "${target_name}" SUFFIX)
  if(NOT binary_suffix)
    set(binary_suffix "app")
  endif()

  set(binary_path "${binary_dir}/${target_name}.${binary_suffix}")

  add_custom_command(
    TARGET
      "${target_name}"

    POST_BUILD

    # Required to get macdeployqt from homebrew to work correctly
    COMMAND
      "${CMAKE_COMMAND}" -E create_symlink "/usr/local/opt/qt/lib" "${CMAKE_CURRENT_BINARY_DIR}/lib"

    COMMAND
      "${macdeployqt_path}" "${binary_path}" -no-strip -always-overwrite

    WORKING_DIRECTORY
      "${CMAKE_CURRENT_BINARY_DIR}"

    COMMENT
      "qt-multiplier: Processing target ${target_name} with macdeployqt"
  )
endfunction()

function(enable_test_definitions target_name)
  target_compile_definitions("${target_name}" PRIVATE
    "MXQT_CI_DATA_PATH=\"${CMAKE_SOURCE_DIR}/ci/data\""
  )
endfunction()
