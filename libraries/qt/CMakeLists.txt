#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

set(qt_component_list
  "Core"
  "Gui"
  "Widgets"
  "Concurrent"
  "Core5Compat"
  "Test"
)

find_package("Qt6"
  COMPONENTS ${qt_component_list}
  REQUIRED
  GLOBAL
)

add_library("mx_qt_library" INTERFACE)

foreach(qt_component ${qt_component_list})
  target_link_libraries("mx_qt_library" INTERFACE
    "Qt6::${qt_component}"
  )
endforeach()
