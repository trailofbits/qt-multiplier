/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QTabBar>

namespace mx::gui {

class TabBar Q_DECL_FINAL : public QTabBar {
  Q_OBJECT

 public:
  TabBar(QWidget *parent = nullptr)
      : QTabBar(parent) {}

  virtual ~TabBar(void);

  TabBar(const TabBar &) = delete;
  TabBar(TabBar &&) noexcept = delete;
  TabBar &operator=(const TabBar &) = delete;
  TabBar &operator=(TabBar &&) noexcept = delete;

 protected:
  void wheelEvent(QWheelEvent *event) Q_DECL_FINAL;
};

}  // namespace mx::gui
