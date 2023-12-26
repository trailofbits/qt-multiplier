// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "InformationExplorerPlugin.h"

#include <multiplier/ui/Context.h>
#include <multiplier/ui/DockWidgetContainer.h>
#include <multiplier/ui/IModel.h>
#include <multiplier/ui/MxTabWidget.h>
#include <multiplier/ui/SimpleTextInputDialog.h>
#include <multiplier/ui/Util.h>

#include <QDialog>

#include "InformationExplorerWidget.h"

namespace mx::gui {
namespace {

static const QKeySequence kKeySeqI("I");
static const QKeySequence kKeySeqShiftI("Shift+I");

static QString ActionName(const VariantEntity &entity) {
  if (auto name = NameOfEntityAsString(entity)) {
    return QObject::tr("Information about '%1'").arg(name.value());
  }
  return QObject::tr("Information about this entity");
}

}  // namespace

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
          &InformationExplorerPlugin::UpdatePrimary)),
      open_secondary_trigger(context.ActionRegistry().Register(
          this,
          "com.trailofbits.OpenSecondaryInformationExplorer",
          &InformationExplorerPlugin::OpenSecondary)),
      open_entity_trigger(
          context.FindAction("com.trailofbits.OpenEntity")) {}

InformationExplorerPlugin::~InformationExplorerPlugin(void) {}

// Update the primary information explorer.
void InformationExplorerPlugin::UpdatePrimary(const QVariant &input) {
  if (!primary_widget || !input.canConvert<VariantEntity>()) {
    return;
  }

  VariantEntity entity = input.value<VariantEntity>();
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  primary_widget->DisplayEntity(EntityId(entity).Pack());
}

// Open a new secondary information explorer.
void InformationExplorerPlugin::OpenSecondary(const QVariant &input) {
  if (!input.canConvert<VariantEntity>()) {
    return;
  }

  VariantEntity entity = input.value<VariantEntity>();
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  auto dock = new DockWidgetContainer<InformationExplorerWidget>(
      context.Index(), context.FileLocationCache(), nullptr /* TODO */,
      false, main_window);

  auto secondary_widget = dock->GetWrappedWidget();
  InitializeSignals(secondary_widget);

  main_window->addDockWidget(Qt::LeftDockWidgetArea, dock);
  secondary_widget->DisplayEntity(EntityId(entity).Pack());
}

// A right-click menu option is added that lets us open up an entity in the
// primary information explorer.
std::optional<IMainWindowPlugin::NamedAction>
InformationExplorerPlugin::ActOnSecondaryClick(const QModelIndex &index) {

  VariantEntity entity = IModel::EntitySkipThroughTokens(index);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return std::nullopt;
  }

  return NamedAction{
    .name = ActionName(entity),
    .action = update_primary_trigger,
    .data = QVariant::fromValue(entity)
  };
}

// If `I` is pressed, then we open up the entity in the primary information
// explorer. If `Shift-I` is pressed, then we open up the entity in the
// secondary information explorer.
std::optional<IMainWindowPlugin::NamedAction>
InformationExplorerPlugin::ActOnKeyPress(const QKeySequence &keys,
                                         const QModelIndex &index) {
  const TriggerHandle *trigger = nullptr;
  if (keys == kKeySeqI) {
    trigger = &update_primary_trigger;

  } else if (keys == kKeySeqShiftI) {
    trigger = &open_secondary_trigger;

  } else {
    return std::nullopt;
  }

  VariantEntity entity = IModel::EntitySkipThroughTokens(index);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return std::nullopt;
  }

  return NamedAction{
    .name = ActionName(entity),
    .action = *trigger,
    .data = QVariant::fromValue(entity)
  };
}

QWidget *InformationExplorerPlugin::CreateDockWidget(QWidget *parent) {
  if (primary_widget) {
    return primary_widget;
  }

  primary_widget = new InformationExplorerWidget(
      context.Index(), context.FileLocationCache(), nullptr /* TODO */,
      true, parent);
  primary_widget->setWindowTitle(tr("Information Explorer"));

  InitializeSignals(primary_widget);

  return primary_widget;
}

void InformationExplorerPlugin::InitializeSignals(
    InformationExplorerWidget *widget) {
  connect(
      widget, &InformationExplorerWidget::SelectedItemChanged,
      [open_entity_trigger = open_entity_trigger] (const QModelIndex &index) {
        open_entity_trigger.Trigger(index.data(IModel::EntityRole));
      });
}

}  // namespace mx::gui
