// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>
#include <multiplier/GUI/Managers/ConfigManager.h>

#include <memory>

namespace mx::gui {

class IWindowManager;
class SpreadsheetModel;

class SpreadsheetExplorer Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~SpreadsheetExplorer(void);

  explicit SpreadsheetExplorer(ConfigManager &config_manager,
                               IWindowManager *parent = nullptr);

  void ActOnPrimaryClick(IWindowManager *manager,
                         const QModelIndex &index) Q_DECL_FINAL;

  void ActOnContextMenu(IWindowManager *manager, QMenu *menu,
                        const QModelIndex &index) Q_DECL_FINAL;

  bool eventFilter(QObject *obj, QEvent *event) Q_DECL_OVERRIDE;

  //! Load persisted sheets from the database. Call after restoreState.
  void LoadPersistedSheets(void);

 private:
  void CreateDockWidget(IWindowManager *manager);
  void CreateDocumentDock(IWindowManager *manager);
  void OpenSheetFromData(const ConfigManager::SheetData &sheet);
  void OpenDocumentViewer(SpreadsheetModel *model, int row, int col,
                          int doc_id = -1);
  void SaveCurrentDocument(void);
  void ShowReopenClosedSheetsMenu(void);

 private slots:
  void OnNewBlankSheet(const QVariant &data);
  void OnOpenInSpreadsheet(const QVariant &data);
  void OnOpenDocument(const QVariant &data);
  void OnIndexChanged(const ConfigManager &config_manager);
  void OnExternalSheetsChanged(void);
};

}  // namespace mx::gui
