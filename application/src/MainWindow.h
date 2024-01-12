// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QApplication>
#include <QContextMenuEvent>
#include <QMainWindow>

namespace mx::gui {

class ThemeManager;

class MainWindow Q_DECL_FINAL : public QMainWindow {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  MainWindow(void) = delete;
 
 public:
  virtual ~MainWindow(void);

  explicit MainWindow(QApplication &application, QWidget *parent=nullptr);

  void InitializeThemes(void);
  void InitializePlugins(void);
  void InitializeMenus(void);
  void InitializeDocks(void);
  void InitializeIndex(QApplication &application);

 public slots:
  void OnThemeListChanged(const ThemeManager &theme_manager);

  //! Invoked on an index whose underlying model follows the `IModel` interface.
  void OnRequestSecondaryClick(const QModelIndex &index);

  //! Invoked on an index whose underlying model follows the `IModel` interface.
  void OnRequestPrimaryClick(const QModelIndex &index);

  //! Invoked on an index whose underlying model follows the `IModel` interface.
  void OnRequestKeyPress(const QKeySequence &keys, const QModelIndex &index);
};

}  // namespace mx::gui