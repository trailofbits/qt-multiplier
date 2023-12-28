#
# Copyright (c) 2022-present, Trail of Bits, Inc.
# All rights reserved.
#
# This source code is licensed in accordance with the terms specified in
# the LICENSE file found in the root directory of this source tree.
#

add_library("mx_util"
  include/multiplier/GUI/IModel.h
  src/IModel.cpp

  include/multiplier/GUI/Util.h
  src/Util.h
  src/Util.cpp

  include/multiplier/GUI/TokenPainter.h
  src/TokenPainter.cpp

  include/multiplier/GUI/Icons.h
  src/Icons.cpp

  include/multiplier/GUI/DockWidgetContainer.h
  include/multiplier/GUI/PopupWidgetContainer.h

  include/multiplier/GUI/TabWidget.h
  src/TabWidget.cpp

  include/multiplier/GUI/TreeWidget.h
  src/TreeWidget.cpp

  include/multiplier/GUI/SimpleTextInputDialog.h
  src/SimpleTextInputDialog.cpp

  include/multiplier/GUI/Assert.h
  src/Assert.cpp
)

target_link_libraries("mx_util"
  PRIVATE
    "mx_cxx_flags"
    "mx_codeviewtheme"
    "thirdparty_qt"
    "thirdparty_multiplier"

  PUBLIC
    "mx_thememanager"
)

target_include_directories("mx_util" PRIVATE
  "include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories("mx_util" SYSTEM INTERFACE
  "include"
)

enable_qt_properties("mx_util")
