// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/ReferenceExplorer.h>

#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Interfaces/IReferenceExplorerPlugin.h>
#include <multiplier/GUI/Interfaces/ITreeGenerator.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Widgets/SimpleTextInputDialog.h>
#include <multiplier/GUI/Widgets/TabWidget.h>
#include <multiplier/GUI/Widgets/TreeGeneratorWidget.h>

#include <QDialog>
#include <QTabBar>

namespace mx::gui {

struct ReferenceExplorer::PrivateData {
  ConfigManager &config_manager;

  QMainWindow * const main_window;

  // The tabbed reference explorer widget docked inside of the main window.
  TabWidget *view{nullptr};

  // List of plugins.
  std::vector<std::unique_ptr<IReferenceExplorerPlugin>> plugins;

  // Launches a reference explorer given a data generator.
  TriggerHandle open_reference_explorer_trigger;

  // Index for the context menu.
  QModelIndex context_index;

  inline PrivateData(ConfigManager &config_manager_,
                     QMainWindow *main_window_)
      : config_manager(config_manager_),
        main_window(main_window_) {}
};

ReferenceExplorer::~ReferenceExplorer(void) {}

ReferenceExplorer::ReferenceExplorer(ConfigManager &config_manager,
                                     QMainWindow *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager, parent)) {

  d->open_reference_explorer_trigger = config_manager.ActionManager().Register(
      this, "com.trailofbits.action.OpenReferenceExplorer",
      &ReferenceExplorer::OnOpenReferenceExplorer);
}

// Act on a primary click. For example, if browse mode is enabled, then this
// is a "normal" click, however, if browse mode is off, then this is a meta-
// click.
void ReferenceExplorer::ActOnPrimaryClick(const QModelIndex &index) {
  for (const auto &plugin : d->plugins) {
    plugin->ActOnMainWindowPrimaryClick(d->main_window, index);
  }
}

// Allow a main window plugin to act on, e.g. modify, a context menu.
void ReferenceExplorer::ActOnContextMenu(
    QMenu *menu, const QModelIndex &index) {

  if (!d->view->isVisible()) {
    return;
  }

  this->IMainWindowPlugin::ActOnContextMenu(menu, index);

  auto current = d->view->currentWidget();
  if (auto tree = dynamic_cast<TreeGeneratorWidget *>(current)) {
    tree->ActOnContextMenu(menu, index);
  }

  for (const auto &plugin : d->plugins) {
    plugin->ActOnMainWindowContextMenu(d->main_window, menu, index);
  }
}

// Allow a main window plugin to act on a long hover over something.
void ReferenceExplorer::ActOnLongHover(const QModelIndex &index) {
  for (const auto &plugin : d->plugins) {
    plugin->ActOnMainWindowLongHover(d->main_window, index);
  }
}

// Allow a main window plugin to provide one of several actions to be
// performed on a key press.
std::vector<NamedAction> ReferenceExplorer::ActOnKeyPressEx(
    const QKeySequence &keys, const QModelIndex &index) {
  std::vector<NamedAction> actions;

  for (const auto &plugin : d->plugins) {
    auto plugin_actions = 
        plugin->ActOnMainWindowKeyPressEx(d->main_window, keys, index);

    actions.insert(actions.end(),
                 std::make_move_iterator(plugin_actions.begin()),
                 std::make_move_iterator(plugin_actions.end()));
  }

  return actions;
}

QWidget *ReferenceExplorer::CreateDockWidget(QWidget *parent) {
  if (d->view) {
    return d->view;
  }

  d->view = new TabWidget(parent);
  d->view->setWindowTitle(tr("Reference Explorer"));
  d->view->setDocumentMode(true);
  d->view->setTabsClosable(true);

  connect(d->view->tabBar(), &QTabBar::tabCloseRequested,
        this, &ReferenceExplorer::OnTabBarClose);

  connect(d->view->tabBar(), &QTabBar::tabBarDoubleClicked,
          this, &ReferenceExplorer::OnTabBarDoubleClick);

  return d->view;
}

void ReferenceExplorer::OnTabBarClose(int i) {
  if (!d->view) {
    return;
  }

  auto widget = d->view->widget(i);
  d->view->RemoveTab(i);

  widget->close();

  if (!d->view->count()) {
    emit HideDockWidget();
  }
}

void ReferenceExplorer::OnTabBarDoubleClick(int i) {
  if (!d->view) {
    return;
  }

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

  auto tree_view = new TreeGeneratorWidget(d->config_manager, d->view);
  connect(tree_view, &TreeGeneratorWidget::OpenItem,
          this, &IMainWindowPlugin::RequestPrimaryClick);

  connect(tree_view, &TreeGeneratorWidget::RequestContextMenu,
          this, &IMainWindowPlugin::RequestContextMenu);

  connect(tree_view, &TreeGeneratorWidget::SelectedItemChanged,
          this, &ReferenceExplorer::OnSelectionChange);

  tree_view->InstallGenerator(std::move(generator));

  d->view->InsertTab(0, tree_view);
  d->view->setCurrentIndex(0);
  d->context_index = {};

  emit ShowDockWidget();
}

void ReferenceExplorer::AddPlugin(
    std::unique_ptr<IReferenceExplorerPlugin> plugin) {
  if (plugin) {
    d->plugins.emplace_back(std::move(plugin));
  }
}

// Behavior depends on if the code previews are open or not.
void ReferenceExplorer::OnSelectionChange(const QModelIndex &index) {
  d->context_index = {};

  (void) index;
}

}  // namespace mx::gui
