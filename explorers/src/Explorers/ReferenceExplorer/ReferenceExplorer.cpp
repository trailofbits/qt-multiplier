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

  this->IMainWindowPlugin::ActOnContextMenu(menu, index);
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
  d->view->removeTab(i);

  widget->close();

  if (!d->view->count()) {
    emit HideDockWidget();
  }

  // d->reference_explorer_dock->setVisible(widget_visible);
  // d->reference_explorer_dock->toggleViewAction()->setEnabled(widget_visible);
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
    new_tab_name = tr("Reference browser #") + QString::number(i);
  }

  d->view->setTabText(i, new_tab_name);
}

void ReferenceExplorer::OnOpenReferenceExplorer(const QVariant &data) {
  if (data.isNull() || !data.canConvert<std::shared_ptr<ITreeGenerator>>()) {
    return;
  }

  auto generator = data.value<std::shared_ptr<ITreeGenerator>>();
  if (!generator) {
    return;
  }

  // auto popup = new PopupWidgetContainer<ReferenceExplorer>(
  //     context.Index(), context.FileLocationCache(),
  //     std::move(generator),
  //     false  /* TODO: enable code preview */,
  //     nullptr  /* TODO: global highlighter */,
  //     nullptr  /* TODO: macro explorer */,
  //     main_window);

  // popup->show();
  // emit PopupOpened(popup);

  // TODO(pag): Trigger browser mode.
  // popup->GetWrappedWidget()->SetBrowserMode(
  //     d->toolbar.browser_mode->isChecked());
}

void ReferenceExplorer::AddPlugin(
    std::unique_ptr<IReferenceExplorerPlugin> plugin) {
  if (plugin) {
    d->plugins.emplace_back(std::move(plugin));
  }
}

}  // namespace mx::gui
