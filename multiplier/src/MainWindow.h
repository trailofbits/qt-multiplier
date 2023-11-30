// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/ui/PreviewableTreeExplorerView.h>
#include <multiplier/ui/ICodeView.h>

#include <multiplier/Index.h>

#include <QMainWindow>

namespace mx::gui {

class ICodeView;
class ITreeGenerator;

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
  void CreateTreeExplorerDock();
  void CreateCodeView();
  void CreateGlobalHighlighter();
  void OpenEntityRelatedToToken(const QModelIndex &index);
  void OpenEntityCode(RawEntityId entity_id, bool canonicalize = true);
  void OpenEntityInfo(RawEntityId entity_id);
  void OpenTokenContextMenu(const QModelIndex &index);
  void OpenCallHierarchy(const QModelIndex &index);
  void OpenTokenEntityInfo(const QModelIndex &index, const bool &new_window);
  void ExpandMacro(const QModelIndex &index);
  void OpenTreeExplorer(std::shared_ptr<ITreeGenerator> generator);
  std::optional<QRect> GetPopupPlacement();
  void OpenCodePreview(const QModelIndex &index, const bool &as_new_window);
  void CloseQuickTreeExplorerPopup();
  void CloseCodePreviewPopup();
  void CloseAllPopups();

  //! Creates the menus needed to configure the reference explorer
  void CreateTreeExplorerMenuOptions();

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

  void OnTreeExplorerItemActivated(const QModelIndex &index);

  void OnCodeViewContextMenuActionTriggered(QAction *action);
  void OnToggleWordWrap(bool checked);
  void SaveTreeExplorer(PreviewableTreeExplorerView *reference_explorer);
  void OnTreeExplorerTabBarClose(int index);
  void OnTreeExplorerTabBarDoubleClick(int index);
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
  void OnCloseActiveTreeExplorerTab();

  //! Called when the view->theme->dark action is selected
  void OnSetDarkTheme();

  //! Called when the view->theme->light action is selected
  void OnSetLightTheme();

  //! Called when toggling the code preview setting of the ref explorer
  void OnTreeExplorerCodePreviewToggled(const bool &checked);

  //! Called by theme manager
  void OnThemeChange(const QPalette &, const CodeViewTheme &);

  //! Updates the icons according to the current theme
  void UpdateIcons();

  //! Called when the browser mode action is interacted with
  void OnBrowserModeToggled();

  //! Called when one of the tree explorers wants to extract a subtree
  //! in a new window
  void OnExtractTreeExplorerSubtree(const QModelIndex &index);

 signals:
  //! Emitted when the browse mode is toggled
  void BrowserModeToggled(const bool &enabled);
};

}  // namespace mx::gui
