/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "Utils.h"
#include "InformationExplorerModel.h"

namespace mx::gui {

bool ShouldPaintAsTokens(const QModelIndex &index) {
  if (!index.isValid()) {
    return false;
  }

  auto token_range_var = index.data(InformationExplorerModel::TokenRangeRole);
  if (!token_range_var.isValid()) {
    return false;
  }

  auto force_text_paint_var =
      index.data(InformationExplorerModel::ForceTextPaintRole);

  if (force_text_paint_var.isValid() && force_text_paint_var.toBool()) {
    return false;
  }

  return true;
}

bool ShouldAutoExpand(const QModelIndex &index) {
  if (!index.isValid()) {
    return true;
  }

  auto auto_expand_var = index.data(InformationExplorerModel::AutoExpandRole);
  if (!auto_expand_var.isValid()) {
    return true;
  }

  return auto_expand_var.toBool();
}

}  // namespace mx::gui
