// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QApplication>
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

 private slots:
  void OnThemeListChanged(const ThemeManager &theme_manager);
};

}  // namespace mx::gui