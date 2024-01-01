/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "InformationExplorerTreeView.h"
#include "InformationExplorerModel.h"
#include "InformationExplorerItemDelegate.h"

#include "InformationExplorerModel.h"

#include <QPainter>

namespace mx::gui {

InformationExplorerTreeView::InformationExplorerTreeView(QWidget *parent)
    : QTreeView(parent) {

  connect(&ThemeManager::Get(), &ThemeManager::ThemeChanged, this,
          &InformationExplorerTreeView::OnThemeChange);

  InstallItemDelegate();
}

InformationExplorerTreeView::~InformationExplorerTreeView() {}

void InformationExplorerTreeView::drawRow(QPainter *painter,
                                          const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const {

  auto code_view_theme = ThemeManager::Get().GetCodeViewTheme();
  auto background_color = (option.state & QStyle::State_Selected) != 0
                              ? code_view_theme.selected_line_background_color
                              : code_view_theme.default_background_color;

  if (auto highlight_color_var = index.data(Qt::BackgroundRole);
      highlight_color_var.isValid()) {

    background_color = qvariant_cast<QColor>(highlight_color_var);
  }

  painter->fillRect(option.rect, QBrush(background_color));

  QTreeView::drawRow(painter, option, index);
}

void InformationExplorerTreeView::InstallItemDelegate() {
  auto old_item_delegate = itemDelegate();
  if (old_item_delegate != nullptr) {
    old_item_delegate->deleteLater();
  }

  setItemDelegate(new InformationExplorerItemDelegate());
}

void InformationExplorerTreeView::OnThemeChange(
    const QPalette &, const CodeViewTheme &code_view_theme) {
  InstallItemDelegate();

  QFont font(code_view_theme.font_name);
  setFont(font);

  update();
}

}  // namespace mx::gui
