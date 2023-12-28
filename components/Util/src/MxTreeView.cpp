/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/MxTreeView.h>

#include <QPainter>

namespace mx::gui {

MxTreeView::MxTreeView(QWidget *parent) : QTreeView(parent) {}

MxTreeView::~MxTreeView() {}

void MxTreeView::drawRow(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const {

  auto background_role_var = index.data(Qt::BackgroundRole);
  if (background_role_var.isValid()) {
    const auto &background_role = background_role_var.value<QColor>();
    painter->fillRect(option.rect, QBrush(background_role));
  }

  QTreeView::drawRow(painter, option, index);
}

}  // namespace mx::gui
