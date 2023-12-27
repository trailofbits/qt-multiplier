// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "ReferenceExplorerPlugin.h"

#include <multiplier/ui/IModel.h>
#include <multiplier/ui/MxTabWidget.h>
#include <multiplier/ui/SimpleTextInputDialog.h>

#include <QDialog>
#include <QTabBar>

#include "ReferenceExplorer.h"

namespace mx::gui {

std::unique_ptr<IMainWindowPlugin>
CreateReferenceExplorerMainWindowPlugin(
    const Context &context, QMainWindow *parent) {
  return std::unique_ptr<IMainWindowPlugin>(
      new ReferenceExplorerPlugin(context, parent));
}

ReferenceExplorerPlugin::~ReferenceExplorerPlugin(void) {}

// Act on a primary click. For example, if browse mode is enabled, then this
// is a "normal" click, however, if browse mode is off, then this is a meta-
// click.
void ReferenceExplorerPlugin::ActOnPrimaryClick(const QModelIndex &index) {
  for (const auto &plugin : plugins) {
    plugin->ActOnMainWindowPrimaryClick(main_window, index);
  }
}

std::optional<NamedAction> ReferenceExplorerPlugin::ActOnSecondaryClick(
    const QModelIndex &index) {
  (void) index;
  
  return std::nullopt;
  // auto entity = IModel::EntitySkipThroughTokens(index);
  // if (std::holds_alternative<NotAnEntity>(entity)) {
  //   return;
  // }

  // auto action = new QAction(tr("Show Call Hierarchy"), menu);
  // menu->addAction(action);

  // return NamedAction{
  //   .name = tr("Show callers of ''"),
  //   .action = 
  // };
}

// Allow a main window plugin to act on, e.g. modify, a context menu.
void ReferenceExplorerPlugin::ActOnContextMenu(
    QMenu *menu, const QModelIndex &index) {

  this->IMainWindowPlugin::ActOnContextMenu(menu, index);
  for (const auto &plugin : plugins) {
    plugin->ActOnMainWindowContextMenu(main_window, menu, index);
  }
}

// Allow a main window plugin to act on a long hover over something.
void ReferenceExplorerPlugin::ActOnLongHover(const QModelIndex &index) {
  for (const auto &plugin : plugins) {
    plugin->ActOnMainWindowLongHover(main_window, index);
  }
}

// Allow a main window plugin to provide one of several actions to be
// performed on a key press.
std::vector<NamedAction> ReferenceExplorerPlugin::ActOnKeyPressEx(
    const QKeySequence &keys, const QModelIndex &index) {
  std::vector<NamedAction> actions;

  for (const auto &plugin : plugins) {
    auto plugin_actions = 
        plugin->ActOnMainWindowKeyPressEx(main_window, keys, index);

    actions.insert(actions.end(),
                 std::make_move_iterator(plugin_actions.begin()),
                 std::make_move_iterator(plugin_actions.end()));
  }

  return actions;
}

QWidget *ReferenceExplorerPlugin::CreateDockWidget(QWidget *parent) {
  if (tab_widget) {
    return tab_widget;
  }

  tab_widget = new MxTabWidget(parent);
  tab_widget->setWindowTitle(tr("Reference Explorer"));
  tab_widget->setDocumentMode(true);
  tab_widget->setTabsClosable(true);

  connect(tab_widget->tabBar(), &QTabBar::tabCloseRequested,
        this, &ReferenceExplorerPlugin::OnTabBarClose);

  connect(tab_widget->tabBar(), &QTabBar::tabBarDoubleClicked,
          this, &ReferenceExplorerPlugin::OnTabBarDoubleClick);

  return tab_widget;
}

void ReferenceExplorerPlugin::OnTabBarClose(int i) {
  auto widget = tab_widget->widget(i);
  tab_widget->removeTab(i);

  widget->close();

  if (!tab_widget->count()) {
    emit HideDockWidget();
  }

  // d->reference_explorer_dock->setVisible(widget_visible);
  // d->reference_explorer_dock->toggleViewAction()->setEnabled(widget_visible);
}

void ReferenceExplorerPlugin::OnTabBarDoubleClick(int i) {
  auto current_tab_name = tab_widget->tabText(i);

  SimpleTextInputDialog dialog(tr("Insert the new tab name"), current_tab_name,
                               tab_widget);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  const auto &opt_tab_name = dialog.GetTextInput();

  QString new_tab_name;
  if (opt_tab_name.has_value()) {
    new_tab_name = opt_tab_name.value();
  } else {
    new_tab_name = tr("Reference browser #") + QString::number(i);
  }

  tab_widget->setTabText(i, new_tab_name);
}

}  // namespace mx::gui
