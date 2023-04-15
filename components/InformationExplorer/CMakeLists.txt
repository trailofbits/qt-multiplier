#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_informationexplorer"
  include/multiplier/ui/IInformationExplorerModel.h
  src/IInformationExplorerModel.cpp

  include/multiplier/ui/IInformationExplorer.h
  src/IInformationExplorer.cpp

  src/InformationExplorerModel.h
  src/InformationExplorerModel.cpp

  src/InformationExplorer.h
  src/InformationExplorer.cpp
)

target_link_libraries("mx_informationexplorer"
  PRIVATE
    "qtmx_cxx_flags"

  PUBLIC
    "thirdparty_qt"
    "multiplier::mx-api"
    "mx_assert"
    "mx_database"
    "mx_searchwidget"
)

target_include_directories("mx_informationexplorer" PRIVATE
  "include"
)

target_include_directories("mx_informationexplorer" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_informationexplorer")
