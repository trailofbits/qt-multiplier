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
class MxTreeView final : public QTreeView {
  Q_OBJECT

 public:
  //! Constructor
  MxTreeView(QWidget *parent = nullptr);

  //! Destructor
  virtual ~MxTreeView() override;

  //! Disabled copy constructor
  MxTreeView(MxTreeView &) = delete;

  //! Disabled copy assignment operator
  MxTreeView &operator=(const MxTreeView &) = delete;

 protected:
  //! Draws the background role on rows
  virtual void drawRow(QPainter *painter, const QStyleOptionViewItem &option,
                       const QModelIndex &index) const override;
};

}  // namespace mx::gui
