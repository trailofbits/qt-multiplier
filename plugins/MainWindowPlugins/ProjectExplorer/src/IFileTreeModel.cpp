/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "FileTreeModel.h"

#include <multiplier/GUI/IFileTreeModel.h>

namespace mx::gui {

IFileTreeModel *IFileTreeModel::Create(const mx::Index &index, QObject *parent) {
  return new FileTreeModel(index, parent);
}

}  // namespace mx::gui
