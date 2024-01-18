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
#include <multiplier/GUI/Widgets/CodeWidget.h>
#include <multiplier/GUI/Util.h>
#include <multiplier/Frontend/TokenTree.h>
#include <multiplier/Index.h>
#include <unordered_map>

#include "CodePreviewWidget.h"
#include "ExpandedMacrosModel.h"
#include "MacroExplorer.h"

namespace mx::gui {
namespace {

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
      break;
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

}  // namespace

struct CodeExplorer::PrivateData {
  ConfigManager &config_manager;
  IWindowManager * const manager;
  CodePreviewWidget *preview{nullptr};

  std::unordered_map<RawEntityId, CodeWidget *> opened_windows;

  TriggerHandle expand_macro_trigger;
  TriggerHandle open_preview_trigger;
  TriggerHandle open_pinned_preview_trigger;

  ExpandedMacrosModel *macro_explorer_model{nullptr};
  MacroExplorer *macro_explorer{nullptr};

  inline PrivateData(ConfigManager &config_manager_,
                     IWindowManager *manager_)
      : config_manager(config_manager_),
        manager(manager_) {}
};

CodeExplorer::~CodeExplorer(void) {}

CodeExplorer::CodeExplorer(ConfigManager &config_manager,
                           IWindowManager *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager, parent)) {

  config_manager.ActionManager().Register(
      this, "com.trailofbits.action.OpenEntity",
      &CodeExplorer::OnOpenEntity);

  d->expand_macro_trigger = config_manager.ActionManager().Register(
      this, "com.trailofbits.action.ExpandMacro",
      &CodeExplorer::OnExpandMacro);

  d->open_preview_trigger = config_manager.ActionManager().Register(
      this, "com.trailofbits.action.OpenEntityPreview",
      &CodeExplorer::OnPreviewEntity);

  d->open_pinned_preview_trigger = config_manager.ActionManager().Register(
      this, "com.trailofbits.action.OpenPinnedEntityPreview",
      &CodeExplorer::OnPinnedPreviewEntity);
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

    OnOpenEntity(QVariant::fromValue<VariantEntity>(
        IModel::EntitySkipThroughTokens(index)));
  }
}

std::optional<NamedAction> CodeExplorer::ActOnKeyPress(
    IWindowManager *, const QKeySequence &keys, const QModelIndex &index) {

  VariantEntity entity;
  TriggerHandle *handle = nullptr;
  QString name;

  if (keys == kKeySeqP) {
    handle = &(d->open_preview_trigger);
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
    
    auto sel_text = index.data(CodeWidget::SelectedTextUserRole).toString();
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

void CodeExplorer::OnOpenEntity(const QVariant &data) {
  if (data.isNull() || !data.canConvert<VariantEntity>()) {
    return;
  }

  VariantEntity entity = data.value<VariantEntity>();
  auto file = File::containing(entity);
  auto frag = Fragment::containing(entity);
  if (!file && !frag) {
    return;
  }

  auto [id, tt] = ([&] (void) -> std::pair<RawEntityId, TokenTree> {
    if (file) {
      return {file->id().Pack(), TokenTree::from(file.value())};
    } else {
      return {frag->id().Pack(), TokenTree::from(frag.value())};
    }
  })();

  auto &code_widget = d->opened_windows[id];
  if (code_widget) {
    code_widget->show();
    code_widget->OnGoToEntity(entity);
    return;
  }

  code_widget = new CodeWidget(d->config_manager, kOpenEntityModelId);

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

  code_widget->SetTokenTree(tt);
  code_widget->OnGoToEntity(entity);

  connect(code_widget, &IWindowWidget::Closed,
          this, [=, this] (void) {
                  d->opened_windows.erase(id);
                });

  IWindowManager::CentralConfig config;
  d->manager->AddCentralWidget(code_widget, config);
}

void CodeExplorer::OnPreviewEntity(const QVariant &data) {
  if (data.isNull() || !data.canConvert<VariantEntity>()) {
    return;
  }

  VariantEntity entity = data.value<VariantEntity>();
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  if (!d->preview) {
    d->preview = new CodePreviewWidget(d->config_manager, true);

    connect(this, &CodeExplorer::ExpandMacros,
            d->preview, &CodePreviewWidget::OnExpandMacros);

    // When the user navigates the history, make sure that we change what the
    // view shows.
    connect(d->preview, &CodePreviewWidget::HistoricalEntitySelected,
            d->preview, [this] (VariantEntity entity) {
                          d->preview->DisplayEntity(
                              std::move(entity), true  /* explicit request */,
                              false  /* add to history */);
                        });
  
    IWindowManager::DockConfig config;
    config.id = "com.trailofbits.dock.CodePreview";
    config.location = IWindowManager::DockLocation::Bottom;
    config.app_menu_location = {tr("View")};
    d->manager->AddDockWidget(d->preview, config);
  }

  d->preview->DisplayEntity(
      std::move(entity), false  /* implicit (click) request */,
      true  /* add to history */);
}

void CodeExplorer::OnPinnedPreviewEntity(const QVariant &data) {
  if (data.isNull() || !data.canConvert<VariantEntity>()) {
    return;
  }

  VariantEntity entity = data.value<VariantEntity>();
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  auto preview = new CodePreviewWidget(d->config_manager, false);

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
