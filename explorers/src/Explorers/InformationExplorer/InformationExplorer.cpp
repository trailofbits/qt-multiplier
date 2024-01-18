// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/InformationExplorer.h>

#include <QThreadPool>

#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Interfaces/IWindowManager.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Util.h>
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

  IWindowManager *window_manager{nullptr};

  inline PrivateData(ConfigManager &config_manager_)
      : config_manager(config_manager_),
        open_entity_trigger(config_manager.ActionManager().Find(
            "com.trailofbits.action.OpenEntity")) {}
};

InformationExplorer::~InformationExplorer(void) {}

InformationExplorer::InformationExplorer(ConfigManager &config_manager,
                                         IWindowManager *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager)) {
  
  d->entity_info_trigger = config_manager.ActionManager().Register(
      this, "com.trailofbits.action.OpenEntityInfo",
      &InformationExplorer::OpenInfo);
  
  d->pinned_entity_info_trigger = config_manager.ActionManager().Register(
      this, "com.trailofbits.action.OpenPinnedEntityInfo",
      &InformationExplorer::OpenPinnedInfo);

  d->window_manager = parent;
  CreateDockWidget(parent);
}

void InformationExplorer::CreateDockWidget(IWindowManager *manager) {
  d->view = new EntityInformationWidget(d->config_manager, true);

  // When the user navigates the history, make sure that we change what the
  // view shows.
  connect(d->view, &EntityInformationWidget::HistoricalEntitySelected,
          [this] (VariantEntity entity) {
            d->view->DisplayEntity(
                std::move(entity), d->config_manager.FileLocationCache(),
                d->plugins, true  /* explicit request */,
                false  /* add to history */);
          });

  connect(d->view, &EntityInformationWidget::SelectedItemChanged,
          [this] (const QModelIndex &index) {
            auto entity = IModel::Entity(index);
            if (!std::holds_alternative<NotAnEntity>(entity)) {
              d->open_entity_trigger.Trigger(QVariant::fromValue(entity));
            }
          });
  
  IWindowManager::DockConfig config;
  config.id = "com.trailofbits.dock.InformationExplorer";
  config.app_menu_location = {tr("View"), tr("Explorers")};
  manager->AddDockWidget(d->view, config);
}

void InformationExplorer::ActOnPrimaryClick(
    IWindowManager *, const QModelIndex &index) {
  if (!d->view->isVisible()) {
    return;
  }

  auto entity = IModel::EntitySkipThroughTokens(index);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  d->view->DisplayEntity(
      std::move(entity), d->config_manager.FileLocationCache(), d->plugins,
      false  /* implicit (click) request */, true  /* add to history */);
}

std::optional<NamedAction> InformationExplorer::ActOnSecondaryClick(
    IWindowManager *, const QModelIndex &index) {

  auto entity = IModel::EntitySkipThroughTokens(index);

  // Don't allow us to open info from entities shown in the info browser itself.
  // In practice, there isn't a good separation between the entity and the
  // referenced entity, e.g. we show a call (the entity), but it logically
  // references the called function. There may be no way to actually get to
  // the referenced entity.
  if (index.data(IModel::ModelIdRole) ==
      EntityInformationModel::ConstantModelId()) {
    
    auto rel_var = index.data(EntityInformationModel::ReferencedEntityRole);
    if (!rel_var.isValid() || !rel_var.canConvert<VariantEntity>()) {
      return std::nullopt;
    }

    entity = rel_var.value<VariantEntity>();
  }

  if (std::holds_alternative<NotAnEntity>(entity)) {
    return std::nullopt;
  }

  return NamedAction{tr("Open Information"), d->entity_info_trigger,
                     QVariant::fromValue(entity)};
}

// Expose an action on key press.
std::optional<NamedAction> InformationExplorer::ActOnKeyPress(
    IWindowManager *, const QKeySequence &keys, const QModelIndex &index) {

  TriggerHandle *handle = nullptr;
  QString name;

  if (keys == kKeySeqI) {
    handle = &(d->entity_info_trigger);
    name = tr("Open Information");

  } else if (keys == kKeySeqShiftI) {
    handle = &(d->pinned_entity_info_trigger);
    name = tr("Open Pinned Information");

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

  d->view->show();
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

  auto view = new EntityInformationWidget(
      d->config_manager, false  /* no history */);

  if (auto name = NameOfEntityAsString(entity)) {
    view->setWindowTitle(
        tr("Information about `%1`").arg(name.value()));
  }

  view->show();
  view->DisplayEntity(
      std::move(entity), d->config_manager.FileLocationCache(), d->plugins,
      true  /* explicit request */, false  /* don't add to history */);

  IWindowManager::DockConfig config;
  config.location = IWindowManager::DockLocation::Right;
  config.delete_on_close = true;
  d->window_manager->AddDockWidget(view, config);
}

void InformationExplorer::AddPlugin(IInformationExplorerPluginPtr plugin) {
  if (plugin) {
    d->plugins.emplace_back(std::move(plugin));
  }
}

}  // namespace mx::gui
