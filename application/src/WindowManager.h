// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IWindowManager.h>

#include <QDockWidget>
#include <QString>

#include <memory>

namespace mx::gui {

class MainWindow;

class WindowManager Q_DECL_FINAL: public IWindowManager {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:

  virtual ~WindowManager(void);

  explicit WindowManager(MainWindow *window);

  void AddToolBarWidget(QWidget *widget) Q_DECL_FINAL;

  QAction *AddToolBarButton(const QIcon &icon,
                            const NamedAction &action) Q_DECL_FINAL;

  QAction *AddDepressableToolBarButton(
      const QIcon &icon, const QString &name,
      const TriggerHandle &trigger) Q_DECL_FINAL;

  void AddCentralWidget(IWindowWidget *widget,
                        const CentralConfig &config) Q_DECL_FINAL;
  void AddDockWidget(IWindowWidget *widget,
                     const DockConfig &config) Q_DECL_FINAL;

  void OnPrimaryClick(const QModelIndex &index) Q_DECL_FINAL;
  void OnSecondaryClick(const QModelIndex &index) Q_DECL_FINAL;
  void OnKeyPress(const QKeySequence &keys,
                  const QModelIndex &index) Q_DECL_FINAL;
  
  QMainWindow *Window(void) const noexcept Q_DECL_FINAL;

  QMenu *Menu(const QString &menu_name) Q_DECL_FINAL;

 private:
  void CreateToolBarIfMissing(void);
  void RemoveDockWidget(QDockWidget *dock_widget);
};

}  // namespace mx::gui
