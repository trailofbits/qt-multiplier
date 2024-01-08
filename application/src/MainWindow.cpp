// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MainWindow.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>

#include <multiplier/GUI/Explorers/EntityExplorer.h>
#include <multiplier/GUI/Explorers/HighlightExplorer.h>
#include <multiplier/GUI/Explorers/InformationExplorer.h>
#include <multiplier/GUI/Explorers/ProjectExplorer.h>
#include <multiplier/GUI/Explorers/ReferenceExplorer.h>
#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>
#include <multiplier/GUI/Plugins/BuiltinEntityInformationPlugin.h>
#include <multiplier/GUI/Plugins/CallHierarchyPlugin.h>
#include <multiplier/GUI/Themes/BuiltinTheme.h>
#include <multiplier/GUI/Widgets/CodeWidget.h>
#include <multiplier/Index.h>
#include <vector>

#include "WindowManager.h"

namespace mx::gui {

struct MainWindow::PrivateData {
  ConfigManager config_manager;

  // Plugins to the main window.
  std::vector<std::unique_ptr<IMainWindowPlugin>> plugins;

  QMenu *view_menu{nullptr};
  QMenu *view_explorers_menu{nullptr};
  QMenu *view_theme_menu{nullptr};

  WindowManager * const window_manager;

  inline PrivateData(QApplication &application, MainWindow *main_window)
      : config_manager(application, main_window),
        window_manager(new WindowManager(main_window)) {}
};

MainWindow::~MainWindow(void) {}

MainWindow::MainWindow(QApplication &application, QWidget *parent)
    : QMainWindow(parent),
      d(new PrivateData(application, this)) {

  InitializeMenus();
  InitializeThemes();
  InitializeIndex(application);
  InitializeDocks();
  InitializePlugins();
  auto file = d->config_manager.Index().file(1152921504606847251ull);
  auto code = new CodeWidget(this);
  code->SetTokenTree(TokenTree::from(file.value()));
  setCentralWidget(code);
}

void MainWindow::InitializePlugins(void) {
  auto wm = d->window_manager;

  d->plugins.emplace_back(new ProjectExplorer(d->config_manager, wm));
  d->plugins.emplace_back(new EntityExplorer(d->config_manager, wm));
  
  auto info_explorer = new InformationExplorer(d->config_manager, wm);
  info_explorer->EmplacePlugin<BuiltinEntityInformationPlugin>();
  d->plugins.emplace_back(info_explorer);

  auto ref_explorer = new ReferenceExplorer(d->config_manager, wm);
  ref_explorer->EmplacePlugin<CallHierarchyPlugin>(
      d->config_manager, ref_explorer);
  d->plugins.emplace_back(ref_explorer);

  d->plugins.emplace_back(new HighlightExplorer(d->config_manager, wm));

  for (const auto &plugin : d->plugins) {
    connect(plugin.get(), &IMainWindowPlugin::RequestSecondaryClick,
            this, &MainWindow::OnRequestSecondaryClick);

    connect(plugin.get(), &IMainWindowPlugin::RequestPrimaryClick,
            this, &MainWindow::OnRequestPrimaryClick);
  }
}

void MainWindow::InitializeMenus(void) {
  d->view_menu = d->window_manager->Menu(tr("View"));
  d->view_theme_menu = new QMenu(tr("Themes"));
  d->view_menu->addMenu(d->view_theme_menu);

  menuBar()->addMenu(d->view_menu);
}

void MainWindow::InitializeThemes(void) {
  auto &theme_manager = d->config_manager.ThemeManager();
  auto &media_manager = d->config_manager.MediaManager();

  theme_manager.Register(CreateDarkTheme(media_manager));
  theme_manager.Register(CreateLightTheme(media_manager));

  // Populate the theme list menu, and keep it up-to-date.
  OnThemeListChanged(theme_manager);
  connect(&theme_manager, &ThemeManager::ThemeListChanged,
          this, &MainWindow::OnThemeListChanged);
}

//! Keep the theme selection menu up-to-date with the set of registered themes.
void MainWindow::OnThemeListChanged(const ThemeManager &theme_manager) {
  d->view_theme_menu->clear();

  auto current_theme = theme_manager.Theme();

  for (auto theme : theme_manager.ThemeList()) {
    auto action = new QAction(theme->Name());
    connect(action, &QAction::triggered,
            [theme = std::move(theme), this] (void) {
              d->config_manager.ThemeManager().SetTheme(std::move(theme));
            });
    d->view_theme_menu->addAction(action);
  }
}

void MainWindow::InitializeDocks(void) {
  
}

void MainWindow::InitializeIndex(QApplication &application) {
  QCommandLineOption theme_option("theme");
  theme_option.setValueName("theme");

  QCommandLineOption db_option("database");
  db_option.setValueName("database");

  QCommandLineParser parser;
  parser.addOption(theme_option);
  parser.addOption(db_option);

  parser.process(application);

  // Set the database.
  QString db_path;
  if (!parser.isSet(db_option)) {
    db_path = QFileDialog::getOpenFileName(
        nullptr, QObject::tr("Select a Multiplier database"), QDir::homePath());
  } else {
    db_path = parser.value(db_option);
  }

  d->config_manager.SetIndex(Index::in_memory_cache(
      Index::from_database(db_path.toStdString())));

  // Set the theme.
  if (parser.isSet(theme_option)) {
    auto &theme_manager = d->config_manager.ThemeManager();
    if (auto theme = theme_manager.Find(parser.value(theme_option))) {
      theme_manager.SetTheme(std::move(theme));
    }
  }
}

//! Invoked on an index whose underlying model follows the `IModel` interface.
void MainWindow::OnRequestSecondaryClick(const QModelIndex &index) {
  QMenu menu(tr("Context Menu"));
  for (const auto &plugin : d->plugins) {
    plugin->ActOnContextMenu(d->window_manager, &menu, index);
  }
  menu.exec(QCursor::pos());
}

//! Invoked on an index whose underlying model follows the `IModel` interface.
void MainWindow::OnRequestPrimaryClick(const QModelIndex &index) {
  for (const auto &plugin : d->plugins) {
    plugin->ActOnPrimaryClick(d->window_manager, index);
  }
}

}  // namespace mx::gui
