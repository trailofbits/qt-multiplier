// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/PythonConsoleExplorer.h>

#include <QVBoxLayout>

#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Interfaces/IWindowManager.h>
#include <multiplier/GUI/Interfaces/IWindowWidget.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>
#include <multiplier/GUI/Widgets/PythonConsoleWidget.h>
#include <multiplier/Index.h>

namespace mx::gui {

struct PythonConsoleExplorer::PrivateData {
  ConfigManager &config_manager;
  IWindowManager * const manager;

  IWindowWidget *dock{nullptr};
  PythonConsoleWidget *console{nullptr};

  inline PrivateData(ConfigManager &config_manager_, IWindowManager *manager_)
      : config_manager(config_manager_),
        manager(manager_) {}
};

PythonConsoleExplorer::~PythonConsoleExplorer(void) {}

PythonConsoleExplorer::PythonConsoleExplorer(ConfigManager &config_manager,
                                             IWindowManager *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager, parent)) {

  auto &action_manager = config_manager.ActionManager();
  action_manager.Register(
      this, "com.trailofbits.action.OpenPythonConsole",
      &PythonConsoleExplorer::OnOpenPythonConsole);

  connect(&config_manager, &ConfigManager::IndexChanged,
          this, &PythonConsoleExplorer::OnIndexChanged);

  // Register the dock shell (without creating the Python widget yet).
  CreateDockWidget(parent);
}

void PythonConsoleExplorer::CreateDockWidget(IWindowManager *manager) {
  d->dock = new IWindowWidget;
  d->dock->setWindowTitle(tr("Python Console"));
  d->dock->setContentsMargins(0, 0, 0, 0);

  auto layout = new QVBoxLayout(d->dock);
  layout->setContentsMargins(0, 0, 0, 0);
  d->dock->setLayout(layout);

  IWindowManager::DockConfig config;
  config.id = "com.trailofbits.dock.PythonConsole";
  config.location = IWindowManager::DockLocation::Bottom;
  config.tabify = true;
  config.app_menu_location = {tr("View"), tr("Explorers")};
  manager->AddDockWidget(d->dock, config);

  // Hidden by default; Python is initialized lazily when first shown.
  d->dock->hide();
}

void PythonConsoleExplorer::EnsureConsoleCreated(void) {
  if (d->console) {
    return;
  }

  auto &theme_manager = d->config_manager.ThemeManager();

  d->console = new PythonConsoleWidget(
      theme_manager, d->config_manager.Index(), d->dock);

  connect(&theme_manager, &ThemeManager::ThemeChanged,
          d->console, &PythonConsoleWidget::OnThemeChanged);

  auto *layout = d->dock->layout();
  layout->addWidget(d->console);
}

void PythonConsoleExplorer::ActOnPrimaryClick(
    IWindowManager *, const QModelIndex &index) {
  if (!d->console || !d->dock || !d->dock->isVisible()) {
    return;
  }

  auto entity = IModel::EntitySkipThroughTokens(index);
  if (!std::holds_alternative<NotAnEntity>(entity)) {
    d->console->SetHere(std::move(entity));
  }
}

void PythonConsoleExplorer::OnOpenPythonConsole(const QVariant &) {
  if (d->dock) {
    EnsureConsoleCreated();
    d->dock->show();
    d->dock->EmitRequestAttention();
  }
}

void PythonConsoleExplorer::OnIndexChanged(const ConfigManager &) {
  // Python console is initialized with the index at creation time.
  // If the index changes, we'd need to recreate the console.
}

}  // namespace mx::gui
