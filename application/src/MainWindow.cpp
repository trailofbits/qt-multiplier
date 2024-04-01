// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MainWindow.h"
#include "WindowManager.h"

#include <multiplier/Frontend/TokenTree.h>
#include <multiplier/GUI/Explorers/CodeExplorer.h>
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
#include <multiplier/GUI/Plugins/StructExplorerPlugin.h>
#include <multiplier/GUI/Themes/BuiltinTheme.h>
#include <multiplier/Index.h>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>

#include <vector>

namespace mx::gui {

namespace {

const char *kMultiplierLicense =
  "Copyright 2018-2024, Trail of Bits, Inc., all rights reserved.\n\n"
  "This software is proprietary and confidential.";

const char *kThirdPartyLibsLicense =
  "Qt 6 (LGPL)\n"
  "Phantom Style (LGPL),\n"
  "Qt-Advanced-Docking-System (LGPL)\n"
  "doctest (MIT)\n"
  "xxHash (BSD 2-Clause)";

}

struct MainWindow::PrivateData {
  ConfigManager config_manager;

  // Plugins to the main window.
  std::vector<std::unique_ptr<IMainWindowPlugin>> plugins;

  QMenu *view_menu{nullptr};
  QMenu *view_explorers_menu{nullptr};
  QMenu *view_theme_menu{nullptr};

  WindowManager *const window_manager;

  inline PrivateData(QApplication &application, MainWindow *main_window)
      : config_manager(application, main_window),
        window_manager(new WindowManager(main_window)) {}
};

MainWindow::~MainWindow(void) {}

MainWindow::MainWindow(QApplication &application, QWidget *parent)
    : QMainWindow(parent),
      d(new PrivateData(application, this)) {

  setWindowTitle("Multiplier");

  InitializeMenus();
  InitializeThemes();
  InitializeIndex(application);
  InitializeDocks();
  InitializePlugins();

  setWindowIcon(
      d->config_manager.MediaManager().Icon("com.trailofbits.icon.Logo"));
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
  ref_explorer->EmplacePlugin<StructExplorerPlugin>(
      d->config_manager, ref_explorer);
  d->plugins.emplace_back(ref_explorer);

  d->plugins.emplace_back(new HighlightExplorer(d->config_manager, wm));
  d->plugins.emplace_back(new CodeExplorer(d->config_manager, wm));

  for (const auto &plugin : d->plugins) {
    connect(plugin.get(), &IMainWindowPlugin::RequestPrimaryClick,
            this, &MainWindow::OnRequestPrimaryClick);

    connect(plugin.get(), &IMainWindowPlugin::RequestSecondaryClick,
            this, &MainWindow::OnRequestSecondaryClick);

    connect(plugin.get(), &IMainWindowPlugin::RequestKeyPress,
            this, &MainWindow::OnRequestKeyPress);
  }
}

void MainWindow::InitializeMenus(void) {
  d->view_menu = d->window_manager->Menu(tr("View"));
  d->view_theme_menu = new QMenu(tr("Themes"), this);
  d->view_menu->addMenu(d->view_theme_menu);

  menuBar()->addMenu(d->view_menu);

  auto help_menu = new QMenu(tr("Help"));
  menuBar()->addMenu(help_menu);

  auto about_action = new QAction(tr("About"));
  help_menu->addAction(about_action);
  connect(about_action, &QAction::triggered, this, [this]() {
    QMessageBox::information(this, "Multiplier", kMultiplierLicense);
  });

  auto third_party_libs = new QAction(tr("Third-party libraries"));
  help_menu->addAction(third_party_libs);
  connect(third_party_libs, &QAction::triggered, this, [this]() {
    QMessageBox::information(this, tr("Third-party libraries"), kThirdPartyLibsLicense);
  });
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
void MainWindow::OnThemeListChanged(const ThemeManager &) {
  d->view_theme_menu->clear();

  auto theme_manager_ptr = &(d->config_manager.ThemeManager());
  for (auto theme : theme_manager_ptr->ThemeList()) {
    auto action = new QAction(theme->Name(), d->view_theme_menu);
    connect(action, &QAction::triggered,
            theme_manager_ptr, [=, theme = std::move(theme)] (void) {
              theme_manager_ptr->SetTheme(std::move(theme));
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
  parser.addHelpOption();
  parser.addVersionOption();
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
  QString theme_name;
  if (parser.isSet(theme_option)) {
    theme_name = parser.value(theme_option);
  } else {
    theme_name = "com.trailofbits.theme.Dark";
  }

  auto &theme_manager = d->config_manager.ThemeManager();
  if (auto theme = theme_manager.Find(parser.value(theme_option))) {
    theme_manager.SetTheme(std::move(theme));
  }
}

//! Invoked on an index whose underlying model follows the `IModel` interface.
void MainWindow::OnRequestSecondaryClick(const QModelIndex &index) {
  auto position = QCursor::pos();
  QMenu menu(tr("Context Menu"));
  menu.move(position);

  for (const auto &plugin : d->plugins) {
    plugin->ActOnContextMenu(d->window_manager, &menu, index);
  }
  menu.exec(position);
}

//! Invoked on an index whose underlying model follows the `IModel` interface.
void MainWindow::OnRequestPrimaryClick(const QModelIndex &index) {
  for (const auto &plugin : d->plugins) {
    plugin->ActOnPrimaryClick(d->window_manager, index);
  }
}

//! Invoked on an index whose underlying model follows the `IModel` interface.
void MainWindow::OnRequestKeyPress(
    const QKeySequence &keys, const QModelIndex &index) {
  
  std::vector<NamedAction> actions;

  for (const auto &plugin : d->plugins) {
    auto plugin_actions = plugin->ActOnKeyPressEx(
        d->window_manager, keys, index);
    actions.insert(actions.end(),
                   std::make_move_iterator(plugin_actions.begin()),
                   std::make_move_iterator(plugin_actions.end()));
  }

  if (actions.empty()) {
    return;
  }

  if (actions.size() == 1u) {
    actions[0].action.Trigger(actions[0].data);
    return;
  }

  auto position = QCursor::pos();

  QMenu key_menu(tr("Key Press Menu"));
  key_menu.move(position);

  for (auto &plugin_action : actions) {
    auto action = new QAction(plugin_action.name, &key_menu);
    connect(
        action, &QAction::triggered,
        [trigger = std::move(plugin_action.action),
         data = std::move(plugin_action.data)] (void) {
          trigger.Trigger(data);
        });
    key_menu.addAction(action);
  }
  key_menu.exec(position);
}

}  // namespace mx::gui
