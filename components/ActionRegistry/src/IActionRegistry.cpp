/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ActionRegistry.h"

#include <multiplier/ui/IActionRegistry.h>

namespace mx::gui {

IActionRegistry::Ptr
IActionRegistry::Create(Index index, FileLocationCache file_location_cache) {
  return std::unique_ptr<IActionRegistry>(
      new ActionRegistry(index, file_location_cache));
}

}  // namespace mx::gui
