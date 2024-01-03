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
    if (background_color.isValid()) {
      return background_color;
    }
  } else if (role == Qt::ForegroundRole) {
    if (foreground_color.isValid()) {
      return foreground_color;
    }
  }
  return this->QIdentityProxyModel::data(index, role);
}

}  // namespace mx::gui
