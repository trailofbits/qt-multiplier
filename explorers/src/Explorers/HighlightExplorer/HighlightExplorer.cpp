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

#include <QColor>
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
  VariantEntity entity;
  std::vector<RawEntityId> eids;
  HighlightedItemsModel *model{nullptr};
  QListView *view{nullptr};
  IWindowManager *manager{nullptr};
  HighlightExplorerWindowWidget *dock{nullptr};

  TriggerHandle set_rand_highlight;
  std::unique_ptr<ColorGenerator> color_generator;

  inline PrivateData(ConfigManager &config_manager_)
      : config_manager(config_manager_),
        theme_manager(config_manager.ThemeManager()),
        open_entity_trigger(config_manager.ActionManager().Find(
            "com.trailofbits.action.OpenEntity")) {}

  void ClearAllColors(HighlightExplorer *self);
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
  d->set_rand_highlight = action_manager.Register(
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

void
HighlightExplorer::InitializeFromModelIndex(const QModelIndex &index) {
  InitializeFromVariantEntity(IModel::EntitySkipThroughTokens(index));
}

void
HighlightExplorer::InitializeFromVariantEntity(const VariantEntity &var_entity) {
  d->eids.clear();
  d->entity = var_entity;

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
}

void HighlightExplorer::ActOnContextMenu(
    IWindowManager *, QMenu *menu, const QModelIndex &index) {

  InitializeFromModelIndex(index);

  auto present = false;
  if (d->proxy) {
    present = d->proxy->color_map.count(d->eids[0]) != 0;
  }

  auto highlight_menu = new QMenu(tr("Highlights"), menu);
  menu->addMenu(highlight_menu);

  auto set_entity_highlight = new QAction(tr("Set Color"), highlight_menu);
  highlight_menu->addAction(set_entity_highlight);
  connect(set_entity_highlight, &QAction::triggered,
          this, &HighlightExplorer::SetUserColor);

  auto set_rand_entity_highlight = new QAction(tr("Set Random Color"), highlight_menu);
  highlight_menu->addAction(set_rand_entity_highlight);
  connect(set_rand_entity_highlight, &QAction::triggered,
          this, &HighlightExplorer::SetRandomColor);

  bool menu_separator_added{false};
  const auto L_addMenuSeparator = [&menu_separator_added,
                                   &highlight_menu]() {
    if (menu_separator_added) {
      return;
    }

    menu_separator_added = true;
    highlight_menu->addSeparator();
  };

  if (present) {
    L_addMenuSeparator();

    auto remove_entity_highlight = new QAction(tr("Remove"), highlight_menu);
    highlight_menu->addAction(remove_entity_highlight);
    connect(remove_entity_highlight, &QAction::triggered,
            this, &HighlightExplorer::RemoveColor);
  }

  if (d->proxy && !d->proxy->color_map.empty()) {
    L_addMenuSeparator();

    auto reset_entity_highlights = new QAction(tr("Reset All"), highlight_menu);
    highlight_menu->addAction(reset_entity_highlights);
    connect(reset_entity_highlights, &QAction::triggered,
            this, &HighlightExplorer::ClearAllColors);
  }
}

std::optional<NamedAction>
HighlightExplorer::ActOnKeyPress(IWindowManager *,
                                 const QKeySequence &keys,
                                 const QModelIndex &index) {

  if (keys == kToggleHighlightColorKeySeq) {
    return NamedAction{
        .name = "com.trailofbits.action.ToggleHighlightColor",
        .action = d->set_rand_highlight,
        .data = index};
  }

  return std::nullopt;
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

void HighlightExplorer::SetColor(const QColor &color) {
  if (d->eids.empty() || std::holds_alternative<NotAnEntity>(d->entity)) {
    return;
  }

  if (!d->dock) {
    CreateDockWidget();
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

  d->dock->EmitRequestAttention();
}

void HighlightExplorer::OnIndexChanged(const ConfigManager &) {
  d->ClearAllColors(this);
}

void HighlightExplorer::SetUserColor(void) {
  QColor color = QColorDialog::getColor();
  if (!color.isValid()) {
    return;
  }

  SetColor(color);
}

void HighlightExplorer::SetRandomColor(void) {
  SetColor(d->color_generator->Next());
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

  d->dock->EmitRequestAttention();
}

void HighlightExplorer::PrivateData::ClearAllColors(HighlightExplorer *self) {
  eids.clear();
  for (auto &[eid, cs] : proxy->color_map) {
    eids.push_back(eid);
  }

  model->RemoveEntity(eids);
  proxy->color_map.clear();
  self->ColorsUpdated();
}

void HighlightExplorer::ClearAllColors(void) {
  auto reply = QMessageBox::question(
      d->view, tr("Reset all highlights?"),
      tr("Are you sure that you want to remove all highlights?"),
      QMessageBox::Yes | QMessageBox::No);
  if (reply == QMessageBox::Yes) {
    d->ClearAllColors(this);
  }
}

void HighlightExplorer::OnThemeChanged(const ThemeManager &theme_manager) {
  if (d->color_generator != nullptr) {
    return;
  }

  auto background_color{theme_manager.Theme()->DefaultBackgroundColor()};
  d->color_generator = std::make_unique<ColorGenerator>(kRandomColorSeed,
                                                        background_color,
                                                        kRandomColorSaturation);
}

void HighlightExplorer::OnToggleHighlightColorAction(const QVariant &data) {
  if (data.isNull()) {
    return;
  }

  if (data.canConvert<VariantEntity>()) {
    const auto &var_entity = data.value<VariantEntity>();
    InitializeFromVariantEntity(var_entity);

  } else if (data.canConvert<QModelIndex>()) {
    const auto &model_index = data.value<QModelIndex>();
    InitializeFromModelIndex(model_index);

  } else {
    return;
  }

  auto present{false};
  if (d->proxy != nullptr) {
    for (const auto &entity_id : d->eids) {
      if (d->proxy->color_map.count(entity_id) > 0) {
        present = true;
        break;
      }
    }
  }

  if (present) {
    RemoveColor();
  } else {
    SetRandomColor();
  }
}

}  // namespace mx::gui
