/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IInformationExplorerModel.h>

#include <QWidget>

namespace mx::gui {

//! A widget that displays entity information
class IInformationExplorer : public QWidget {
  Q_OBJECT

 public:
  //! Factory function
  static IInformationExplorer *Create(IInformationExplorerModel *model,
                                      QWidget *parent = nullptr);

  //! Constructor
  IInformationExplorer(QWidget *parent) : QWidget(parent) {}

  //! Destructor
  virtual ~IInformationExplorer() override = default;

  //! Disable the copy constructor
  IInformationExplorer(const IInformationExplorer &) = delete;

  //! Disable copy assignment
  IInformationExplorer &operator=(const IInformationExplorer &) = delete;

 signals:
  //! Emitted whenever the selected item changes
  void SelectedItemChanged(const QModelIndex &current_index);
};

}  // namespace mx::gui
