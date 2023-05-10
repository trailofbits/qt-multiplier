/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IReferenceExplorer.h>

namespace mx::gui {

//! The main implementation for the IReferenceExplorer interface
class ReferenceExplorer final : public IReferenceExplorer {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~ReferenceExplorer() override;

  //! \copybrief IReferenceExplorer::Model
  virtual IReferenceExplorerModel *Model() override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  ReferenceExplorer(IReferenceExplorerModel *model, const Mode &mode,
                    QWidget *parent, IGlobalHighlighter *global_highlighter);

  //! Initializes the internalwidgets
  void InitializeWidgets(IReferenceExplorerModel *model, const Mode &mode,
                         IGlobalHighlighter *global_highlighter);

  friend class IReferenceExplorer;
};

}  // namespace mx::gui
