// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QDockWidget>
#include <QTimer>

namespace mx::gui {

//! A wrapper class that turns a widget into a dock widget
template <typename Widget>
class DockWidgetContainer final : public QDockWidget {
 public:
  //! Constructor
  template <typename... Args>
  DockWidgetContainer(Args &&...args)
      : QDockWidget(nullptr),
        d(new PrivateData) {
    d->wrapped_widget = new Widget(std::forward<Args>(args)...);

    setWidget(d->wrapped_widget);
    setAllowedAreas(Qt::AllDockWidgetAreas);

    OnUpdateTitle();
    connect(&d->title_update_timer, &QTimer::timeout, this,
            &DockWidgetContainer<Widget>::OnUpdateTitle);

    d->title_update_timer.start(500);
  }

  //! Destructor
  virtual ~DockWidgetContainer() = default;

  //! Returns the wrapped widget
  Widget *GetWrappedWidget() {
    return d->wrapped_widget;
  }

  //! Disabled copy constructor
  DockWidgetContainer(const DockWidgetContainer &) = delete;

  //! Disabled copy assignment operator
  DockWidgetContainer &operator=(const DockWidgetContainer &) = delete;

 private:
  //! Private class data
  struct PrivateData final {
    QTimer title_update_timer;
    Widget *wrapped_widget{nullptr};
  };

  std::unique_ptr<PrivateData> d;

 private slots:
  //! Updates the window title at regular intervals
  void OnUpdateTitle() {
    setWindowTitle(d->wrapped_widget->windowTitle());
  }
};

}  // namespace mx::gui
