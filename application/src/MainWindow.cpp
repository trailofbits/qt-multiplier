// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MainWindow.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDockWidget>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>
#include <multiplier/GUI/Themes/BuiltinTheme.h>
#include <multiplier/Index.h>
#include <vector>

#include "Explorers/EntityExplorer/EntityExplorer.h"
#include "Explorers/ProjectExplorer/ProjectExplorer.h"

namespace mx::gui {

struct MainWindow::PrivateData {
  ConfigManager config_manager;

  // Plugins to the main window.
  std::vector<std::unique_ptr<IMainWindowPlugin>> plugins;

  QMenu *view_menu{nullptr};
  QMenu *view_explorers_menu{nullptr};
  QMenu *view_theme_menu{nullptr};

  inline PrivateData(QApplication &application, QMainWindow *main_window)
      : config_manager(application, main_window) {}
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
}

void MainWindow::InitializePlugins(void) {
  d->plugins.emplace_back(new ProjectExplorer(d->config_manager, this));
  d->plugins.emplace_back(new EntityExplorer(d->config_manager, this));

  for (const auto &plugin : d->plugins) {
    QWidget *dockable_widget = plugin->CreateDockWidget(this);
    if (!dockable_widget) {
      continue;
    }

    auto dock = new QDockWidget(dockable_widget->windowTitle(), this);
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock->setWidget(dockable_widget);

    d->view_menu->addAction(dock->toggleViewAction());
    addDockWidget(Qt::LeftDockWidgetArea, dock);

    connect(
        plugin.get(), &IMainWindowPlugin::ShowDockWidget,
        [=] (void) {
          dock->show();
        });

    connect(
        plugin.get(), &IMainWindowPlugin::HideDockWidget,
        [=] (void) {
          dock->hide();
        });

    connect(plugin.get(), &IMainWindowPlugin::RequestContextMenu,
            this, &MainWindow::OnRequestContextMenu);

    // connect(
    //     plugin.get(), &IMainWindowPlugin::PopupOpened,
    //     [this] (QWidget *popup) {
    //       popup->installEventFilter(this);
    //       d->popup_widget_list.push_back(popup);
    //     });
  }
}

void MainWindow::InitializeMenus(void) {
  d->view_menu = new QMenu(tr("View"));
  d->view_explorers_menu = new QMenu(tr("Explorers"));
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
  setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::West);
  setTabPosition(Qt::RightDockWidgetArea, QTabWidget::East);
  setTabPosition(Qt::TopDockWidgetArea, QTabWidget::North);
  setTabPosition(Qt::BottomDockWidgetArea, QTabWidget::North);
  setDocumentMode(false);
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
void MainWindow::OnRequestContextMenu(const QModelIndex &index) {
  QMenu menu(tr("Context Menu"));
  for (const auto &plugin : d->plugins) {
    plugin->ActOnContextMenu(&menu, index);
  }
  menu.exec(QCursor::pos());
}

}  // namespace mx::gui
