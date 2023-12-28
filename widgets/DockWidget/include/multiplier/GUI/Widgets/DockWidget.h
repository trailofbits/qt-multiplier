// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QDockWidget>

namespace mx::gui {

//! A wrapper class that turns a widget into a dock widget
class DockWidget final : public QDockWidget {
  Q_OBJECT

  struct PrivateData;
  const std::unique_ptr<PrivateData> d;

  using QDockWidget::setWidget;

 public:
  //! Constructor
  DockWidget(QWidget *parent = nullptr);

  //! Destructor
  virtual ~DockWidget(void);

  void SetWrappedWidget(QWidget *wrapped_widget);
  
  //! Returns the wrapped widget
  QWidget *WrappedWidget(void);

 private slots:
  //! Updates the window title at regular intervals
  void OnUpdateTitle(void);
};

}  // namespace mx::gui
