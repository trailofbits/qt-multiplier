// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include "Types.h"

#include <multiplier/ui/ICodeView.h>
#include <multiplier/ui/IReferenceExplorerModel.h>

#include <multiplier/Index.h>

#include <QMainWindow>
#include <QMimeData>

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
  void CreateReferenceExplorerDock();
  void CreateNewReferenceExplorer(QString window_title);
  void CreateCodeView();
  void OpenEntityRelatedToToken(const CodeModelIndex &index);
  void OpenEntityCode(RawEntityId entity_id);
  void OpenEntityInfo(RawEntityId entity_id, bool force = false);
  void OpenTokenContextMenu(CodeModelIndex index);
  void OpenTokenReferenceExplorer(CodeModelIndex index);
  void OpenTokenTaintExplorer(CodeModelIndex index);
  void OpenTokenEntityInfo(CodeModelIndex index);
  void OpenReferenceExplorer(RawEntityId entity_id,
                             IReferenceExplorerModel::ExpansionMode mode);
  void CloseTokenReferenceExplorer();
  ICodeView *CreateNewCodeView(RawEntityId file_entity_id, QString tab_name);

  ICodeView *
  GetOrCreateFileCodeView(RawEntityId file_id,
                          std::optional<QString> opt_tab_name = std::nullopt);

  void AboutToChangeToCodeView(QWidget *next_tab);

  void UpdateHistoryMenus();

  void UpdateCurrentCodeTabLocation(QVariant entity_id);

  static std::optional<QString> HistoryLabel(
      const FileLocationCache &file_location_cache,
      const VariantEntity &entity);

  void AddToHistory(QVariant entity_id);

  void NavigateBackToHistoryItem(History::ItemList::iterator item_it);
  void NavigateForwardToHistoryItem(History::ItemList::iterator item_it);

  void OnTokenTriggered(const ICodeView::TokenAction &token_action,
                        const CodeModelIndex &index);

 private slots:
  void OnIndexViewFileClicked(RawEntityId file_id, QString file_name,
                              Qt::KeyboardModifiers modifiers,
                              Qt::MouseButtons buttons);

  void OnMainCodeViewTokenTriggered(const ICodeView::TokenAction &token_action,
                                    const CodeModelIndex &index);

  void OnMainCodeViewCursorMoved(const CodeModelIndex &index);

  void OnPreviewCodeViewTokenTriggered(
      const ICodeView::TokenAction &token_action,
      const CodeModelIndex &index);

  void OnReferenceExplorerItemActivated(const QModelIndex &index);

  void OnCodeViewContextMenuActionTriggered(QAction *action);
  void OnToggleWordWrap(bool checked);
  void OnQuickRefExplorerSaveAllClicked(QMimeData *mime_data,
                                        const QString &window_title,
                                        const bool &as_new_tab);
  void OnReferenceExplorerTabBarClose(int index);
  void OnReferenceExplorerTabBarDoubleClick(int index);
  void OnCodeViewTabBarClose(int index);
  void OnCodeViewTabClicked(int index);
  void OnEntityExplorerEntityClicked(RawEntityId entity_id);

  void OnNavigateBack();
  void OnNavigateForward();
  void OnNavigateBackToHistoryItem(QAction *action);
  void OnNavigateForwardToHistoryItem(QAction *action);
};

}  // namespace mx::gui
