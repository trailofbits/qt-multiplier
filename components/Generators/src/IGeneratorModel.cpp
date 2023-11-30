/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GeneratorModel.h"

#include <multiplier/ui/IGeneratorModel.h>

namespace mx::gui {

IGeneratorModel::~IGeneratorModel(void) {}

IGeneratorModel *IGeneratorModel::Create(QObject *parent) {
  return new GeneratorModel(parent);
}

IGeneratorModel *IGeneratorModel::CreateFrom(const QModelIndex &root_item,
                                                   QObject *parent) {
  auto &internal_source_model =
      *static_cast<const GeneratorModel *>(root_item.model());

  return new GeneratorModel(internal_source_model, root_item, parent);
}

}  // namespace mx::gui
