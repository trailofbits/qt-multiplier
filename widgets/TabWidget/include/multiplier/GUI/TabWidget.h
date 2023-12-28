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

 public:
  //! Constructor
  TabWidget(QWidget *parent = nullptr);

  //! Destructor
  virtual ~TabWidget(void);

  //! Disabled copy constructor
  TabWidget(const TabWidget &) = delete;
  TabWidget(TabWidget &&) noexcept = delete;

  //! Disabled copy assignment operator
  TabWidget &operator=(const TabWidget &) = delete;
  TabWidget &operator=(TabWidget &&) noexcept = delete;
};

}  // namespace mx::gui
