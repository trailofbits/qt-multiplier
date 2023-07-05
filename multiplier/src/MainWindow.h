// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include "PreviewableReferenceExplorer.h"

#include <multiplier/ui/ICodeView.h>
#include <multiplier/ui/IReferenceExplorerModel.h>

#include <multiplier/Index.h>

#include <QMainWindow>

namespace mx::gui {

class ICodeView;

class MainWindow final : public QMainWindow {
  Q_OBJECT

 public:
  MainWindow();
  virtual ~MainWindow() override;

  MainWindow(const MainWindow &) = delete;
  MainWindow &operator=(const MainWindow &) = delete;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  void InitializeWidgets();
  void InitializeToolBar();
  void CreateProjectExplorerDock();
  void CreateEntityExplorerDock();
  void CreateInfoExplorerDock();
  void CreateMacroExplorerDock();
  void CreateReferenceExplorerDock();
  void CreateCodeView();
  void CreateGlobalHighlighter();
  void OpenEntityRelatedToToken(const QModelIndex &index);
  void OpenEntityCode(RawEntityId entity_id, bool canonicalize = true);
  void OpenEntityInfo(RawEntityId entity_id);
  void OpenTokenContextMenu(const QModelIndex &index);
  void OpenTokenReferenceExplorer(const QModelIndex &index);
  void OpenTokenTaintExplorer(const QModelIndex &index);
  void OpenTokenEntityInfo(const QModelIndex &index);
  void ExpandMacro(const QModelIndex &index);
  void OpenReferenceExplorer(RawEntityId entity_id,
                             IReferenceExplorerModel::ExpansionMode mode);
  void OpenCodePreview(const QModelIndex &index);
  void CloseQuickRefExplorerPopup();
  void CloseCodePreviewPopup();
  void CloseAllPopups();

  //! Creates the menus needed to configure the reference explorer
  void CreateRefExplorerMenuOptions();

  //! Tell the history back/forward button widget that our current location has
  //! changed.
  void UpdateLocatiomFromWidget(QWidget *widget);

  ICodeView *
  CreateNewCodeView(RawEntityId file_entity_id, QString tab_name,
                    const std::optional<QString> &opt_file_path = std::nullopt);

  ICodeView *GetOrCreateFileCodeView(
      RawEntityId file_id, std::optional<QString> opt_tab_name = std::nullopt,
      const std::optional<QString> &opt_file_path = std::nullopt);

 private slots:
  //! Called when a file name in the project explorer (list of indexed files) is
  //! clicked.
  void OnProjectExplorerFileClicked(RawEntityId file_id, QString file_name,
                                    const QString &file_path);

  //! Called when we interact with a token in a main file view, or in a code
  //! preview, e.g. a reference browser code view.
  void OnTokenTriggered(const ICodeView::TokenAction &token_action,
                        const QModelIndex &index);

  //! Called when we user moves their cursor, or when an action triggered by the
  //! user causes the view itself to re-position the cursor in a file code view.
  //! Wherever the new cursor position is, we capture the token under the cursor
  //! and record it to the current file view's tab as its current location, so
  //! that if we switch away from the tab, then we can store that current location
  //! into the history.
  void OnMainCodeViewCursorMoved(const QModelIndex &index);

  void OnReferenceExplorerItemActivated(const QModelIndex &index);

  void OnCodeViewContextMenuActionTriggered(QAction *action);
  void OnToggleWordWrap(bool checked);
  void SaveReferenceExplorer(PreviewableReferenceExplorer *reference_explorer);
  void OnReferenceExplorerTabBarClose(int index);
  void OnReferenceExplorerTabBarDoubleClick(int index);
  void OnCodeViewTabBarClose(int index);
  void OnCodeViewTabClicked(int index);

  //! Called when an entity in the entity explorer / filter, which allows us to
  //! search for entities by name/type, is clicked.
  void OnOpenEntity(RawEntityId entity_id);

  //! Called when the history menu is used to go back/forward to some specific
  //! entity ID.
  void OnHistoryNavigationEntitySelected(RawEntityId original_id,
                                         RawEntityId canonical_id);
  void OnInformationExplorerSelectionChange(const QModelIndex &index);
  void OnCloseActiveCodeViewTab();
  void OnCloseActiveRefExplorerTab();

  //! Called when the view->theme->dark action is selected
  void OnSetDarkTheme();

  //! Called when the view->theme->light action is selected
  void OnSetLightTheme();

  //! Called when toggling the code preview setting of the ref explorer
  void OnRefExplorerCodePreviewToggled(const bool &checked);
};

}  // namespace mx::gui
