// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/ui/ICodeView.h>
#include <multiplier/ui/ITreeGenerator.h>
#include <multiplier/ui/PopupWidgetContainer.h>

#include <QWidget>

namespace mx::gui {

class IGlobalHighlighter;
class IMacroExplorer;
class IGeneratorModel;

//! A container for a ReferenceExplorerView and the linked ICodeView
class ReferenceExplorer final : public QWidget {
  Q_OBJECT

 public:
  //! Constructor
  ReferenceExplorer(const Index &index,
                    const FileLocationCache &file_location_cache,
                    std::shared_ptr<ITreeGenerator> generator,
                    const bool &show_code_preview,
                    IGlobalHighlighter &highlighter,
                    IMacroExplorer &macro_explorer, QWidget *parent = nullptr);

  //! Destructor
  virtual ~ReferenceExplorer() override;

  //! Returns the active model
  IGeneratorModel *Model();

  //! Disabled copy constructor
  ReferenceExplorer(const ReferenceExplorer &) = delete;

  //! Disabled copy assignment operator
  ReferenceExplorer &operator=(const ReferenceExplorer &) = delete;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Initializes the internal widgets
  void InitializeWidgets(mx::Index index,
                         mx::FileLocationCache file_location_cache,
                         std::shared_ptr<ITreeGenerator> generator,
                         const bool &show_code_preview,
                         IGlobalHighlighter &highlighter,
                         IMacroExplorer &macro_explorer);

  //! Schedules a post-update scroll-to-line operation
  void SchedulePostUpdateLineScrollCommand(unsigned line_number);

  //! Returns a previously scheduled scroll-to-line operation, if any
  std::optional<unsigned> GetScheduledPostUpdateLineScrollCommand();

  //! Updates the code preview using the given model index
  void UpdateCodePreview(const QModelIndex &index);

 private slots:
  //! Schedules a code model update whenever a reference is clicked
  void OnReferenceExplorerSelectedItemChanged(const QModelIndex &index);

  //! Used to do the first time initialization of the code preview
  void OnRowsInserted();

  //! Called when the model resolves the new name of the tree.
  void OnTreeNameChanged(QString new_name);

 public slots:
  //! Enables or disables the browser mode of the inner code view
  void SetBrowserMode(const bool &enabled);

 signals:
  //! The forwarded IReferenceExplorerView::SelectedItemChanged signal
  void SelectedItemChanged(const QModelIndex &index);

  //! The forwarded IReferenceExplorerView::ItemActivated signal
  void ItemActivated(const QModelIndex &index);

  //! The forwarded ICodeView::TokenTriggered
  void TokenTriggered(const ICodeView::TokenAction &token_action,
                      const QModelIndex &index);
};

//! A popup version of the ReferenceExplorer
using PopupReferenceExplorer = PopupWidgetContainer<ReferenceExplorer>;

}  // namespace mx::gui
