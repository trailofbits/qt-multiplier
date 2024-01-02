/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Widgets/TabWidget.h>

#include <QTimer>

#include <unordered_map>

#include "TabBar.h"

namespace mx::gui {

struct TabWidget::PrivateData {
  std::unordered_map<QWidget *, QTimer> title_update_timers;
};

TabWidget::~TabWidget(void) {}

TabWidget::TabWidget(QWidget *parent)
    : QTabWidget(parent),
      d(new PrivateData) {
  setTabBar(new TabBar(this));
}

void TabWidget::TrackTitle(QWidget *widget) {
  auto &timer = d->title_update_timers[widget];
  connect(&timer, &QTimer::timeout,
          [=, this] (void) {
            d->title_update_timers.erase(widget);
            auto index = indexOf(widget);
            if (index == -1) {
              return;
            }

            setTabText(index, widget->windowTitle());
          });

  timer.start(500);
}

void TabWidget::AddTab(QWidget *widget) {
  addTab(widget, widget->windowTitle());
  TrackTitle(widget);
  
}

void TabWidget::InsertTab(int index, QWidget *widget) {
  insertTab(index, widget, widget->windowTitle());
  TrackTitle(widget);
}

void TabWidget::RemoveTab(int index) {
  if (auto tab = widget(index)) {
    d->title_update_timers[tab].stop();
    d->title_update_timers.erase(tab);
    removeTab(index);
  }
}

}  // namespace mx::gui
