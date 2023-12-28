/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "EntityExplorerModel.h"

#include <multiplier/GUI/IEntityExplorerModel.h>

namespace mx::gui {

IEntityExplorerModel *
IEntityExplorerModel::Create(const Index &index,
                             const FileLocationCache &file_location_cache,
                             QObject *parent) {

  try {
    return new EntityExplorerModel(index, file_location_cache, parent);

  } catch (const std::bad_alloc &) {
    return nullptr;
  }
}

}  // namespace mx::gui
