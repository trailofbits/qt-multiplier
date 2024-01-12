/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Widgets/TabWidget.h>

#include <unordered_map>

#include "TabBar.h"

namespace mx::gui {

TabWidget::~TabWidget(void) {}

TabWidget::TabWidget(QWidget *parent)
    : QTabWidget(parent) {
  setTabBar(new TabBar(this));
}

void TabWidget::TrackTitle(QWidget *widget) {
  connect(widget, &QWidget::windowTitleChanged,
          [=, this] (const QString &new_title) {
            auto index = indexOf(widget);
            if (index != -1) {
              setTabText(index, new_title);
            }
          });
}

void TabWidget::AddTab(QWidget *widget, bool update_title) {
  auto index = count();
  addTab(widget, widget->windowTitle());
  if (update_title) {
    TrackTitle(widget);
  }
  setCurrentIndex(index);
}

void TabWidget::InsertTab(int index, QWidget *widget, bool update_title) {
  insertTab(index, widget, widget->windowTitle());
  if (update_title) {
    TrackTitle(widget);
  }
  setCurrentIndex(0);
}

void TabWidget::RemoveTab(int index) {
  if (auto tab = widget(index)) {
    removeTab(index);
  }
}

}  // namespace mx::gui
