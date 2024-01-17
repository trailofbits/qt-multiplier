// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/ReferenceExplorer.h>

#include <multiplier/Index.h>
#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Interfaces/IReferenceExplorerPlugin.h>
#include <multiplier/GUI/Interfaces/ITreeGenerator.h>
#include <multiplier/GUI/Interfaces/IWindowManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Widgets/SimpleTextInputDialog.h>
#include <multiplier/GUI/Widgets/TabWidget.h>
#include <multiplier/GUI/Widgets/TreeGeneratorWidget.h>

#include <QDialog>
#include <QTabBar>
#include <QVBoxLayout>

namespace mx::gui {

struct ReferenceExplorer::PrivateData {
  ConfigManager &config_manager;

  IWindowManager * const manager;

  // The tabbed reference explorer widget docked inside of the main window.
  TabWidget *view{nullptr};
  IWindowWidget *dock{nullptr};

  // List of plugins.
  std::vector<IReferenceExplorerPluginPtr> plugins;

  // Launches a code preview for the selected entity.
  TriggerHandle open_entity_trigger;

  // Launches a code preview for the selected entity.
  TriggerHandle preview_entity_trigger;

  inline PrivateData(ConfigManager &config_manager_,
                     IWindowManager *manager_)
      : config_manager(config_manager_),
        manager(manager_) {}
};

ReferenceExplorer::~ReferenceExplorer(void) {}

ReferenceExplorer::ReferenceExplorer(ConfigManager &config_manager,
                                     IWindowManager *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager, parent)) {

  auto &action_manager = config_manager.ActionManager();

  action_manager.Register(
      this, "com.trailofbits.action.OpenReferenceExplorer",
      &ReferenceExplorer::OnOpenReferenceExplorer);

  d->open_entity_trigger = action_manager.Find(
      "com.trailofbits.action.OpenEntity");

  d->preview_entity_trigger = action_manager.Find(
      "com.trailofbits.action.OpenEntityPreview");
}

// Act on a primary click. For example, if browse mode is enabled, then this
// is a "normal" click, however, if browse mode is off, then this is a meta-
// click.
void ReferenceExplorer::ActOnPrimaryClick(
    IWindowManager *manager, const QModelIndex &index) {
  for (const auto &plugin : d->plugins) {
    plugin->ActOnPrimaryClick(manager, index);
  }
}

// Allow a main window plugin to act on, e.g. modify, a context menu.
void ReferenceExplorer::ActOnContextMenu(
    IWindowManager *manager, QMenu *menu, const QModelIndex &index) {
  for (const auto &plugin : d->plugins) {
    plugin->ActOnContextMenu(manager, menu, index);
  }

  if (d->view && d->view->isVisible() && index.isValid()) {
    auto current = d->view->currentWidget();
    if (auto tree = dynamic_cast<TreeGeneratorWidget *>(current)) {
      tree->ActOnContextMenu(manager, menu, index);
    }
  }
}

// Allow a main window plugin to act on a long hover over something.
void ReferenceExplorer::ActOnLongHover(
    IWindowManager *manager, const QModelIndex &index) {
  for (const auto &plugin : d->plugins) {
    plugin->ActOnLongHover(manager, index);
  }
}

// Allow a main window plugin to provide one of several actions to be
// performed on a key press.
std::vector<NamedAction> ReferenceExplorer::ActOnKeyPressEx(
    IWindowManager *manager, const QKeySequence &keys,
    const QModelIndex &index) {

  std::vector<NamedAction> actions;

  for (const auto &plugin : d->plugins) {
    auto plugin_actions = 
        plugin->ActOnKeyPressEx(manager, keys, index);

    actions.insert(actions.end(),
                 std::make_move_iterator(plugin_actions.begin()),
                 std::make_move_iterator(plugin_actions.end()));
  }

  return actions;
}

void ReferenceExplorer::CreateDockWidget(void) {
  d->dock = new IWindowWidget;
  d->dock->setWindowTitle(tr("Reference Explorer"));
  d->dock->setContentsMargins(0, 0, 0, 0);

  d->view = new TabWidget(d->dock);
  d->view->setDocumentMode(true);
  d->view->setTabsClosable(true);

  connect(d->view->tabBar(), &QTabBar::tabCloseRequested,
        this, &ReferenceExplorer::OnTabBarClose);

  connect(d->view->tabBar(), &QTabBar::tabBarDoubleClicked,
          this, &ReferenceExplorer::OnTabBarDoubleClick);

  auto dock_layout = new QVBoxLayout(d->dock);
  dock_layout->setContentsMargins(0, 0, 0, 0);
  dock_layout->addWidget(d->view, 1);
  dock_layout->addStretch();
  d->dock->setLayout(dock_layout);

  IWindowManager::DockConfig config;
  config.id = "com.trailofbits.dock.ReferenceExplorer";
  config.location = IWindowManager::DockLocation::Bottom;
  config.app_menu_location = {tr("View"), tr("Explorers")};
  d->manager->AddDockWidget(d->dock, config);
}

void ReferenceExplorer::OnTabBarClose(int i) {
  auto widget = d->view->widget(i);
  d->view->RemoveTab(i);
  widget->close();

  if (!d->view->count()) {
    d->dock->hide();
  }
}

void ReferenceExplorer::OnTabBarDoubleClick(int i) {
  auto current_tab_name = d->view->tabText(i);

  SimpleTextInputDialog dialog(tr("Insert the new tab name"), current_tab_name,
                               d->view);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  const auto &opt_tab_name = dialog.TextInput();

  QString new_tab_name;
  if (opt_tab_name.has_value()) {
    new_tab_name = opt_tab_name.value();
  } else {
    new_tab_name = tr("Reference Browser #") + QString::number(i);
  }

  d->view->setTabText(i, new_tab_name);
}

void ReferenceExplorer::OnOpenReferenceExplorer(const QVariant &data) {
  if (data.isNull() || !data.canConvert<ITreeGeneratorPtr>()) {
    return;
  }

  auto generator = data.value<ITreeGeneratorPtr>();
  if (!generator) {
    return;
  }

  if (!d->view) {
    CreateDockWidget();
  }

  d->view->show();

  auto tree_view = new TreeGeneratorWidget(d->config_manager, d->view);

  connect(tree_view, &TreeGeneratorWidget::OpenItem,
          this, &ReferenceExplorer::OnOpenItem);

  connect(tree_view, &TreeGeneratorWidget::RequestSecondaryClick,
          this, &IMainWindowPlugin::RequestSecondaryClick);

  connect(tree_view, &TreeGeneratorWidget::RequestPrimaryClick,
          this, &ReferenceExplorer::OnSelectionChange);

  connect(tree_view, &TreeGeneratorWidget::RequestPrimaryClick,
          this, &IMainWindowPlugin::RequestPrimaryClick);

  tree_view->InstallGenerator(std::move(generator));

  d->view->InsertTab(0, tree_view);
  d->dock->show();
}

void ReferenceExplorer::AddPlugin(IReferenceExplorerPluginPtr plugin) {
  if (plugin) {
    d->plugins.emplace_back(std::move(plugin));
  }
}

void ReferenceExplorer::OnOpenItem(const QModelIndex &index) {
  auto entity = IModel::EntitySkipThroughTokens(index);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  d->open_entity_trigger.Trigger(QVariant::fromValue(entity));
}

void ReferenceExplorer::OnSelectionChange(const QModelIndex &index) {
  auto entity = IModel::EntitySkipThroughTokens(index);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  d->preview_entity_trigger.Trigger(QVariant::fromValue(entity));
}

}  // namespace mx::gui
