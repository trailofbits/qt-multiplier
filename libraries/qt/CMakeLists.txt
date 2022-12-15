#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

set(qt_component_list
  "Core"
  "Widgets"
  "Concurrent"
)

if(MX_QT_VERSION GREATER "5")
  list(APPEND qt_component_list "Core5Compat")
endif()

find_package("Qt${MX_QT_VERSION}"
  COMPONENTS ${qt_component_list}
  REQUIRED
  GLOBAL
)

add_library("thirdparty_qt" INTERFACE)

foreach(qt_component ${qt_component_list})
  target_link_libraries("thirdparty_qt" INTERFACE
    "Qt${MX_QT_VERSION}::${qt_component}"
  )
endforeach()
