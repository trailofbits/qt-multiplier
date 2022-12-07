#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

function(mx_deploy_targets target_name target_bundle_path)
  if(NOT TARGET "${target_name}")
    message(FATAL_ERROR "Target ${target_name} is not defined")
  endif()

  get_property(imported_target_list DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY IMPORTED_TARGETS)
  foreach(mx_target ${imported_target_list})
    if(mx_target MATCHES "^multiplier::*")
      get_target_property(mx_target_location ${mx_target} LOCATION)
      if(EXISTS ${mx_target_location})
        
        get_filename_component(mx_target_name ${mx_target_location} NAME)

        # Exclude mx-index and mx-import since they are already bundled
        # into the target app. We don't want to replace them since the
        # custom command does not fix the rpath.
        if (${mx_target_name} MATCHES "^(mx-index|mx-import)$" )
          continue()
        endif()
        
        add_custom_target(
          ${mx_target_name} ALL 
          DEPENDS ${output_bundle_path}/Contents/bin/${mx_target_name}
        )
      
        add_custom_command(
          OUTPUT ${output_bundle_path}/Contents/bin/${mx_target_name}
          COMMAND ${CMAKE_COMMAND} -E copy ${mx_target_location} ${output_bundle_path}/Contents/bin/${mx_target_name}
        )
      endif()
    endif()
  endforeach()
endfunction()

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
