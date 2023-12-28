// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Widgets/DockWidget.h>

#include <QTimer>

namespace mx::gui {

//! Private class data
struct DockWidget::PrivateData final {
  QTimer title_update_timer;
  QWidget *wrapped_widget{nullptr};
};

DockWidget::~DockWidget(void) {}

DockWidget::DockWidget(QWidget *parent)
    : QDockWidget(parent),
      d(new PrivateData) {
  setAllowedAreas(Qt::AllDockWidgetAreas);

  connect(&d->title_update_timer, &QTimer::timeout,
          this, &DockWidget::OnUpdateTitle);
}

void DockWidget::SetWrappedWidget(QWidget *wrapped_widget) {
  d->wrapped_widget = wrapped_widget;
  setWidget(d->wrapped_widget);

  OnUpdateTitle();

  d->title_update_timer.start(500);
}

//! Returns the wrapped widget
QWidget *DockWidget::WrappedWidget(void) {
  return d->wrapped_widget;
}

//! Updates the window title at regular intervals
void DockWidget::OnUpdateTitle(void) {
  setWindowTitle(d->wrapped_widget->windowTitle());
}

}  // namespace mx::gui
