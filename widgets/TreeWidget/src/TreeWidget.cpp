/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Widgets/TreeWidget.h>

#include <QHeaderView>
#include <QMouseEvent>
#include <QPainter>

namespace mx::gui {

TreeWidget::TreeWidget(QWidget *parent)
    : QTreeView(parent) {

  last_mouse_event_time.start();   

  setAlternatingRowColors(false);
  setSelectionBehavior(QAbstractItemView::SelectRows);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setTextElideMode(Qt::TextElideMode::ElideRight);

  // The auto scroll takes care of keeping the active item within the
  // visible viewport region. This is true for mouse clicks but also
  // keyboard navigation (i.e. arrow keys, page up/down, etc).
  setAutoScroll(false);

  // Smooth scrolling.
  setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

  // We'll potentially have a bunch of columns depending on the configuration,
  // so make sure they span to use all available space.
  header()->setStretchLastSection(true);

  // Don't let double click expand things in three; we capture double click so
  // that we can make it open up the use in the code.
  setExpandsOnDoubleClick(false);

  // Disallow multiple selection. If we have grouping by file enabled, then when
  // a user clicks on a file name, we instead jump down to the first entry
  // grouped under that file. This is to make using the up/down arrows easier.
  setSelectionBehavior(QAbstractItemView::SelectRows);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setAllColumnsShowFocus(true);
  setTreePosition(0);   
}

TreeWidget::~TreeWidget(void) {}

//! Allows us to detect 
void TreeWidget::mousePressEvent(QMouseEvent *event) {
  last_mouse_event_time.restart();
  last_mouse_event_flags = event->flags();
  QTreeView::mousePressEvent(event);
}

std::pair<qint64, Qt::MouseEventFlags> TreeWidget::GetLastMouseEvent(void) {
  auto lt = last_mouse_event_time.elapsed();
  auto lf = last_mouse_event_flags;
  last_mouse_event_flags = {};
  return {lt, lf};
}

void TreeWidget::drawRow(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const {

  auto background_role_var = index.data(Qt::BackgroundRole);
  if (background_role_var.isValid()) {
    const auto &background_role = background_role_var.value<QColor>();
    painter->fillRect(option.rect, QBrush(background_role));
  }

  QTreeView::drawRow(painter, option, index);
}

}  // namespace mx::gui
