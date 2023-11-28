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
class MxTabWidget final : public QTabWidget {
  Q_OBJECT

 public:
  //! Constructor
  MxTabWidget(QWidget *parent = nullptr);

  //! Destructor
  virtual ~MxTabWidget() override;

  //! Disabled copy constructor
  MxTabWidget(MxTabWidget &) = delete;

  //! Disabled copy assignment operator
  MxTabWidget &operator=(const MxTabWidget &) = delete;
};

}  // namespace mx::gui
