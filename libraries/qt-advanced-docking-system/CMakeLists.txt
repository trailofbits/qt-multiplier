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

find_package(Git REQUIRED)

execute_process(
  COMMAND "${GIT_EXECUTABLE}" reset --hard
  COMMAND "${GIT_EXECUTABLE}" apply "${CMAKE_CURRENT_SOURCE_DIR}/patches/qt-advanced-docking-system_css_patch.patch"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# When building on Linux, this library requires access to the private
# headers of the Gui module.
#
# They are used in the `ads_globals.cpp` source file and are included
# with the following path: `qpa/qplatformnativeinterface.h`
#
# The path on disk is the following:
# We need Qt Gui private headers for qpa/qplatformnativeinterface.h.
# Use the Qt6GuiPrivate target if available (system Qt with private-dev
# package installed), otherwise fall back to manual path discovery.
if (UNIX AND NOT APPLE)
  if(TARGET Qt6::GuiPrivate)
    target_link_libraries("mx_qt-ads" PRIVATE Qt6::GuiPrivate)
  else()
    # Manual fallback: find the private headers directory.
    # System Qt (Ubuntu): /usr/include/<arch>/qt6/QtGui/<version>/QtGui/
    # Vendored Qt: <prefix>/include/QtGui/<version>/QtGui/
    set(_qtgui_private_found FALSE)

    get_target_property(_qt6gui_include_dirs Qt6::Gui INTERFACE_INCLUDE_DIRECTORIES)
    foreach(_inc_dir ${_qt6gui_include_dirs})
      set(_candidate "${_inc_dir}/${Qt6_VERSION}/QtGui")
      if(EXISTS "${_candidate}/qpa/qplatformnativeinterface.h")
        target_include_directories("mx_qt-ads" PRIVATE "${_candidate}")
        set(_qtgui_private_found TRUE)
        break()
      endif()
    endforeach()

    if(NOT _qtgui_private_found)
      message(FATAL_ERROR
          "mx_qt-ads: Failed to locate the private Qt Gui module headers. "
          "Install qt6-base-private-dev or set Qt6Gui_DIR.")
    endif()
  endif()
endif()

if(MXQT_ENABLE_INSTALL)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(install_destination "lib")
  else()
    set(install_destination ".")
  endif()

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties("mx_qt-ads" PROPERTIES
      INSTALL_RPATH "$ORIGIN/../lib"
    )
  else()
    set_target_properties("mx_qt-ads" PROPERTIES
      INSTALL_RPATH "@loader_path"
    )
  endif()

  install(
    TARGETS "mx_qt-ads"
    DESTINATION "${install_destination}"
  )

  install(
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/patches/qt-advanced-docking-system_css_patch.patch"
    DESTINATION "${install_destination}/usr/share/multiplier/patches/qt-advanced-docking-system"
  )
endif()
