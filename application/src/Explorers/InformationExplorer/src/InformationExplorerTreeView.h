/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/GUI/ThemeManager.h>

#include <QTreeView>

namespace mx::gui {

class InformationExplorerTreeView final : public QTreeView {
  Q_OBJECT

 public:
  InformationExplorerTreeView(QWidget *parent);
  virtual ~InformationExplorerTreeView() override;

 protected:
  void drawRow(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

 private:
  void InstallItemDelegate();

 private slots:
  void OnThemeChange(const QPalette &palette,
                     const CodeViewTheme &code_view_theme);
};

}  // namespace mx::gui
