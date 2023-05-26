/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QTabWidget>

namespace mx::gui {

class MxTabWidget final : public QTabWidget {
  Q_OBJECT

 public:
  MxTabWidget(QWidget *parent = nullptr);
  virtual ~MxTabWidget() override;

  MxTabWidget(MxTabWidget &) = delete;
  MxTabWidget &operator=(const MxTabWidget &) = delete;
};

}  // namespace mx::gui
