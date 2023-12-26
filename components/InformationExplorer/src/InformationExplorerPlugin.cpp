// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "InformationExplorerPlugin.h"

#include <multiplier/ui/Context.h>
#include <multiplier/ui/IModel.h>
#include <multiplier/ui/MxTabWidget.h>
#include <multiplier/ui/SimpleTextInputDialog.h>
#include <multiplier/ui/Util.h>

#include <QDialog>
#include <QTabBar>

#include "InformationExplorerWidget.h"

namespace mx::gui {

std::unique_ptr<IMainWindowPlugin>
CreateInformationExplorerMainWindowPlugin(
    const Context &context, QMainWindow *parent) {
  return std::unique_ptr<IMainWindowPlugin>(
      new InformationExplorerPlugin(context, parent));
}

InformationExplorerPlugin::InformationExplorerPlugin(
    const Context &context_, QMainWindow *parent)
    : IMainWindowPlugin(context_, parent),
      context(context_),
      main_window(parent),
      update_primary_trigger(context.ActionRegistry().Register(
          this,
          "com.trailofbits.UpdatePrimaryInformationExplorer",
          [this] (const QVariant &input) {
            if (!primary_widget || !input.canConvert<VariantEntity>()) {
              return;
            }

            VariantEntity entity = input.value<VariantEntity>();
            primary_widget->DisplayEntity(EntityId(entity).Pack());
          })),
      open_entity_trigger(
          context.FindAction("com.trailofbits.OpenEntity")) {}

InformationExplorerPlugin::~InformationExplorerPlugin(void) {}

void InformationExplorerPlugin::ActOnSecondaryClick(
    QMenu *menu, const QModelIndex &index) {

  VariantEntity entity = IModel::EntitySkipThroughTokens(index);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  QString action_name = tr("Information about this entity");

  if (auto name = NameOfEntityAsString(entity)) {
    action_name = tr("Information about '%1'").arg(name.value());
  }

  auto action = new QAction(action_name, menu);
  menu->addAction(action);

  connect(
      action, &QAction::triggered,
      [upt = update_primary_trigger, entity = std::move(entity)] (void) {
        upt.Trigger(QVariant::fromValue(entity));
      });
}

QWidget *InformationExplorerPlugin::CreateDockWidget(QWidget *parent) {
  if (primary_widget) {
    return primary_widget;
  }

  primary_widget = new InformationExplorerWidget(
      context.Index(), context.FileLocationCache(), nullptr, true, parent);
  primary_widget->setWindowTitle(tr("Information Explorer"));

  connect(
      primary_widget, &InformationExplorerWidget::SelectedItemChanged,
      [oet = open_entity_trigger] (const QModelIndex &index) {
        oet.Trigger(index.data(IModel::EntityRole));
      });

  return primary_widget;
}

}  // namespace mx::gui
