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
  "Sql"
  "Test"
)

# When using the vendor build (MXQT_USE_SYSTEM_QT=OFF), Qt is either:
#   - Already found by mxqt_setup_qt() in the root CMakeLists.txt, OR
#   - Not yet built (MXQT_QT_AVAILABLE=FALSE), in which case we skip.
if(NOT MXQT_USE_SYSTEM_QT)
  if(NOT MXQT_QT_AVAILABLE)
    message(STATUS "qt-multiplier: Skipping Qt library setup (Qt vendor build not yet complete)")
    return()
  endif()
  # Qt was already found by mxqt_setup_qt(); targets exist.
else()
  find_package("Qt6"
    COMPONENTS ${qt_component_list}
    REQUIRED
    GLOBAL
  )
endif()

add_library("mx_qt_library" INTERFACE)

target_include_directories("mx_qt_library" SYSTEM INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

foreach(qt_component ${qt_component_list})
  target_link_libraries("mx_qt_library" INTERFACE
    "Qt6::${qt_component}"
  )
endforeach()
