/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ThemedItemModel.h"

namespace mx::gui {

QVariant ThemedItemModel::data(const QModelIndex &index, int role) const {
  if (role == Qt::BackgroundRole) {
    return background_color;
  } else {
    return this->QIdentityProxyModel::data(index, role);
  }
}

}  // namespace mx::gui
