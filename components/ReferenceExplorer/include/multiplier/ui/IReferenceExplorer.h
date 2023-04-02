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

  //! Returns the active model
  virtual IReferenceExplorerModel *Model() = 0;

  //! Disabled copy constructor
  IReferenceExplorer(const IReferenceExplorer &) = delete;

  //! Disabled copy assignment operator
  IReferenceExplorer &operator=(const IReferenceExplorer &) = delete;

 signals:
  //! Emitted when the selected item has changed
  void SelectedItemChanged(const QModelIndex &index);

  //! Emitted when an item has been activated using the dedicated button
  void ItemActivated(const QModelIndex &index);
};

}  // namespace mx::gui
