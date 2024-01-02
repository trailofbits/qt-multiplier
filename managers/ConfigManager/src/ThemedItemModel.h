/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QColor>
#include <QIdentityProxyModel>

namespace mx::gui {

class ThemedItemModel Q_DECL_FINAL : public QIdentityProxyModel {
  Q_OBJECT

 public:
  QColor background_color;

  using QIdentityProxyModel::QIdentityProxyModel;

  virtual ~ThemedItemModel(void) = default;

  QVariant data(const QModelIndex &index, int role) const Q_DECL_FINAL;
};

}  // namespace mx::gui
