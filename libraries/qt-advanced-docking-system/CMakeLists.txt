#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

set(library_root
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

add_library("mx_qt-ads" SHARED
  "${library_root}/src/ads.qrc"

  "${library_root}/src/ads_globals.cpp"
  "${library_root}/src/ads_globals.h"

  "${library_root}/src/AutoHideDockContainer.cpp"
  "${library_root}/src/AutoHideDockContainer.h"

  "${library_root}/src/AutoHideSideBar.cpp"
  "${library_root}/src/AutoHideSideBar.h"

  "${library_root}/src/AutoHideTab.cpp"
  "${library_root}/src/AutoHideTab.h"

  "${library_root}/src/DockAreaTabBar.cpp"
  "${library_root}/src/DockAreaTabBar.h"

  "${library_root}/src/DockAreaTitleBar.cpp"
  "${library_root}/src/DockAreaTitleBar.h"
  "${library_root}/src/DockAreaTitleBar_p.h"

  "${library_root}/src/DockAreaWidget.cpp"
  "${library_root}/src/DockAreaWidget.h"

  "${library_root}/src/DockComponentsFactory.cpp"
  "${library_root}/src/DockComponentsFactory.h"

  "${library_root}/src/DockContainerWidget.cpp"
  "${library_root}/src/DockContainerWidget.h"

  "${library_root}/src/DockFocusController.cpp"
  "${library_root}/src/DockFocusController.h"

  "${library_root}/src/DockingStateReader.cpp"
  "${library_root}/src/DockingStateReader.h"

  "${library_root}/src/DockManager.cpp"
  "${library_root}/src/DockManager.h"

  "${library_root}/src/DockOverlay.cpp"
  "${library_root}/src/DockOverlay.h"

  "${library_root}/src/DockSplitter.cpp"
  "${library_root}/src/DockSplitter.h"

  "${library_root}/src/DockWidget.cpp"
  "${library_root}/src/DockWidget.h"

  "${library_root}/src/DockWidgetTab.cpp"
  "${library_root}/src/DockWidgetTab.h"

  "${library_root}/src/ElidingLabel.cpp"
  "${library_root}/src/ElidingLabel.h"

  "${library_root}/src/FloatingDockContainer.cpp"
  "${library_root}/src/FloatingDockContainer.h"

  "${library_root}/src/FloatingDragPreview.cpp"
  "${library_root}/src/FloatingDragPreview.h"

  "${library_root}/src/IconProvider.cpp"
  "${library_root}/src/IconProvider.h"

  "${library_root}/src/PushButton.cpp"
  "${library_root}/src/PushButton.h"

  "${library_root}/src/ResizeHandle.cpp"
  "${library_root}/src/ResizeHandle.h"
)

if (UNIX AND NOT APPLE)
  target_sources("mx_qt-ads" PRIVATE
    "${library_root}/src/linux/FloatingWidgetTitleBar.cpp"
    "${library_root}/src/linux/FloatingWidgetTitleBar.h"
  )
endif()

target_compile_definitions("mx_qt-ads" PRIVATE
  "ADS_SHARED_EXPORT"
)

enable_qt_properties("mx_qt-ads")

add_library("mx_qt-ads_disable_warnings" INTERFACE)
target_compile_options("mx_qt-ads_disable_warnings" INTERFACE
  -Wno-everything
  -Wno-error
)

target_link_libraries("mx_qt-ads"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "mx_qt_library"
    "mx_qt-ads_disable_warnings"
)

target_include_directories("mx_qt-ads" PRIVATE
  "${library_root}/src"
)

target_include_directories("mx_qt-ads" SYSTEM INTERFACE
  "${library_root}/src"
)

# When building on Linux, this library requires access to the private
# headers of the Gui module.
#
# They are used in the `ads_globals.cpp` source file and are included
# with the following path: `qpa/qplatformnativeinterface.h`
#
# The path on disk is the following:
# <qt-install-prefix>/usr/local/Qt-6.5.2/include/QtGui/6.5.2/QtGui/qpa/qplatformnativeinterface.h
if (UNIX AND NOT APPLE)
  if(NOT Qt6Gui_DIR)
    message(FATAL_ERROR "mx_qt-ads: The `Qt6Gui_DIR` variable is not defined")
  endif()

  get_filename_component(qtsdk_base_path "${Qt6Gui_DIR}" DIRECTORY)
  get_filename_component(qtsdk_base_path "${qtsdk_base_path}" DIRECTORY)
  get_filename_component(qtsdk_base_path "${qtsdk_base_path}" DIRECTORY)

  get_filename_component(qtsdk_version "${qtsdk_base_path}" NAME)
  string(REPLACE "-" ";" qtsdk_version "${qtsdk_version}")
  list(GET qtsdk_version 1 qtsdk_version)

  set(qtgui_private_include_dir "${qtsdk_base_path}/include/QtGui/${qtsdk_version}/QtGui")

  if(NOT EXISTS "${qtgui_private_include_dir}/qpa/qplatformnativeinterface.h")
    message(FATAL_ERROR "mx_qt-ads: Failed to locate the private Qt Gui module headers")
  endif()

  target_include_directories("mx_qt-ads" PRIVATE
    "${qtgui_private_include_dir}"
  )
endif()

if(MXQT_ENABLE_INSTALL)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(install_destination "lib")
  else()
    set(install_destination ".")
  endif()

  set_target_properties("mx_qt-ads" PROPERTIES
    INSTALL_RPATH "$ORIGIN/../lib"
  )

  install(
    TARGETS "mx_qt-ads"
    DESTINATION "${install_destination}"
  )
endif()
