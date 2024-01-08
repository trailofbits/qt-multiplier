/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QElapsedTimer>
#include <QMouseEvent>
#include <QTreeView>
#include <utility>

namespace mx::gui {

//! A QTreeView class that can paint the background role on rows
class TreeWidget Q_DECL_FINAL : public QTreeView {
  Q_OBJECT

  QElapsedTimer last_mouse_event_time;
  Qt::MouseEventFlags last_mouse_event_flags{};

 public:
  //! Constructor
  TreeWidget(QWidget *parent = nullptr);

  //! Destructor
  virtual ~TreeWidget(void);

  std::pair<qint64, Qt::MouseEventFlags> GetLastMouseEvent(void);

 protected:
  //! Allows us to detect 
  void mousePressEvent(QMouseEvent *event) Q_DECL_FINAL;

  //! Draws the background role on rows
  void drawRow(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const Q_DECL_FINAL;
};

}  // namespace mx::gui
