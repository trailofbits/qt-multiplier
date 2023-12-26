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

QString UpdateInformationExplorerAction::Verb(void) const noexcept {
  return "com.trailofbits.UpdatePrimaryInformationExplorer";
}

void UpdateInformationExplorerAction::Run(const QVariant &input) noexcept {
  if (!widget || !input.canConvert<VariantEntity>()) {
    return;
  }

  VariantEntity entity = input.value<VariantEntity>();
  widget->DisplayEntity(EntityId(entity).Pack());
}

InformationExplorerPlugin::InformationExplorerPlugin(
    const Context &context_, QMainWindow *parent)
    : IMainWindowPlugin(context_, parent),
      context(context_),
      main_window(parent),
      update_primary(this),
      update_primary_trigger(context.RegisterAction(update_primary)) {}

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
  if (update_primary.widget) {
    return update_primary.widget;
  }

  update_primary.widget = new InformationExplorerWidget(
      context.Index(), context.FileLocationCache(), nullptr, true, parent);
  update_primary.widget->setWindowTitle(tr("Information Explorer"));

  return update_primary.widget;
}

}  // namespace mx::gui
