// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/HighlightExplorer.h>

#include <QColor>
#include <QColorDialog>
#include <QListView>
#include <QPoint>
#include <QVBoxLayout>

#include <multiplier/AST/NamedDecl.h>
#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Interfaces/IWindowManager.h>
#include <multiplier/GUI/Interfaces/IWindowWidget.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>
#include <multiplier/Frontend/DefineMacroDirective.h>
#include <multiplier/Frontend/MacroParameter.h>
#include <multiplier/Index.h>
#include <vector>

#include "HighlightedItemsModel.h"
#include "HighlightThemeProxy.h"

namespace mx::gui {

struct HighlightExplorer::PrivateData {
  ConfigManager &config_manager;
  ThemeManager &theme_manager;
  const TriggerHandle open_entity_trigger;

  HighlightThemeProxy *proxy{nullptr};
  VariantEntity entity;
  std::vector<RawEntityId> eids;
  HighlightedItemsModel *model{nullptr};
  QListView *view{nullptr};
  IWindowWidget *dock{nullptr};

  inline PrivateData(ConfigManager &config_manager_)
      : config_manager(config_manager_),
        theme_manager(config_manager.ThemeManager()),
        open_entity_trigger(config_manager.ActionManager().Find(
            "com.trailofbits.action.OpenEntity")) {}
};

HighlightExplorer::~HighlightExplorer(void) {}

HighlightExplorer::HighlightExplorer(ConfigManager &config_manager,
                                     IWindowManager *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager)) {

  connect(&config_manager, &ConfigManager::IndexChanged,
          this, &HighlightExplorer::OnIndexChanged);

  CreateDockWidget(parent);
}

void HighlightExplorer::CreateDockWidget(IWindowManager *manager) {

  d->dock = new IWindowWidget;
  d->dock->setWindowTitle(tr("Highlight Explorer"));
  d->dock->setContentsMargins(0, 0, 0, 0);

  d->view = new QListView(d->dock);

  // Install an `IModel`-compatible model listing only our highlighted entities.
  d->model = new HighlightedItemsModel(d->view);
  d->view->setModel(d->model);

  // Turn on coloring based on the model.
  d->config_manager.InstallItemDelegate(d->view);

  // Enable context menus on the list itself.
  d->view->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(d->view, &QListView::customContextMenuRequested,
          [=, this] (const QPoint &point) {
            auto index = d->view->indexAt(point);
            if (index.isValid()) {
              manager->OnSecondaryClick(index);
            }
          });

  connect(d->view, &QAbstractItemView::clicked,
          this, &IMainWindowPlugin::RequestPrimaryClick);

  connect(d->view->selectionModel(), &QItemSelectionModel::currentChanged,
          this, &IMainWindowPlugin::RequestPrimaryClick);

  auto dock_layout = new QVBoxLayout(d->dock);
  dock_layout->setContentsMargins(0, 0, 0, 0);
  dock_layout->addWidget(d->view, 1);
  dock_layout->addStretch();
  d->dock->setLayout(dock_layout);

  IWindowManager::DockConfig config;
  config.tabify = true;
  config.id = "com.trailofbits.dock.HighlightExplorer";
  config.app_menu_location = {tr("View"), tr("Explorers")};
  manager->AddDockWidget(d->dock, config);
}

// When clicked, open the entity.
void HighlightExplorer::ActOnPrimaryClick(
    IWindowManager *, const QModelIndex &index) {
  d->entity = {};

  if (!d->model || index.model() != d->model) {
    return;
  }

  auto entity = IModel::Entity(index);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  d->view->setCurrentIndex(QModelIndex());
  d->open_entity_trigger.Trigger(QVariant::fromValue(entity));
}

void HighlightExplorer::ActOnContextMenu(
    IWindowManager *, QMenu *menu, const QModelIndex &index) {

  if (!d->view) {
    return;
  }

  d->eids.clear();
  d->entity = IModel::EntitySkipThroughTokens(index);

  // It's only reasonable to add highlights on named entities.
  if (!DefineMacroDirective::from(d->entity) &&
      !MacroParameter::from(d->entity) && !NamedDecl::from(d->entity)) {
    d->entity = {};
    return;
  }

  // If the entity is a declaration then go get the canonical declaration, and
  // collect the set of all redeclaration IDs, so that we can highlight all
  // variants of this entity.
  if (std::holds_alternative<Decl>(d->entity)) {
    auto decl = std::get<Decl>(d->entity).canonical_declaration();
    d->entity = decl;

    for (auto redecl : decl.redeclarations()) {
      d->eids.push_back(redecl.id().Pack());
    }
  } else {
    d->eids.push_back(EntityId(d->entity).Pack());
  }

  if (d->eids.empty() || d->eids.back() == kInvalidEntityId) {
    return;
  }

  auto present = false;
  if (d->proxy) {
    present = d->proxy->color_map.count(d->eids[0]) != 0;
  }

  auto highlight_menu = new QMenu(tr("Highlights"), menu);
  menu->addMenu(highlight_menu);

  auto set_entity_highlight = new QAction(tr("Set Color"), highlight_menu);
  highlight_menu->addAction(set_entity_highlight);
  connect(set_entity_highlight, &QAction::triggered,
          this, &HighlightExplorer::SetColor);

  if (present) {
    auto remove_entity_highlight = new QAction(tr("Remove"), highlight_menu);
    highlight_menu->addAction(remove_entity_highlight);
    connect(remove_entity_highlight, &QAction::triggered,
            this, &HighlightExplorer::RemoveColor);
  }

  if (d->proxy && !d->proxy->color_map.empty()) {
    auto reset_entity_highlights = new QAction(tr("Reset"), highlight_menu);
    highlight_menu->addAction(reset_entity_highlights);
    connect(reset_entity_highlights, &QAction::triggered,
            this, &HighlightExplorer::ClearAllColors);
  }
}

void HighlightExplorer::ColorsUpdated(void) {
  d->eids.clear();
  if (!d->proxy) {
    return;
  }

  if (d->proxy->color_map.empty()) {
    d->proxy->UninstallFromOwningManager();
    d->proxy = nullptr;
  } else {
    d->proxy->SendUpdate();
  }
}

void HighlightExplorer::OnIndexChanged(const ConfigManager &) {
  ClearAllColors();
}

void HighlightExplorer::SetColor(void) {
  if (!d->model || d->eids.empty() || std::holds_alternative<NotAnEntity>(d->entity)) {
    return;
  }

  QColor color = QColorDialog::getColor();
  if (!color.isValid()) {
    d->eids.clear();
    return;
  }

  auto made_proxy = false;
  if (!d->proxy) {
    made_proxy = true;
    d->proxy = new HighlightThemeProxy;
  }

  d->view->setCurrentIndex(QModelIndex());
  if (!d->proxy->color_map.count(d->eids[0])) {
    d->model->AddEntity(d->entity);
  }

  for (auto eid : d->eids) {
    d->proxy->color_map.erase(eid);
    d->proxy->color_map.try_emplace(
        eid, ITheme::ContrastingColor(color), color);
  }
  
  if (made_proxy) {
    d->theme_manager.AddProxy(IThemeProxyPtr(d->proxy));
  } else {
    ColorsUpdated();
  }
}

void HighlightExplorer::RemoveColor(void) {
  if (!d->proxy || !d->model) {
    return;
  }

  if (d->eids.empty() || std::holds_alternative<NotAnEntity>(d->entity)) {
    return;
  }

  for (auto eid : d->eids) {
    d->proxy->color_map.erase(eid);
  }

  d->view->setCurrentIndex(QModelIndex());
  d->model->RemoveEntity(d->eids);
  ColorsUpdated();
}

void HighlightExplorer::ClearAllColors(void) {
  d->eids.clear();
  for (auto &[eid, cs] : d->proxy->color_map) {
    d->eids.push_back(eid);
  }

  d->model->RemoveEntity(d->eids);
  d->proxy->color_map.clear();
  ColorsUpdated();
}

}  // namespace mx::gui
