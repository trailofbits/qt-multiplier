// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/ui/IReferenceExplorer.h>
#include <multiplier/ui/ICodeView.h>
#include <multiplier/ui/IGlobalHighlighter.h>

#include <QWidget>

namespace mx::gui {

//! A container for a ReferenceExplorer and the linked ICodeView
class PreviewableReferenceExplorer final : public QWidget {
  Q_OBJECT

 public:
  //! Constructor
  PreviewableReferenceExplorer(const Index &index,
                               const FileLocationCache &file_location_cache,
                               IReferenceExplorerModel *model,
                               const IReferenceExplorer::Mode &mode,
                               IGlobalHighlighter &highlighter,
                               QWidget *parent = nullptr);

  //! Destructor
  virtual ~PreviewableReferenceExplorer() override;

  //! Returns the active model
  IReferenceExplorerModel *Model();

  //! Disabled copy constructor
  PreviewableReferenceExplorer(const PreviewableReferenceExplorer &) = delete;

  //! Disabled copy assignment operator
  PreviewableReferenceExplorer &
  operator=(const PreviewableReferenceExplorer &) = delete;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Initializes the internal widgets
  void InitializeWidgets(mx::Index index,
                         mx::FileLocationCache file_location_cache,
                         IReferenceExplorerModel *model,
                         const IReferenceExplorer::Mode &mode,
                         IGlobalHighlighter &highlighter);

  //! Schedules a post-update scroll-to-line operation
  void SchedulePostUpdateLineScrollCommand(unsigned line_number);

  //! Returns a previously scheduled scroll-to-line operation, if any
  std::optional<unsigned> GetScheduledPostUpdateLineScrollCommand();

  //! Updates the code preview using the given model index
  void UpdateCodePreview(const QModelIndex &index);

 private slots:
  //! Schedules a code model update whenever a reference is clicked
  void OnReferenceExplorerSelectedItemChanged(const QModelIndex &index);

  //! Handles scheduled scroll-to-line operations for the code view
  void OnCodeViewDocumentChange();

  //! Used to do the first time initialization of the code preview
  void OnRowsInserted();

 signals:
  //! The forwarded IReferenceExplorer::SelectedItemChanged signal
  void SelectedItemChanged(const QModelIndex &index);

  //! The forwarded IReferenceExplorer::ItemActivated signal
  void ItemActivated(const QModelIndex &index);

  //! The forwarded ICodeView::TokenTriggered
  void TokenTriggered(const ICodeView::TokenAction &token_action,
                      const QModelIndex &index);
};

}  // namespace mx::gui
