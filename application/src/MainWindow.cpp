// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MainWindow.h"
#include "WindowManager.h"

#include <multiplier/Frontend/TokenTree.h>
#include <multiplier/GUI/Explorers/CodeExplorer.h>
#include <multiplier/GUI/Explorers/CodeSearchExplorer.h>
#include <multiplier/GUI/Explorers/DocumentExplorer.h>
#include <multiplier/GUI/Explorers/EntityExplorer.h>
#ifdef MX_ENABLE_PYTHON
# include <multiplier/GUI/Explorers/PythonConsoleExplorer.h>
#endif
#include <multiplier/GUI/Explorers/HighlightExplorer.h>
#include <multiplier/GUI/Explorers/InformationExplorer.h>
#include <multiplier/GUI/Explorers/ProjectExplorer.h>
#include <multiplier/GUI/Explorers/ReferenceExplorer.h>
#include <multiplier/GUI/Explorers/SpreadsheetExplorer.h>

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>
#include <multiplier/GUI/Plugins/BuiltinEntityInformationPlugin.h>
#include <multiplier/GUI/Plugins/CallHierarchyPlugin.h>
#include <multiplier/GUI/Plugins/ClassHierarchyPlugin.h>
#include <multiplier/GUI/Plugins/StructExplorerPlugin.h>
#include <multiplier/GUI/Themes/BuiltinTheme.h>
#include <multiplier/Index.h>

#include <QCloseEvent>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QTimer>
#include <QToolButton>
#include <QUndoGroup>

#include <vector>

namespace mx::gui {

namespace {

const char *kMultiplierLicense =
  "Copyright 2018-2024, Trail of Bits, Inc., all rights reserved.\n\n"
  "This software is proprietary and confidential.";

const char *kThirdPartyLibsLicense =
  "Qt 6 (LGPL)\n"
  "Phantom Style 309c97a (LGPL)\n"
  "Qt-Advanced-Docking-System 4.2.1 (LGPL)\n"
  "doctest 2.4.11 (MIT)\n"
  "xxHash 0.8.2 (BSD 2-Clause)";

}

struct MainWindow::PrivateData {
  ConfigManager config_manager;

  // Plugins to the main window.
  std::vector<std::unique_ptr<IMainWindowPlugin>> plugins;

  QMenu *view_menu{nullptr};
  QMenu *view_explorers_menu{nullptr};

  WindowManager *const window_manager;

  inline PrivateData(QApplication &application, MainWindow *main_window)
      : config_manager(application, main_window),
        window_manager(new WindowManager(main_window)) {}
};

MainWindow::~MainWindow(void) {
  d->config_manager.SaveSettings();
}

void MainWindow::closeEvent(QCloseEvent *event) {
  // Save window layout while the window is still fully alive.
  d->config_manager.SaveWindowLayout(saveState(), saveGeometry());

  // Destroy plugins now (while ConfigManager is still alive) so their
  // destructors can safely save state to the database.
  d->plugins.clear();

  QMainWindow::closeEvent(event);
}

MainWindow::MainWindow(QApplication &application, QWidget *parent)
    : QMainWindow(parent),
      d(new PrivateData(application, this)) {

  setWindowTitle("Multiplier");

  InitializeMenus();
  InitializeThemes();
  // Load persistent settings after themes are registered so the saved
  // theme can be restored.
  d->config_manager.LoadSettings();
  // InitializeIndex is called after the event loop starts (from main)
  // so that file dialogs work properly on macOS.
  InitializeDocks();

  setWindowIcon(
      d->config_manager.MediaManager().Icon("com.trailofbits.icon.Logo"));

  // Window layout is restored in InitializeIndex after all plugins
  // and docks are created.
}

void MainWindow::InitializePlugins(void) {
  auto wm = d->window_manager;

  // Global undo/redo toolbar buttons — added first so they appear
  // at the left of the toolbar.
  {
    auto &undo_group = d->config_manager.UndoGroup();
    auto &media_manager = d->config_manager.MediaManager();

    auto *undo_action = undo_group.createUndoAction(this, tr("Undo"));
    undo_action->setShortcut(QKeySequence::Undo);
    undo_action->setIcon(media_manager.Icon("com.trailofbits.icon.Undo"));

    auto *redo_action = undo_group.createRedoAction(this, tr("Redo"));
    redo_action->setShortcut(QKeySequence::Redo);
    redo_action->setIcon(media_manager.Icon("com.trailofbits.icon.Redo"));

    auto *undo_btn = new QToolButton(this);
    undo_btn->setDefaultAction(undo_action);
    undo_btn->setIconSize(QSize(16, 16));
    wm->AddToolBarWidget(undo_btn);

    auto *redo_btn = new QToolButton(this);
    redo_btn->setDefaultAction(redo_action);
    redo_btn->setIconSize(QSize(16, 16));
    wm->AddToolBarWidget(redo_btn);


    // Also add to the Edit menu.
    auto *edit_menu = wm->Menu(tr("Edit"));
    edit_menu->addAction(undo_action);
    edit_menu->addAction(redo_action);
  }

  d->plugins.emplace_back(new ProjectExplorer(d->config_manager, wm));
  d->plugins.emplace_back(new EntityExplorer(d->config_manager, wm));
  
  auto info_explorer = new InformationExplorer(d->config_manager, wm);
  info_explorer->EmplacePlugin<BuiltinEntityInformationPlugin>();
  d->plugins.emplace_back(info_explorer);

  auto ref_explorer = new ReferenceExplorer(d->config_manager, wm);
  ref_explorer->EmplacePlugin<CallHierarchyPlugin>(
      d->config_manager, ref_explorer);
  ref_explorer->EmplacePlugin<ClassHierarchyPlugin>(
      d->config_manager, ref_explorer);
  ref_explorer->EmplacePlugin<StructExplorerPlugin>(
      d->config_manager, ref_explorer);
  d->plugins.emplace_back(ref_explorer);

  d->plugins.emplace_back(new HighlightExplorer(d->config_manager, wm));
  d->plugins.emplace_back(new CodeExplorer(d->config_manager, wm));
  d->plugins.emplace_back(new SpreadsheetExplorer(d->config_manager, wm));
  d->plugins.emplace_back(new DocumentExplorer(d->config_manager, wm));
  d->plugins.emplace_back(new CodeSearchExplorer(d->config_manager, wm));

#ifdef MX_ENABLE_PYTHON
  d->plugins.emplace_back(new PythonConsoleExplorer(d->config_manager, wm));
#endif

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
  // Create File menu first so it appears before View and Help.
  d->window_manager->Menu(tr("File"));

  d->view_menu = d->window_manager->Menu(tr("View"));

  // Let each manager add its items to the View menu.
  d->config_manager.PopulateViewMenu(d->view_menu);

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

  // When the theme changes, force the docking system to re-resolve its
  // palette-based stylesheet so tab bars, scrollbars, etc. update.
  connect(&theme_manager, &ThemeManager::ThemeChanged,
          this, [this](const ThemeManager &tm) {
            d->window_manager->RefreshDockStylesheet(
                tm.Theme()->DefaultBackgroundColor());
          });
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
    QFileDialog dialog(this, tr("Select a Multiplier database"),
                       QDir::homePath());
    dialog.setNameFilter(tr("Multiplier databases (*.db);;All files (*)"));
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setWindowModality(Qt::ApplicationModal);

    if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty()) {
      db_path = dialog.selectedFiles().first();
    }

    if (db_path.isEmpty()) {
      QMessageBox::warning(this, tr("No database selected"),
                           tr("No database was selected. Exiting."));
      QTimer::singleShot(0, &application, &QApplication::quit);
      return;
    }
  } else {
    db_path = parser.value(db_option);
  }

  d->config_manager.SetIndex(Index::in_memory_cache(
      Index::from_database(db_path.toStdString())), db_path);

  InitializePlugins();

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

  // Restore window layout now that all plugins and docks are created.
  QByteArray state, geometry;
  if (d->config_manager.LoadWindowLayout(state, geometry) &&
      !state.isEmpty()) {
    restoreGeometry(geometry);
    bool ok = restoreState(state);
    // If restore failed (e.g. stale state), fall through to defaults.
    if (!ok) {
      for (auto *child : findChildren<QDockWidget *>()) {
        child->hide();
      }
      for (auto *child : findChildren<QDockWidget *>()) {
        auto name = child->objectName();
        if (name.contains(QStringLiteral("ProjectExplorer")) ||
            name.contains(QStringLiteral("EntityExplorer")) ||
            name.contains(QStringLiteral("InformationExplorer"))) {
          child->show();
        }
      }
    }
  } else {
    // First launch: hide all docks, then show only the defaults.
    for (auto *child : findChildren<QDockWidget *>()) {
      child->hide();
    }
    for (auto *child : findChildren<QDockWidget *>()) {
      auto name = child->objectName();
      if (name.contains(QStringLiteral("ProjectExplorer")) ||
          name.contains(QStringLiteral("EntityExplorer")) ||
          name.contains(QStringLiteral("InformationExplorer"))) {
        child->show();
      }
    }
    // Maximize on first launch.
    showMaximized();
  }

  // Load persisted sheets AFTER restoreState so the dock visibility
  // from the saved layout is preserved.
  for (const auto &plugin : d->plugins) {
    if (auto *sheet = dynamic_cast<SpreadsheetExplorer *>(plugin.get())) {
      sheet->LoadPersistedSheets();
      break;
    }
  }

  // Ensure visible docks have a reasonable minimum height.
  // restoreState can leave dock areas with zero-height splitters.
  for (auto *dock : findChildren<QDockWidget *>()) {
    if (dock->isVisible() && dock->height() < 50) {
      dock->setMinimumHeight(100);
      // Reset after layout settles.
      QTimer::singleShot(0, dock, [dock] () {
        dock->setMinimumHeight(0);
      });
    }
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
