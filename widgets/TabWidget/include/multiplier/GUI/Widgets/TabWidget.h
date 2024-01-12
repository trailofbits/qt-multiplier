/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QTabWidget>

namespace mx::gui {

//! A QTabWidget that supports touchpad scrolling
class TabWidget final : public QTabWidget {
  Q_OBJECT

  using QTabWidget::addTab;
  using QTabWidget::insertTab;
  using QTabWidget::removeTab;

  void TrackTitle(QWidget *widget);

 public:
  //! Constructor
  TabWidget(QWidget *parent = nullptr);

  //! Destructor
  virtual ~TabWidget(void);

  void AddTab(QWidget *widget, bool update_title=true);
  void InsertTab(int index, QWidget *widget, bool update_title=true);
  void RemoveTab(int index);
};

}  // namespace mx::gui
