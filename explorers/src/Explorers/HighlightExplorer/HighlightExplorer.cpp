// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "HighlightedItemsModel.h"
#include "HighlightThemeProxy.h"
#include "ColorGenerator.h"

#include <multiplier/GUI/Explorers/HighlightExplorer.h>
#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Interfaces/IWindowManager.h>
#include <multiplier/GUI/Interfaces/IWindowWidget.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>

#include <multiplier/AST/NamedDecl.h>
#include <multiplier/Frontend/DefineMacroDirective.h>
#include <multiplier/Frontend/MacroParameter.h>

#include <QColorDialog>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QPoint>
#include <QVBoxLayout>

#include <vector>

namespace mx::gui {

namespace {

static const QKeySequence kToggleHighlightColorKeySeq("h");

// Color generator parameters; we are using a predictable
// sequence for now.
constexpr float kRandomColorSaturation{0.7f};
constexpr int kRandomColorSeed{0};

// Since our logic is not inside a class deriving from IWindowWidget, we
// have to define a method that lets HighlightExplorer issue the
// RequestAttention signal.
//
// Without this, the main window won't know when to show again the dock
// widget once it gets closed.
class HighlightExplorerWindowWidget Q_DECL_FINAL : public IWindowWidget {
 public:
  HighlightExplorerWindowWidget(void) = default;

  void EmitRequestAttention(void) {
    emit RequestAttention();
  }
};

}  // namespace

struct HighlightExplorer::PrivateData {
  ConfigManager &config_manager;
  ThemeManager &theme_manager;
  const TriggerHandle open_entity_trigger;

  HighlightThemeProxy *proxy{nullptr};
  HighlightedItemsModel *model{nullptr};
  QListView *view{nullptr};
  IWindowManager *manager{nullptr};
  HighlightExplorerWindowWidget *dock{nullptr};

  bool color_update_scheduled{false};
  TriggerHandle toggle_highlight_color_trigger;
  std::unique_ptr<ColorGenerator> color_generator;
  std::vector<EntityInformation> random_highlight_list;

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

  d->manager = parent;

  connect(&d->theme_manager, &ThemeManager::ThemeChanged,
          this, &HighlightExplorer::OnThemeChanged);

  OnThemeChanged(d->theme_manager);

  auto &action_manager = config_manager.ActionManager();
  d->toggle_highlight_color_trigger = action_manager.Register(
      this, "com.trailofbits.action.ToggleHighlightColor",
      &HighlightExplorer::OnToggleHighlightColorAction);
}

void HighlightExplorer::CreateDockWidget(void) {

  d->dock = new HighlightExplorerWindowWidget;
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
          [this] (const QPoint &point) {
            auto index = d->view->indexAt(point);
            if (index.isValid()) {
              d->manager->OnSecondaryClick(index);
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
  d->manager->AddDockWidget(d->dock, config);
}

void HighlightExplorer::ActOnContextMenu(
    IWindowManager *, QMenu *menu, const QModelIndex &index) {

  auto opt_entity_info = QueryEntityInformation(index);
  if (!opt_entity_info.has_value()) {
    return;
  }

  const auto &entity_information = opt_entity_info.value();

  auto highlight_menu = new QMenu(tr("Highlights"), menu);
  menu->addMenu(highlight_menu);

  auto set_entity_highlight = new QAction(tr("Set Color"), highlight_menu);
  highlight_menu->addAction(set_entity_highlight);
  connect(
    set_entity_highlight,
    &QAction::triggered,
    this,
    [=, this]() {
      QColor color = QColorDialog::getColor();
      if (!color.isValid()) {
        return;
      }

      if (IsEntityHighlighted(entity_information)) {
        RemoveEntityHighlight(entity_information);
      }

      SetEntityHighlight(entity_information, color);
      EmitColorUpdate();
    }
  );

  auto set_rand_entity_highlight = new QAction(tr("Set Random Color"), highlight_menu);
  highlight_menu->addAction(set_rand_entity_highlight);
  connect(
    set_rand_entity_highlight,
    &QAction::triggered,
    this,
    [=, this]() {
      if (IsEntityHighlighted(entity_information)) {
        RemoveEntityHighlight(entity_information);
      }

      SetEntityHighlight(entity_information);
      EmitColorUpdate();
    }
  );

  bool menu_separator_added{false};
  const auto L_addMenuSeparator = [&menu_separator_added,
                                   &highlight_menu]() {
    if (menu_separator_added) {
      return;
    }

    menu_separator_added = true;
    highlight_menu->addSeparator();
  };

  if (IsEntityHighlighted(entity_information)) {
    L_addMenuSeparator();

    auto remove_entity_highlight = new QAction(tr("Remove"), highlight_menu);
    highlight_menu->addAction(remove_entity_highlight);
    connect(
      remove_entity_highlight,
      &QAction::triggered,
      this,
      [=, this]() {
        RemoveEntityHighlight(entity_information);
        EmitColorUpdate();
      }
    );
  }

  if (d->proxy && !d->proxy->color_map.empty()) {
    L_addMenuSeparator();

    auto reset_entity_highlights = new QAction(tr("Reset All"), highlight_menu);
    highlight_menu->addAction(reset_entity_highlights);
    connect(
      reset_entity_highlights,
      &QAction::triggered,
      this,
      [=, this]() {
        ClearAllHighlights();
        EmitColorUpdate();
      }
    );
  }
}

std::optional<NamedAction>
HighlightExplorer::ActOnKeyPress(IWindowManager *,
                                 const QKeySequence &keys,
                                 const QModelIndex &index) {

  if (keys == kToggleHighlightColorKeySeq) {
    return NamedAction{
        .name = "com.trailofbits.action.ToggleHighlightColor",
        .action = d->toggle_highlight_color_trigger,
        .data = index};
  }

  return std::nullopt;
}

void HighlightExplorer::ColorsUpdated(void) {
  if (d->proxy == nullptr) {
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
  EmitColorUpdate();
}

void HighlightExplorer::ClearAllHighlights() {
  d->random_highlight_list.clear();
  d->color_update_scheduled = false;

  std::vector<RawEntityId> entity_id_list;
  for (auto &[eid, cs] : d->proxy->color_map) {
    entity_id_list.push_back(eid);
  }

  d->model->RemoveEntity(entity_id_list);
  d->proxy->color_map.clear();
  ScheduleColorUpdate();
}

void HighlightExplorer::ClearAllColors(void) {
  auto reply = QMessageBox::question(
      d->view, tr("Reset all highlights?"),
      tr("Are you sure that you want to remove all highlights?"),
      QMessageBox::Yes | QMessageBox::No);

  if (reply != QMessageBox::Yes) {
    return;
  }

  ClearAllHighlights();
}

void HighlightExplorer::OnThemeChanged(const ThemeManager &theme_manager) {
  // NOTE: The theme manager will issue updates even if the application
  // theme is not changed. Skip unneeded updates by checking the
  // background color
  auto background_color{theme_manager.Theme()->DefaultBackgroundColor()};

  if (d->color_generator != nullptr) {
    const auto &ref_background_color = d->color_generator->ReferenceBackgroundColor();
    if (background_color == ref_background_color) {
      return;
    }
  }

  d->color_generator = std::make_unique<ColorGenerator>(kRandomColorSeed,
                                                        background_color,
                                                        kRandomColorSaturation);

  auto random_highlight_list = d->random_highlight_list;
  for (const auto &random_highlight : random_highlight_list) {
    RemoveEntityHighlight(random_highlight);
  }

  for (const auto &random_highlight : random_highlight_list) {
    SetEntityHighlight(random_highlight);
  }

  EmitColorUpdate();
}

void HighlightExplorer::OnToggleHighlightColorAction(const QVariant &data) {
  auto opt_entity_info = QueryEntityInformation(data);
  if (!opt_entity_info.has_value()) {
    return;
  }

  const auto &entity_information = opt_entity_info.value();
  if (IsEntityHighlighted(entity_information)) {
    RemoveEntityHighlight(entity_information);
  } else {
    SetEntityHighlight(entity_information);
  }

  EmitColorUpdate();
}

std::optional<HighlightExplorer::EntityInformation>
HighlightExplorer::QueryEntityInformation(const VariantEntity &var_entity) {
  // It's only reasonable to add highlights on named entities.
  if (!DefineMacroDirective::from(var_entity) &&
      !MacroParameter::from(var_entity) &&
      !NamedDecl::from(var_entity)) {
    return std::nullopt;
  }

  EntityInformation entity_info;
  entity_info.var_entity = var_entity;
  entity_info.deref_var_entity = var_entity;

  // If the entity is a declaration then go get the canonical declaration, and
  // collect the set of all redeclaration IDs, so that we can highlight all
  // variants of this entity.
  if (std::holds_alternative<Decl>(var_entity)) {
    auto decl = std::get<Decl>(var_entity).canonical_declaration();
    entity_info.deref_var_entity = decl;

    for (auto redecl : decl.redeclarations()) {
      entity_info.id_list.push_back(redecl.id().Pack());
    }
  } else {
    entity_info.id_list.push_back(EntityId(var_entity).Pack());
  }

  if (entity_info.id_list.empty() ||
      entity_info.id_list.back() == kInvalidEntityId ||
      std::holds_alternative<NotAnEntity>(entity_info.deref_var_entity)) {
    return std::nullopt;
  }

  return entity_info;
}

std::optional<HighlightExplorer::EntityInformation>
HighlightExplorer::QueryEntityInformation(const QModelIndex &index) {
  return QueryEntityInformation(IModel::EntitySkipThroughTokens(index));
}

std::optional<HighlightExplorer::EntityInformation>
HighlightExplorer::QueryEntityInformation(const QVariant &var) {
  if (var.isNull()) {
    return std::nullopt;

  } else if (var.canConvert<VariantEntity>()) {
    const auto &var_entity = var.value<VariantEntity>();
    return QueryEntityInformation(var_entity);

  } else if (var.canConvert<QModelIndex>()) {
    const auto &index = var.value<QModelIndex>();
    return QueryEntityInformation(index);

  } else {
    return std::nullopt;
  }
}

bool
HighlightExplorer::IsEntityHighlighted(const EntityInformation &entity_info) {
  if (d->proxy == nullptr) {
    return false;
  }

  const auto &color_map = d->proxy->color_map;

  for (const auto &entity_id : entity_info.id_list) {
    if (color_map.count(entity_id) > 0) {
      return true;
    }
  }

  return false;
}

void
HighlightExplorer::RemoveEntityHighlight(const EntityInformation &entity_info) {
  if (!d->proxy || !d->model) {
    return;
  }

  auto &color_map = d->proxy->color_map;
  for (const auto &entity_id : entity_info.id_list) {
    color_map.erase(entity_id);
  }

  // Make sure to also remove it from the list of random highlights
  auto random_highlight_list_it = std::find_if(
    d->random_highlight_list.begin(),
    d->random_highlight_list.end(),

    [&](const EntityInformation &rand_highlight) -> bool {
      auto id_list_it = std::find(
        rand_highlight.id_list.begin(),
        rand_highlight.id_list.end(),
        entity_info.id_list.front()
      );

      return id_list_it != rand_highlight.id_list.end();
    }
  );

  if (random_highlight_list_it != d->random_highlight_list.end()) {
    d->random_highlight_list.erase(random_highlight_list_it);
  }

  d->view->setCurrentIndex(QModelIndex());
  d->model->RemoveEntity(entity_info.id_list);
  ScheduleColorUpdate();
}

void
HighlightExplorer::SetEntityHighlight(const EntityInformation &entity_info,
                                      const std::optional<QColor> &opt_color) {
  if (!d->dock) {
    CreateDockWidget();
  }

  auto made_proxy = false;
  if (!d->proxy) {
    made_proxy = true;
    d->proxy = new HighlightThemeProxy;
  }

  d->view->setCurrentIndex(QModelIndex());
  if (!d->proxy->color_map.count(entity_info.id_list[0])) {
    d->model->AddEntity(entity_info.deref_var_entity);
  }

  auto color = opt_color.value_or(d->color_generator->Next());

  for (const auto &entity_id : entity_info.id_list) {
    d->proxy->color_map.erase(entity_id);
    d->proxy->color_map.try_emplace(
        entity_id, ITheme::ContrastingColor(color), color);
  }
  
  if (made_proxy) {
    d->theme_manager.AddProxy(IThemeProxyPtr(d->proxy));
  } else {
    ScheduleColorUpdate();
  }

  // Save random color highlights for later
  if (!opt_color.has_value()) {
    d->random_highlight_list.push_back(entity_info);
  }
}

void
HighlightExplorer::ScheduleColorUpdate() {
  d->color_update_scheduled = true;
}

void
HighlightExplorer::EmitColorUpdate() {
  if (!d->color_update_scheduled) {
    return;
  }

  d->color_update_scheduled = false;

  ColorsUpdated();
  d->dock->EmitRequestAttention();
}

}  // namespace mx::gui
