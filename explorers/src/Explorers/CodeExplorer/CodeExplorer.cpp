// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/CodeExplorer.h>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QKeySequence>
#include <QMenu>

#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Interfaces/IWindowManager.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Widgets/CodeWidget.h>
#include <multiplier/GUI/Widgets/HistoryWidget.h>
#include <multiplier/GUI/Util.h>
#include <multiplier/Frontend/TokenTree.h>
#include <multiplier/Index.h>
#include <unordered_map>

#include "CodePreviewWidget.h"
#include "ExpandedMacrosModel.h"
#include "MacroExplorer.h"

namespace mx::gui {
namespace {

static const unsigned kMaxHistorySize = 32u;

static const QKeySequence kKeySeqE("E");
static const QKeySequence kKeySeqP("P");
static const QKeySequence kKeySeqShiftP("Shift+P");

static const QString kOpenEntityModelId("com.trailofbits.CodeViewModel");

// Figure out what entity to use for macro expansion.
static VariantEntity EntityForExpansion(VariantEntity entity) {

  // It's a token; see if we can find it inside of a macro.
  if (std::holds_alternative<Token>(entity)) {
    for (Macro macro : Macro::containing(std::get<Token>(entity))) {
      entity = macro;
      if (!IncludeLikeMacroDirective::from(macro)) {
        break;
      }
    }
  }

  // It's still a token; get the related entity.
  if (std::holds_alternative<Token>(entity)) {
    entity = std::get<Token>(entity).related_entity();
  }

  if (!std::holds_alternative<Macro>(entity)) {
    return NotAnEntity{};
  }

  switch (std::get<Macro>(entity).kind()) {
    case MacroKind::DEFINE_DIRECTIVE:
    case MacroKind::SUBSTITUTION:
    case MacroKind::STRINGIFY:
    case MacroKind::CONCATENATE:
    case MacroKind::PARAMETER_SUBSTITUTION:
    case MacroKind::EXPANSION:
      return entity;
    default:
      return NotAnEntity{};
  }
}

using Location = std::pair<VariantEntity, CodeWidget::OpaqueLocation>;

}  // namespace

struct CodeExplorer::PrivateData {
  ConfigManager &config_manager;
  IWindowManager * const manager;
  CodePreviewWidget *preview{nullptr};
  HistoryWidget * const history;

  std::unordered_map<RawEntityId, std::pair<VariantEntity, CodeWidget *>>
      opened_windows;

  TriggerHandle expand_macro_trigger;
  TriggerHandle open_user_preview_trigger;
  TriggerHandle open_pinned_preview_trigger;
  TriggerHandle browse_mode_trigger;

  // TODO(pag): `connect` the same signals to update `scene_options` to keep
  //            them in sync with other things.
  CodeWidget::SceneOptions scene_options;

  ExpandedMacrosModel *macro_explorer_model{nullptr};
  MacroExplorer *macro_explorer{nullptr};

  bool browse_mode{false};
  QAction *browse_mode_action{nullptr};

  inline PrivateData(ConfigManager &config_manager_,
                     IWindowManager *manager_)
      : config_manager(config_manager_),
        manager(manager_),
        history(new HistoryWidget(
            config_manager, kMaxHistorySize,
            true  /* install global shortcuts */)) {}

  std::pair<VariantEntity, CodeWidget *> CurrentOpenCodeWidget(void) const {
    for (auto &[id, entity_widget] : opened_windows) {
      if (entity_widget.second->isVisible()) {
        return entity_widget;
      }
    }
    return {{}, nullptr};
  }

  void AddCurrentToHistory(void) {
    auto [ent, prev_widget] = CurrentOpenCodeWidget();
    if (prev_widget) {
      auto loc = prev_widget->LastLocation();
      history->SetCurrentItem(QVariant::fromValue(Location(ent, loc)));
    }
  }
};

CodeExplorer::~CodeExplorer(void) {}

CodeExplorer::CodeExplorer(ConfigManager &config_manager,
                           IWindowManager *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager, parent)) {

  auto &action_manager = config_manager.ActionManager();
  auto &media_manager = config_manager.MediaManager();

  action_manager.Register(
      this, "com.trailofbits.action.OpenEntity",
      &CodeExplorer::OnOpenEntity);

  d->expand_macro_trigger = action_manager.Register(
      this, "com.trailofbits.action.ExpandMacro",
      &CodeExplorer::OnExpandMacro);

  (void) action_manager.Register(
      this, "com.trailofbits.action.OpenEntityPreview",
      &CodeExplorer::OnImplicitPreviewEntity);

  d->open_user_preview_trigger = action_manager.Register(
      this, "com.trailofbits.action.OpenUserRequestedEntityPreview",
      &CodeExplorer::OnExplicitPreviewEntity);

  d->open_pinned_preview_trigger = action_manager.Register(
      this, "com.trailofbits.action.OpenPinnedEntityPreview",
      &CodeExplorer::OnPinnedPreviewEntity);

  d->browse_mode_trigger = action_manager.Register(
      this, "com.trailofbits.action.ToggleBrowseMode",
      &CodeExplorer::OnToggleBrowseMode);

  parent->AddToolBarWidget(d->history);

  d->browse_mode_action = parent->AddDepressableToolBarButton(
      media_manager.Pixmap("com.trailofbits.icon.BrowseMode"),
      tr("Browse Mode"), d->browse_mode_trigger);

  d->browse_mode_action->setChecked(true);

  // When the user navigates the history, make sure that we change what the
  // view shows.
  connect(d->history, &HistoryWidget::GoToHistoricalItem,
          this, &CodeExplorer::OnGoToHistoricalItem);
}

void CodeExplorer::OnToggleBrowseMode(const QVariant &data) {
  d->browse_mode = data.toBool();
}

void CodeExplorer::ActOnPrimaryClick(IWindowManager *,
                                     const QModelIndex &index) {
  if (!index.isValid()) {
    return;
  }

  // Clicking on something in the code view should open the code.
  auto model_id = IModel::ModelId(index);
  if (model_id == CodePreviewWidget::kModelId ||
      model_id == kOpenEntityModelId) {

    // When we click on an entity in a code view, try to bring us to its
    // definition.
    auto entity = IModel::EntitySkipThroughTokens(index);
    if (std::holds_alternative<Decl>(entity)) {
      entity = std::get<Decl>(entity).canonical_declaration();
    }

    OnOpenEntity(QVariant::fromValue<VariantEntity>(entity));
  }
}

std::optional<NamedAction> CodeExplorer::ActOnKeyPress(
    IWindowManager *, const QKeySequence &keys, const QModelIndex &index) {

  VariantEntity entity;
  TriggerHandle *handle = nullptr;
  QString name;

  if (keys == kKeySeqP) {
    handle = &(d->open_user_preview_trigger);
    name = tr("Open Preview");
    entity = IModel::EntitySkipThroughTokens(index);

  } else if (keys == kKeySeqShiftP) {
    handle = &(d->open_pinned_preview_trigger);
    name = tr("Open Pinned Preview");
    entity = IModel::EntitySkipThroughTokens(index);

  } else if (keys == kKeySeqE) {
    handle = &(d->expand_macro_trigger);
    name = tr("Expand Macro");
    entity = EntityForExpansion(IModel::Entity(index));

  } else {
    return std::nullopt;
  }

  if (std::holds_alternative<NotAnEntity>(entity)) {
    return std::nullopt;
  }

  return NamedAction{name, *handle, QVariant::fromValue(entity)};
}

void CodeExplorer::ActOnContextMenu(IWindowManager *, QMenu *menu,
                                    const QModelIndex &index) {
  auto model_id = IModel::ModelId(index);
  if (model_id == CodePreviewWidget::kModelId ||
      model_id == kOpenEntityModelId) {
    
    auto sel_text = index.data(CodeWidget::SelectedTextRole).toString();
    if (!sel_text.isEmpty()) {
      auto copy_selection = new QAction(tr("Copy"), menu);
      menu->addAction(copy_selection);
      connect(copy_selection, &QAction::triggered,
              [sel_text = std::move(sel_text)] (void) {
                qApp->clipboard()->setText(sel_text);
              });
    }
  }

  auto exp_entity = EntityForExpansion(IModel::Entity(index));
  if (!std::holds_alternative<NotAnEntity>(exp_entity)) {
    auto expand_action = new QAction(tr("Expand Macro"), menu);
    menu->addAction(expand_action);
    connect(expand_action, &QAction::triggered,
            [exp_entity, action = d->expand_macro_trigger] (void) {
              action.Trigger(QVariant::fromValue(exp_entity));
            });
  }

  auto entity = IModel::EntitySkipThroughTokens(index);
  if (!std::holds_alternative<NotAnEntity>(entity)) {
    auto preview_action = new QAction(tr("Open Pinned Preview"), menu);
    menu->addAction(preview_action);
    connect(preview_action, &QAction::triggered,
            [entity, action = d->open_pinned_preview_trigger] (void) {
              action.Trigger(QVariant::fromValue(entity));
            });
  }
}

void CodeExplorer::OpenEntity(const VariantEntity &entity,
                              bool add_to_history) {
  auto file = File::containing(entity);
  auto frag = Fragment::containing(entity);
  if (!file && !frag) {
    return;
  }

  VariantEntity containing_entity;
  VariantEntity tooltip_entity = entity;
  if (file) {
    containing_entity = file.value();
    tooltip_entity = containing_entity;
  } else {
    containing_entity = frag.value();
  }

  const auto id = EntityId(containing_entity).Pack();
  auto tt = file ? TokenTree::from(file.value()) : TokenTree::from(frag.value());

  // If we're adding to history, then find the currently open window and
  // record its location.
  if (add_to_history) {
    d->history->CommitCurrentItemToHistory();
  }

  // Try to get the alread-opened code widget. If we have it, then we just
  // need to show it and go to the relevant entity.
  auto &entity_widget = d->opened_windows[id];
  if (entity_widget.second) {
    entity_widget.second->show();
    entity_widget.second->OnGoToEntity(entity, true  /* take focus */);
    return;
  }

  auto code_widget = new CodeWidget(
    d->config_manager, kOpenEntityModelId, d->browse_mode);

  entity_widget.first = entity;
  entity_widget.second = code_widget;

  connect(this, &CodeExplorer::ExpandMacros,
          code_widget, &CodeWidget::OnExpandMacros);

  // Figure out the window title.
  if (file) {
    for (auto path : file->paths()) {
      code_widget->setWindowTitle(QString::fromStdString(
          path.filename().generic_string()));
      break;
    }
  } else {
    auto found = false;
    for (auto tld : frag->top_level_declarations()) {
      if (auto name = NameOfEntityAsString(tld)) {
        code_widget->setWindowTitle(name.value());
        found = true;
        break;
      }
    }

    if (!found) {
      for (auto mt : frag->preprocessed_code()) {
        auto macro = std::get_if<Macro>(&mt);
        if (!macro) {
          continue;
        }
        if (auto name = NameOfEntityAsString(*macro)) {
          code_widget->setWindowTitle(name.value());
          break;
        }
      }
    }
  }

  code_widget->ChangeScene(tt, d->scene_options);

  connect(code_widget, &IWindowWidget::Closed,
          this, [id = id, this] (void) {
                  d->history->CommitCurrentItemToHistory();
                  d->opened_windows.erase(id);
                });

  connect(code_widget, &CodeWidget::LocationChanged,
          this, [=, this] (CodeWidget::LocationChangeReason reason) {
            switch (reason) {
              case CodeWidget::kExternalGoToEntity:
              case CodeWidget::kExternalSceneChange:
              case CodeWidget::kExternalMousePress:
              case CodeWidget::kExternalKeyPress:
              case CodeWidget::kExternalScrollChange:
              case CodeWidget::kExternalFocusChange:
                break;
              case CodeWidget::kInternalGoToSearchResult:
              case CodeWidget::kInternalGoToLine:
              case CodeWidget::kExternalSetOpaqueLocation:
                return;
            }
            d->history->SetCurrentItem(QVariant::fromValue(
                Location(containing_entity, code_widget->LastLocation())));
          });

  IWindowManager::CentralConfig config;
  if (auto name = NameOfEntityAsString(tooltip_entity)) {
    config.tooltip = name.value();
  }

  d->manager->AddCentralWidget(code_widget, config);

  // NOTE(pag): If we're not adding to history, then we're called from
  //            `OnGoToHistoricalItem` and it uses the `OpaqueLocation` to
  //            go to the relevant entity. We want to make sure we use that
  //            so that we don't trigger an external-looking location change.
  if (add_to_history) {
    code_widget->OnGoToEntity(entity, true  /* take focus */);
  }
}

void CodeExplorer::OnOpenEntity(const QVariant &data) {
  if (data.isNull() || !data.canConvert<VariantEntity>()) {
    return;
  }

  OpenEntity(data.value<VariantEntity>(), true);
}

void CodeExplorer::OnImplicitPreviewEntity(const QVariant &data) {
  OnPreviewEntity(data, false);
}

void CodeExplorer::OnExplicitPreviewEntity(const QVariant &data) {
  OnPreviewEntity(data, true);
}

void CodeExplorer::OnPreviewEntity(const QVariant &data, bool is_explicit) {
  if (data.isNull() || !data.canConvert<VariantEntity>()) {
    return;
  }

  VariantEntity entity = data.value<VariantEntity>();
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  if (!d->preview) {
    d->preview = new CodePreviewWidget(
        d->config_manager, d->scene_options, d->browse_mode, true);

    connect(this, &CodeExplorer::ExpandMacros,
            d->preview, &CodePreviewWidget::OnExpandMacros);

    // When the user navigates the history, make sure that we change what the
    // view shows.
    connect(d->preview, &CodePreviewWidget::HistoricalEntitySelected,
            this, &CodeExplorer::OnHistoricalPreviewedEntitySelected);
  
    IWindowManager::DockConfig config;
    config.id = "com.trailofbits.dock.CodePreview";
    config.location = IWindowManager::DockLocation::Bottom;
    config.app_menu_location = {tr("View")};
    d->manager->AddDockWidget(d->preview, config);
  }

  d->preview->DisplayEntity(
      std::move(entity), is_explicit, true  /* add to history */);
}

void CodeExplorer::OnGoToHistoricalItem(const QVariant &data) {
  auto [ent, loc] = data.value<Location>();
  OpenEntity(ent, false  /* don't add to history */);
  d->CurrentOpenCodeWidget().second->TryGoToLocation(
      loc, true  /* take focus */);
}

void CodeExplorer::OnHistoricalPreviewedEntitySelected(const QVariant &data) {
  d->preview->DisplayEntity(data.value<VariantEntity>(),
                            true  /* explicit request */,
                            false  /* add to history */);
}

void CodeExplorer::OnPinnedPreviewEntity(const QVariant &data) {
  if (data.isNull() || !data.canConvert<VariantEntity>()) {
    return;
  }

  VariantEntity entity = data.value<VariantEntity>();
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  auto preview = new CodePreviewWidget(
      d->config_manager, d->scene_options, d->browse_mode, false);

  connect(this, &CodeExplorer::ExpandMacros,
          preview, &CodePreviewWidget::OnExpandMacros);

  if (auto name = NameOfEntityAsString(entity)) {
    preview->setWindowTitle(
        tr("Preview of `%1`").arg(name.value()));
  }

  preview->DisplayEntity(
      std::move(entity), true  /* explicit request */,
      false  /* don't add to history */);

  IWindowManager::DockConfig config;
  config.id = "com.trailofbits.dock.PinnedCodePreview";
  config.location = IWindowManager::DockLocation::Right;
  config.delete_on_close = true;
  d->manager->AddDockWidget(preview, config);
}

void CodeExplorer::OnExpandMacro(const QVariant &data) {
  if (data.isNull() || !data.canConvert<VariantEntity>()) {
    return;
  }

  VariantEntity entity = data.value<VariantEntity>();
  if (!std::holds_alternative<Macro>(entity)) {
    return;
  }

  if (!d->macro_explorer) {
    d->macro_explorer_model = new ExpandedMacrosModel(d->config_manager, this);
    d->macro_explorer = new MacroExplorer(
        d->config_manager, d->macro_explorer_model);

    connect(d->macro_explorer_model, &ExpandedMacrosModel::ExpandMacros,
            this, &CodeExplorer::ExpandMacros);

    // Keep our shadow model of scene options in sync with the macro explorer.
    // The macro explorer actually does the real double-checking of things.
    connect(d->macro_explorer_model, &ExpandedMacrosModel::ExpandMacros,
            this, [this] (const QSet<RawEntityId> &macros_to_expand) {
              d->scene_options.macros_to_expand = macros_to_expand;
            });

    IWindowManager::DockConfig config;
    config.tabify = true;
    config.id = "com.trailofbits.dock.MacroExplorer";
    config.app_menu_location = {tr("View"), tr("Explorers")};
    d->manager->AddDockWidget(d->macro_explorer, config);
  }

  d->macro_explorer_model->AddMacro(std::move(std::get<Macro>(entity)));
}

void CodeExplorer::OnRenameEntity(QVector<RawEntityId> entity_ids,
                                  QString new_name) {
  (void) entity_ids;
  (void) new_name;
}

}  // namespace mx::gui
