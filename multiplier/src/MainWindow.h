// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/Index.h>

#include <multiplier/ui/ICodeModel.h>

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
  void CreateFileTreeDock();
  void CreateReferenceExplorerDock();
  void CreateNewReferenceExplorer(QString window_title);
  void CreateCodeView();
  void OpenEntityRelatedToToken(CodeModelIndex index);
  void OpenTokenContextMenu(CodeModelIndex index);
  void OpenTokenReferenceExplorer(CodeModelIndex index);
  void OpenTokenReferenceExplorer(RawEntityId entity_id);
  void CloseTokenReferenceExplorer();
  ICodeView *CreateNewCodeView(RawEntityId file_entity_id, QString tab_name);

  ICodeView *
  GetOrCreateFileCodeView(RawEntityId file_id,
                          std::optional<QString> opt_tab_name = std::nullopt);

 private slots:
  void OnIndexViewFileClicked(RawEntityId file_id, QString file_name,
                              Qt::KeyboardModifiers modifiers,
                              Qt::MouseButtons buttons);

  void OnTokenClicked(CodeModelIndex index, Qt::MouseButtons mouse_button,
                      Qt::KeyboardModifiers modifiers, bool double_click);

  void OnReferenceExplorerItemClicked(const QModelIndex &index,
                                      const bool &middle_button);

  void OnCodeViewContextMenuActionTriggered(QAction *action);
  void OnToggleWordWrap(bool checked);
  void OnQuickRefExplorerSaveAllClicked(QMimeData *mime_data,
                                        const QString &window_title,
                                        const bool &as_new_tab);
  void OnReferenceExplorerTabBarClose(int index);
  void OnReferenceExplorerTabBarDoubleClick(int index);
  void OnCodeViewTabBarClose(int index);
};

}  // namespace mx::gui
