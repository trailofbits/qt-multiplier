/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GeneratorView.h"

#include <multiplier/ui/IGeneratorView.h>

namespace mx::gui {

IGeneratorView *IGeneratorView::Create(QAbstractItemModel *model,
                                       const Configuration &config,
                                       QWidget *parent) {

  return new GeneratorView(model, config, parent);
}

}  // namespace mx::gui
