/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QAbstractItemModel>

namespace mx::gui {

bool ShouldPaintAsTokens(const QModelIndex &index);
bool ShouldAutoExpand(const QModelIndex &index);

}  // namespace mx::gui
