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
