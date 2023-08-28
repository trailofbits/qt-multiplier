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
  InformationExplorer(IInformationExplorerModel *model, QWidget *parent,
                      IGlobalHighlighter *global_highlighter,
                      const bool &enable_history);

  //! Initializes the internal widgets
  void InitializeWidgets(IInformationExplorerModel *model,
                         const bool &enable_history);

  //! Installs the specified model
  void InstallModel(IInformationExplorerModel *model,
                    IGlobalHighlighter *global_highlighter);

 private slots:
  //! Used to auto-expand nodes at each model reset
  void OnModelReset();

  //! Called when new rows are inserted
  void OnRowsInserted(const QModelIndex &parent, int first, int last);

  //! Called when the data in the model changes
  void OnHighlightModelDataChange(const QModelIndex &top_left,
                                  const QModelIndex &bottom_right,
                                  const QList<int> &roles);

  //! Called by the ISearchWidget component whenever search options change
  void OnSearchParametersChange(
      const ISearchWidget::SearchParameters &search_parameters);

  //! Called when the seletion in the tree view changes
  void OnCurrentItemChanged(const QModelIndex &current_index,
                            const QModelIndex &);

  //! Called when the history widget is interacted with
  void OnHistoryNavigationEntitySelected(RawEntityId original_id,
                                         RawEntityId canonical_id);

  void ExpandAllNodes(const QModelIndex &parent);

  friend class IInformationExplorer;
};

}  // namespace mx::gui
