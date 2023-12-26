#
# Copyright (c) 2023-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_mainwindowplugin"
  include/multiplier/ui/IMainWindowPlugin.h
  src/IMainWindowPlugin.cpp
)

target_link_libraries("mx_mainwindowplugin"
  PRIVATE
    "mx_cxxflags"

  PUBLIC
    "thirdparty_qt"
    "thirdparty_multiplier"
    "mx_actionregistry"
    "mx_globalhighlighter"
)

target_include_directories("mx_mainwindowplugin" PRIVATE
  "include"
)

target_include_directories("mx_mainwindowplugin" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_mainwindowplugin")
