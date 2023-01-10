#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

cmake_minimum_required(VERSION 3.19)

add_executable("multiplier"
  src/main.cpp

  src/MainWindow.h
  src/MainWindow.cpp

  src/meta_types.h
  src/meta_types.cpp
)

target_sources("multiplier" PRIVATE
  Resources/resources.qrc
  Info.plist
)

target_link_libraries("multiplier"
  PUBLIC
    multiplier::mx-api
    thirdparty_qt
    mx_codeview
    mx_util
    mx_database
    mx_fontdatabase
)

enable_qt_properties("multiplier")
