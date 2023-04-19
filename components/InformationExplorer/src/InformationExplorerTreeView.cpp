/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "InformationExplorerTreeView.h"
#include "InformationExplorerModel.h"
#include "InformationExplorerItemDelegate.h"

#include <multiplier/ui/IInformationExplorerModel.h>
#include <multiplier/ui/CodeViewTheme.h>

#include <QPainter>

namespace mx::gui {

InformationExplorerTreeView::InformationExplorerTreeView(QWidget *parent)
    : QTreeView(parent) {

  setItemDelegate(new InformationExplorerItemDelegate());
}

InformationExplorerTreeView::~InformationExplorerTreeView() {}

void InformationExplorerTreeView::drawRow(QPainter *painter,
                                          const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const {

  auto code_view_theme = GetDefaultCodeViewTheme(true);
  auto background_color = (option.state & QStyle::State_Selected) != 0
                              ? code_view_theme.selected_line_background_color
                              : code_view_theme.default_background_color;

  painter->fillRect(option.rect, QBrush(background_color));
  QTreeView::drawRow(painter, option, index);
}

}  // namespace mx::gui
