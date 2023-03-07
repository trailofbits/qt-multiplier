// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/ui/IReferenceExplorerModel.h>

#include <QWidget>

namespace mx::gui {

//! A container for a ReferenceExplorer and the linked ICodeView
class PreviewableReferenceExplorer final : public QWidget {
  Q_OBJECT

 public:
  //! Constructor
  PreviewableReferenceExplorer(mx::Index index,
                               mx::FileLocationCache file_location_cache,
                               IReferenceExplorerModel *model,
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

 protected:
  //! The resize event is used to determine whether to show the preview
  virtual void resizeEvent(QResizeEvent *event) override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Initializes the internal widgets
  void InitializeWidgets(mx::Index index,
                         mx::FileLocationCache file_location_cache,
                         IReferenceExplorerModel *model);

  //! Based on the widget size, it will show or hide the code preview
  void UpdateLayout();

  //! Schedules a post-update scroll-to-line operation
  void SchedulePostUpdateLineScrollCommand(unsigned line_number);

  //! Returns a previously scheduled scroll-to-line operation, if any
  std::optional<unsigned> GetScheduledPostUpdateLineScrollCommand();

 private slots:
  //! Schedules a code model update whenever a reference is clicked
  void OnReferenceExplorerItemClicked(const QModelIndex &index);

  //! Handles scheduled scroll-to-line operations for the code view
  void OnCodeViewDocumentChange();

 signals:
  //! The forwarded IReferenceExplorer::ItemClicked signal
  void ItemClicked(const QModelIndex &index);

  //! The forwarded IReferenceExplorer::ItemActivated signal
  void ItemActivated(const QModelIndex &index);
};

}  // namespace mx::gui
