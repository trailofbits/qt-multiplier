/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IInformationExplorer.h>
#include <multiplier/ui/ISearchWidget.h>

#include <QWidget>

namespace mx::gui {

//! A widget that displays entity information
class InformationExplorer final : public IInformationExplorer {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~InformationExplorer() override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  InformationExplorer(IInformationExplorerModel *model, QWidget *parent);

  //! Initializes the internal widgets
  void InitializeWidgets();

  //! Installs the specified model
  void InstallModel(IInformationExplorerModel *model);

  //! Expands all the nodes in the tree view
  void ExpandAllNodes();

 private slots:
  //! Used to auto-expand nodes at each model reset
  void OnModelReset();

  //! Called by the ISearchWidget component whenever search options change
  void OnSearchParametersChange(
      const ISearchWidget::SearchParameters &search_parameters);

  friend class IInformationExplorer;
};

}  // namespace mx::gui
