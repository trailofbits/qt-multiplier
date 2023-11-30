/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "TreeExplorerModel.h"

#include <multiplier/ui/ITreeGenerator.h>
#include <multiplier/ui/ITreeExplorerModel.h>

namespace mx::gui {

ITreeExplorerModel::~ITreeExplorerModel(void) {}

ITreeExplorerModel *ITreeExplorerModel::Create(QObject *parent) {
  return new TreeExplorerModel(parent);
}

ITreeExplorerModel *ITreeExplorerModel::CreateFrom(const QModelIndex &root_item,
                                                   QObject *parent) {
  auto &internal_source_model =
      *static_cast<const TreeExplorerModel *>(root_item.model());

  return new TreeExplorerModel(internal_source_model, root_item, parent);
}

}  // namespace mx::gui
