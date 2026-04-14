// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "CodePreviewWidget.h"
#include "ExpandedMacrosModel.h"
#include "MacroExplorer.h"

#include <multiplier/GUI/Explorers/CodeExplorer.h>
#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Interfaces/IWindowManager.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Widgets/CodeWidget.h>
#include <multiplier/GUI/Widgets/HistoryWidget.h>
#include <multiplier/GUI/Util.h>

#include <multiplier/Frontend/MacroConcatenate.h>
#include <multiplier/Frontend/MacroExpansion.h>
#include <multiplier/Frontend/TokenTree.h>
#include <multiplier/Index.h>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDataStream>
#include <QIODevice>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QMimeData>
#include <QUndoCommand>
#include <QUndoGroup>
#include <QUndoStack>

#include <unordered_map>

Q_DECLARE_METATYPE(mx::TokenRange)

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
    auto token = std::get<Token>(entity);
    entity = token.related_entity();

    for (Macro macro : Macro::containing(token)) {

      // If we've found an `#include` directive, then keep going, because we
      // might find this include nested inside of a substitution because this
      // include is embedded in a fragment.
      //
      // We also skip concatenates, because we auto-expand them in `CodeWidget`
      // so that people don't need to expand through them.
      if (IncludeLikeMacroDirective::from(macro) ||
          MacroConcatenate::from(macro)) {
        continue;
      }

      auto exp = MacroExpansion::from(macro);
      if (!exp) {
        entity = macro;
        break;
      }

      auto def = exp->definition();
      if (!def) {
        entity = macro;
        break;
      }

      // If we've found the most likely expansion corresponding to the macro
      // referenced. It can happen [1] that the left corner of an expansion is
      // an expansion, and that the upper macro to be expanded is only revealed
      // by the lower macro.
      //
      // [1] Multiplier test: tests/Macros/DeferredExpansionFromConcatenation.c
      if (!std::holds_alternative<Macro>(entity) ||
          std::get<Macro>(entity) == def.value()) {
        entity = macro;
        break;
      }
    }
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

// Undoable command for expanding a macro.
class ExpandMacroCommand : public QUndoCommand {
 public:
  ExpandMacroCommand(ExpandedMacrosModel *model_, Macro macro_)
      : QUndoCommand(QObject::tr("Expand Macro")),
        model(model_), macro(std::move(macro_)) {}

  void redo(void) override { model->AddMacro(macro); }
  void undo(void) override { model->RemoveMacro(macro); }

 private:
  ExpandedMacrosModel *model;
  Macro macro;
};

// Undoable command for unexpanding a macro.
class UnexpandMacroCommand : public QUndoCommand {
 public:
  UnexpandMacroCommand(ExpandedMacrosModel *model_, Macro macro_)
      : QUndoCommand(QObject::tr("Unexpand Macro")),
        model(model_), macro(std::move(macro_)) {}

  void redo(void) override { model->RemoveMacro(macro); }
  void undo(void) override { model->AddMacro(macro); }

 private:
  ExpandedMacrosModel *model;
  Macro macro;
};

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
  QUndoStack *macro_undo_stack{nullptr};

  bool browse_mode{false};
  QAction *browse_mode_action{nullptr};

  inline PrivateData(ConfigManager &config_manager_,
                     IWindowManager *manager_)
      : config_manager(config_manager_),
        manager(manager_),
        history(new HistoryWidget(
            config_manager, kMaxHistorySize,
            true  /* install global shortcuts */,
            manager->Window())) {}

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

CodeExplorer::~CodeExplorer(void) {
  // Save preview history while ConfigManager is alive.
  if (d->preview) {
    d->preview->SaveHistory();
  }

  d->history->SaveToProject(
      QStringLiteral("CodeExplorer"),
      [] (const QVariant &item) -> RawEntityId {
        if (!item.canConvert<Location>()) return kInvalidEntityId;
        return EntityId(item.value<Location>().first).Pack();
      });

  // Save open entity IDs with cursor info.
  // The restore order determines tab order, so save in a
  // deterministic order (sorted by entity ID).
  std::vector<std::pair<RawEntityId, CodeWidget *>> sorted_windows;
  for (const auto &[id, pair] : d->opened_windows) {
    if (pair.second) {
      sorted_windows.emplace_back(id, pair.second);
    }
  }
  std::sort(sorted_windows.begin(), sorted_windows.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });

  QString ids;
  RawEntityId active_id = kInvalidEntityId;
  for (const auto &[id, widget] : sorted_windows) {
    if (!ids.isEmpty()) ids += QLatin1Char(';');
    auto loc = widget->LastLocation();
    ids += QString::number(static_cast<qint64>(id))
           + QLatin1Char(':')
           + QString::number(loc.scroll_y.relative)
           + QLatin1Char(':')
           + QString::number(loc.cursor_index);
    if (widget->isVisible()) {
      active_id = id;
    }
  }
  d->config_manager.SaveHeaderState(
      QStringLiteral("code_explorer_open_files"), ids.toUtf8());
  d->config_manager.SaveHeaderState(
      QStringLiteral("code_explorer_active_file"),
      QByteArray::number(static_cast<qint64>(active_id)));
}

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

  // Restore expanded macros from project settings.
  d->scene_options.macros_to_expand = config_manager.LoadExpandedMacros();

  d->macro_undo_stack = new QUndoStack(this);
  config_manager.UndoGroup().addStack(d->macro_undo_stack);

  // Create macro explorer eagerly (hidden) so it appears in View > Explorers
  // and restoreState can restore its visibility.
  d->macro_explorer_model = new ExpandedMacrosModel(d->config_manager, this);
  d->macro_explorer = new MacroExplorer(
      d->config_manager, d->macro_explorer_model);

  connect(d->macro_explorer_model, &ExpandedMacrosModel::ExpandMacros,
          this, &CodeExplorer::ExpandMacros);

  connect(d->macro_explorer_model, &ExpandedMacrosModel::ExpandMacros,
          this, [this] (const QSet<RawEntityId> &macros_to_expand) {
            d->scene_options.macros_to_expand = macros_to_expand;
            d->config_manager.SaveExpandedMacros(macros_to_expand);
          });

  connect(d->macro_explorer, &MacroExplorer::RequestRemoveMacro,
          this, [this] (Macro macro) {
    d->config_manager.UndoGroup().setActiveStack(d->macro_undo_stack);
    d->macro_undo_stack->push(new UnexpandMacroCommand(
        d->macro_explorer_model, std::move(macro)));
  });

  {
    IWindowManager::DockConfig mconfig;
    mconfig.tabify = true;
    mconfig.start_hidden = true;
    mconfig.id = "com.trailofbits.dock.MacroExplorer";
    mconfig.app_menu_location = {tr("View"), tr("Explorers")};
    parent->AddDockWidget(d->macro_explorer, mconfig);
  }

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

  d->browse_mode_action->setChecked(false);

  // When the user navigates the history, make sure that we change what the
  // view shows.
  connect(d->history, &HistoryWidget::GoToHistoricalItem,
          this, &CodeExplorer::OnGoToHistoricalItem);

  // Restore navigation history when a database is loaded.
  connect(&config_manager, &ConfigManager::IndexChanged,
          this, [this] (const ConfigManager &cm) {
    auto entries = cm.LoadNavigationHistory(QStringLiteral("CodeExplorer"));
    const auto &index = cm.Index();
    for (const auto &entry : entries) {
      VariantEntity entity = index.entity(EntityId(entry.entity_id));
      if (std::holds_alternative<NotAnEntity>(entity)) continue;

      CodeWidget::OpaqueLocation loc;
      d->history->SetCurrentItem(
          QVariant::fromValue(Location(entity, loc)),
          entry.label.isEmpty() ? std::nullopt
                                : std::optional<QString>(entry.label));
      d->history->CommitCurrentItemToHistory();
    }

    RestoreOpenFiles(cm);
  });

  // Also load for the initial index (SetIndex fires before constructor).
  RestoreOpenFiles(config_manager);
}

void CodeExplorer::RestoreOpenFiles(const ConfigManager &cm) {
  const auto &index = cm.Index();
  auto saved = cm.LoadHeaderState(
      QStringLiteral("code_explorer_open_files"));
  if (saved.isEmpty()) return;

  // Format: "eid:scrollYRel:cursorIdx;eid:scrollYRel:cursorIdx;..."
  struct PendingRestore {
    RawEntityId eid;
    int scroll_y_relative;
    int cursor_index;
  };
  QVector<PendingRestore> pending;

  for (const auto &entry : QString::fromUtf8(saved)
           .split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
    auto parts = entry.split(QLatin1Char(':'));
    if (parts.isEmpty()) continue;

    bool ok = false;
    auto eid = static_cast<RawEntityId>(parts[0].toLongLong(&ok));
    if (!ok || eid == kInvalidEntityId) continue;

    PendingRestore pr;
    pr.eid = eid;
    pr.scroll_y_relative = (parts.size() > 1) ? parts[1].toInt() : -1;
    pr.cursor_index = (parts.size() > 2) ? parts[2].toInt() : -1;
    pending.push_back(pr);
  }

  for (const auto &pr : pending) {
    VariantEntity entity = index.entity(EntityId(pr.eid));
    if (std::holds_alternative<NotAnEntity>(entity)) continue;

    OnOpenEntity(QVariant::fromValue(entity));

    // Try to restore cursor position.
    auto it = d->opened_windows.find(pr.eid);
    if (it != d->opened_windows.end() && it->second.second) {
      CodeWidget::OpaqueLocation loc;
      loc.scroll_y.relative = pr.scroll_y_relative;
      loc.cursor_index = pr.cursor_index;
      loc.entity = entity;
      it->second.second->TryGoToLocation(loc, false);
    }
  }

  // Restore the active tab.
  auto active_data = cm.LoadHeaderState(
      QStringLiteral("code_explorer_active_file"));
  if (!active_data.isEmpty()) {
    bool ok = false;
    auto active_eid = static_cast<RawEntityId>(
        QString::fromUtf8(active_data).toLongLong(&ok));
    if (ok && active_eid != kInvalidEntityId) {
      auto it = d->opened_windows.find(active_eid);
      if (it != d->opened_windows.end() && it->second.second) {
        it->second.second->EmitRequestAttention();
      }
    }
  }
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
      auto sel_tokens = index.data(CodeWidget::SelectedTokensRole);
      auto copy_selection = new QAction(tr("Copy"), menu);
      menu->addAction(copy_selection);
      connect(copy_selection, &QAction::triggered,
              [sel_text, sel_tokens] (void) {
                auto *mime = new QMimeData;
                mime->setText(sel_text);

                // Add token data for rich paste into spreadsheets.
                if (sel_tokens.isValid() && sel_tokens.canConvert<TokenRange>()) {
                  auto range = sel_tokens.value<TokenRange>();
                  if (!range.empty()) {
                    QByteArray data;
                    QDataStream stream(&data, QIODevice::WriteOnly);
                    auto count = static_cast<quint32>(range.size());
                    stream << count;
                    for (Token tok : range) {
                      stream << static_cast<quint64>(tok.id().Pack());
                      stream << static_cast<quint32>(tok.kind());
                      stream << static_cast<quint32>(tok.category());
                      auto td = tok.data();
                      stream << QString::fromUtf8(
                          td.data(), static_cast<qsizetype>(td.size()));
                      stream << static_cast<quint64>(
                          tok.related_entity_id().Pack());
                    }
                    mime->setData(QStringLiteral(
                        "application/x-qtmultiplier-tokens"), data);
                  }
                }

                qApp->clipboard()->setMimeData(mime);
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
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  auto preview_action = new QAction(tr("Open Pinned Preview"), menu);
  menu->addAction(preview_action);
  connect(preview_action, &QAction::triggered,
          [entity, action = d->open_pinned_preview_trigger] (void) {
            action.Trigger(QVariant::fromValue(entity));
          });
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
  auto tt = file ? TokenTree::create(file.value()) : TokenTree::create(frag.value());

  // If we're adding to history, then find the currently open window and
  // record its location.
  if (add_to_history) {
    d->history->CommitCurrentItemToHistory();
  }

  // Try to get the alread-opened code widget. If we have it, then we just
  // need to show it and go to the relevant entity.
  auto &entity_widget = d->opened_windows[id];
  if (entity_widget.second) {
    entity_widget.second->EmitRequestAttention();
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

  connect(code_widget, &QObject::destroyed,
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
    config.app_menu_location = {tr("View"), tr("Drawers")};
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

  d->config_manager.UndoGroup().setActiveStack(d->macro_undo_stack);
  d->macro_undo_stack->push(new ExpandMacroCommand(
      d->macro_explorer_model, std::move(std::get<Macro>(entity))));

  d->macro_explorer->show();
  d->macro_explorer->EmitRequestAttention();
}

void CodeExplorer::OnRenameEntity(QVector<RawEntityId> entity_ids,
                                  QString new_name) {
  (void) entity_ids;
  (void) new_name;
}

}  // namespace mx::gui
