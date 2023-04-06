/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IReferenceExplorer.h>
#include <multiplier/ui/ISearchWidget.h>
#include <multiplier/ui/ICodeView.h>

namespace mx::gui {

//! A text-based implementation for the IReferenceExplorer interface
class TextBasedReferenceExplorer final : public IReferenceExplorer {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~TextBasedReferenceExplorer() override;

  //! \copybrief IReferenceExplorer::Model
  virtual IReferenceExplorerModel *Model() override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  TextBasedReferenceExplorer(IReferenceExplorerModel *model, QWidget *parent);

  //! Initializes the internalwidgets
  void InitializeWidgets(IReferenceExplorerModel *model);

 private slots:
  //! Code view event handler
  void OnTokenTriggered(const ICodeView::TokenAction &token_action,
                        const CodeModelIndex &index);

  //! Code view event handler
  void OnCursorMoved(const CodeModelIndex &index);

  friend class ReferenceExplorer;
};

}  // namespace mx::gui
