/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "MacroExplorer.h"

#include <multiplier/GUI/IMacroExplorer.h>

namespace mx::gui {

IMacroExplorer *IMacroExplorer::Create(
    const Index &index, const FileLocationCache &file_cache, QWidget *parent) {

  return new MacroExplorer(index, file_cache, parent);
}

}  // namespace mx::gui
