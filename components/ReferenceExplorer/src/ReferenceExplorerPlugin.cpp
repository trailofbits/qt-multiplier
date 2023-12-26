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

std::optional<IMainWindowPlugin::NamedAction>
ReferenceExplorerPlugin::ActOnSecondaryClick(const QModelIndex &index) {
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
