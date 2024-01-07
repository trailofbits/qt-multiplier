// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/InformationExplorer.h>

#include <QThreadPool>

#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/Index.h>
#include <vector>

#include "EntityInformationModel.h"
#include "EntityInformationRunnable.h"
#include "EntityInformationWidget.h"

namespace mx::gui {
namespace {

static const QKeySequence kKeySeqI("I");
static const QKeySequence kKeySeqShiftI("Shift+I");

}  // namespace

struct InformationExplorer::PrivateData {
  ConfigManager &config_manager;

  std::vector<IInformationExplorerPluginPtr> plugins;

  EntityInformationWidget *view{nullptr};

  // Open the relevant entity.
  TriggerHandle open_entity_trigger;

  // Open an entity's information.
  TriggerHandle entity_info_trigger;
  TriggerHandle pinned_entity_info_trigger;

  inline PrivateData(ConfigManager &config_manager_)
      : config_manager(config_manager_),
        open_entity_trigger(config_manager.ActionManager().Find(
            "com.trailofbits.action.OpenEntity")) {}
};

InformationExplorer::~InformationExplorer(void) {}

InformationExplorer::InformationExplorer(ConfigManager &config_manager,
                                         QMainWindow *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager)) {
  
  d->entity_info_trigger = config_manager.ActionManager().Register(
      this, "com.trailofbits.action.OpenEntityInfo",
      &InformationExplorer::OpenInfo);
  
  d->pinned_entity_info_trigger = config_manager.ActionManager().Register(
      this, "com.trailofbits.action.OpenPinnedEntityInfo",
      &InformationExplorer::OpenPinnedInfo);
}

QWidget *InformationExplorer::CreateDockWidget(QWidget *parent) {
  if (d->view) {
    return d->view;
  }

  d->view = new EntityInformationWidget(d->config_manager, true, parent);

  // When the user navigates the history, make sure that we change what the
  // view shows.
  connect(d->view, &EntityInformationWidget::HistoricalEntitySelected,
          [this] (VariantEntity entity) {
            d->view->DisplayEntity(
                std::move(entity), d->config_manager.FileLocationCache(),
                d->plugins, true  /* explicit request */,
                false  /* add to history */);
          });

  connect(d->view, &EntityInformationWidget::RequestContextMenu,
          this, &IMainWindowPlugin::RequestContextMenu);

  connect(d->view, &EntityInformationWidget::SelectedItemChanged,
          [this] (const QModelIndex &index) {
            auto entity = IModel::Entity(index);
            if (!std::holds_alternative<NotAnEntity>(entity)) {
              d->open_entity_trigger.Trigger(QVariant::fromValue(entity));
            }
          });

  return d->view;
}

void InformationExplorer::ActOnPrimaryClick(const QModelIndex &index) {
  auto entity = IModel::EntitySkipThroughTokens(index);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  d->view->DisplayEntity(
      std::move(entity), d->config_manager.FileLocationCache(), d->plugins,
      false  /* implicit (click) request */, true  /* add to history */);
}

std::optional<NamedAction> InformationExplorer::ActOnSecondaryClick(
    const QModelIndex &index) {

  // Don't allow us to open info from entities shown in the info browser itself.
  // In practice, there isn't a good separation between the entity and the
  // referenced entity, e.g. we show a call (the entity), but it logically
  // references the called function. There may be no way to actually get to
  // the referenced entity.
  //
  // TODO(pag): Consider adding a `.referenced_entity` field to the info
  //            `Item`s.
  if (index.data(IModel::ModelIdRole) ==
      EntityInformationModel::ConstantModelId()) {
    return std::nullopt;
  }

  auto entity = IModel::Entity(index);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return std::nullopt;
  }

  return NamedAction{tr("Open Information"), d->entity_info_trigger,
                     QVariant::fromValue(entity)};
}

// Expose an action on key press.
std::optional<NamedAction> InformationExplorer::ActOnKeyPress(
    const QKeySequence &keys, const QModelIndex &index) {

  TriggerHandle *handle = nullptr;
  QString name;

  if (keys == kKeySeqI) {
    handle = &(d->entity_info_trigger);
    name = tr("Open Entity Info");

  } else if (keys == kKeySeqShiftI) {
    handle = &(d->pinned_entity_info_trigger);
    name = tr("Open Pinned Entity Info");

  } else {
    return std::nullopt;
  }

  auto entity = IModel::Entity(index);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return std::nullopt;
  }

  return NamedAction{name, *handle, QVariant::fromValue(entity)};
}

void InformationExplorer::OpenInfo(const QVariant &data) {
  if (!data.isValid() || !data.canConvert<VariantEntity>()) {
    return;
  }

  auto entity = data.value<VariantEntity>();
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  d->view->DisplayEntity(
      std::move(entity), d->config_manager.FileLocationCache(), d->plugins,
      false  /* implicit (click) request */, true  /* add to history */);
}

void InformationExplorer::OpenPinnedInfo(const QVariant &data) {
  if (!data.isValid() || !data.canConvert<VariantEntity>()) {
    return;
  }

  auto entity = data.value<VariantEntity>();
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }
}

void InformationExplorer::AddPlugin(IInformationExplorerPluginPtr plugin) {
  if (plugin) {
    d->plugins.emplace_back(std::move(plugin));
  }
}

}  // namespace mx::gui
