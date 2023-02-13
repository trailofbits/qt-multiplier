/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IReferenceExplorer.h>

namespace mx::gui {

//! The implementation for the IReferenceExplorer interface
class ReferenceExplorer final : public IReferenceExplorer {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~ReferenceExplorer() override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  ReferenceExplorer(IReferenceExplorerModel *model, QWidget *parent);

  //! Initializes the internalwidgets
  void InitializeWidgets();

  //! Installs the given model
  void InstallModel(IReferenceExplorerModel *model);

 private slots:
  //! Used to expand and resize the items after a model reset
  void OnModelReset();

  friend class IReferenceExplorer;
};

}  // namespace mx::gui
