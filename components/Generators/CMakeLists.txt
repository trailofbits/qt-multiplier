#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_generators"
  include/multiplier/GUI/IGeneratorView.h
  src/IGeneratorView.cpp

  src/GeneratorView.h
  src/GeneratorView.cpp

  src/SearchFilterModelProxy.h
  src/SearchFilterModelProxy.cpp

  src/FilterSettingsWidget.h
  src/FilterSettingsWidget.cpp

  include/multiplier/GUI/IGeneratorModel.h
  src/IGeneratorModel.cpp

  include/multiplier/GUI/ITreeGenerator.h
  src/ITreeGenerator.cpp

  src/ITreeExplorerExpansionThread.h
  src/ITreeExplorerExpansionThread.cpp

  src/InitTreeExplorerThread.h
  src/InitTreeExplorerThread.cpp

  src/InitTreeExplorerThread.h
  src/InitTreeExplorerThread.cpp

  src/ExpandTreeExplorerThread.h
  src/ExpandTreeExplorerThread.cpp

  src/GeneratorModel.h
  src/GeneratorModel.cpp
)

target_link_libraries("mx_generators"
  PRIVATE
    "mx_cxx_flags"

  PUBLIC
    "mx_qt_library"
    "mx_multiplier_library"
    "mx_util_component"
    "mx_searchwidget"
    "mx_theme_manager"
)

target_include_directories("mx_generators" PRIVATE
  "include"
)

target_include_directories("mx_generators" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_generators")
