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
  void CreateNewReferenceExplorer();
  void CreateCodeView();
  void OpenTokenContextMenu(const CodeModelIndex &index);
  void OpenTokenReferenceExplorer(const CodeModelIndex &index);
  void CloseTokenReferenceExplorer();

 private slots:
  void OnIndexViewFileClicked(const PackedFileId &file_id,
                              const std::string &file_name, bool double_click);

  void OnTokenClicked(const CodeModelIndex &index,
                      const Qt::MouseButton &mouse_button,
                      const Qt::KeyboardModifiers &modifiers,
                      bool double_click);

  void OnCodeViewContextMenuActionTriggered(QAction *action);
  void OnToggleWordWrap(bool checked);
  void OnQuickRefExplorerSaveAllClicked(QMimeData *mime_data,
                                        const bool &as_new_tab);
  void OnReferenceExplorerTabBarClose(int index);
};

}  // namespace mx::gui
