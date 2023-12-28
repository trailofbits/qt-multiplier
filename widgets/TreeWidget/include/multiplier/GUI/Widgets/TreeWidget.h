/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QTreeView>

namespace mx::gui {

//! A QTreeView class that can paint the background role on rows
class TreeWidget Q_DECL_FINAL : public QTreeView {
  Q_OBJECT

 public:
  //! Constructor
  TreeWidget(QWidget *parent = nullptr);

  //! Destructor
  virtual ~TreeWidget(void);

  TreeWidget(const TreeWidget &) = delete;
  TreeWidget(TreeWidget &&) noexcept = delete;
  TreeWidget &operator=(const TreeWidget &) = delete;
  TreeWidget &operator=(TreeWidget &&) noexcept = delete;

 protected:
  //! Draws the background role on rows
  void drawRow(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const Q_DECL_FINAL;
};

}  // namespace mx::gui
