/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IReferenceExplorerModel.h>

#include <QWidget>

namespace mx::gui {

//! The reference explorer widget
class IReferenceExplorer : public QWidget {
  Q_OBJECT

 public:
  //! Factory method
  static IReferenceExplorer *Create(IReferenceExplorerModel *model,
                                    QWidget *parent = nullptr);

  //! Constructor
  IReferenceExplorer(QWidget *parent) : QWidget(parent) {}

  //! Destructor
  virtual ~IReferenceExplorer() override = default;

  //! Disabled copy constructor
  IReferenceExplorer(const IReferenceExplorer &) = delete;

  //! Disabled copy assignment operator
  IReferenceExplorer &operator=(const IReferenceExplorer &) = delete;
};

}  // namespace mx::gui
