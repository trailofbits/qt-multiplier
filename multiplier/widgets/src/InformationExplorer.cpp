/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "InformationExplorer.h"

#include <multiplier/Entities/MacroKind.h>
#include <multiplier/File.h>
#include <multiplier/Index.h>
#include <multiplier/Token.h>
#include <multiplier/ui/IDatabase.h>
#include <optional>
#include <QPushButton>
#include <QComboBox>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QString>
#include <QTreeView>
#include <vector>

namespace mx::gui {

struct InformationExplorer::PrivateData {
  const Index index;
  const FileLocationCache file_location_cache;

  IDatabase::Ptr database;
  QFuture<IDatabase::EntityInformationResult> future_result;
  QFutureWatcher<IDatabase::EntityInformationResult> future_watcher;

  //! Should we update the view when the entity on which the cursor is focused
  //! changes?
  bool update_on_focus_change{true};

  //! History of items.
  std::vector<EntityInformation> history;

  //! The size of what is in the history. If `current` is non-`nullptr`, then
  // `current == &(history[history_size - 1u])`. When we go backward through
  // history, we end up with `history_size < history.size()`.
  size_t history_size{0};

  // The currently shown entity information.
  EntityInformation *current{nullptr};

  std::unique_ptr<QComboBox> history_view;
  std::unique_ptr<QPushButton> prev_item;
  std::unique_ptr<QPushButton> next_item;
  std::unique_ptr<QTreeView> current_view;

  inline PrivateData(const Index &index_,
                     const FileLocationCache &file_location_cache_)
      : index(index_),
        file_location_cache(file_location_cache_),
        database(IDatabase::Create(index, file_location_cache)) {}

  //! Add `info` to history. This overwrites `current` to point and the new
  //! item within `history`.
  void AddToHistory(EntityInformation info);

  //! Cull some old history items. This keeps the size of the history
  //! reasonable.
  void CullOldHistory(void);

  //! Render the data from `current` into the view.
  void Render(void);

  bool TryReuseHistoricalInfo(RawEntityId entity_id);
};

void InformationExplorer::PrivateData::AddToHistory(EntityInformation info) {
  history.resize(history_size);
  current = &(history.emplace_back(std::move(info)));
  history_size = history.size();

  CullOldHistory();
}

bool InformationExplorer::PrivateData::TryReuseHistoricalInfo(
    RawEntityId entity_id) {
  if (history.empty()) {
    return false;
  }

  // The most recently backed away from history item was the thing we're
  // trying to show, so go to it.
  if (history_size < history.size() &&
      (history[history_size].requested_id == entity_id ||
       history[history_size].id == entity_id)) {
    ++history_size;
    current = &(history[history_size]);
    return true;
  }

  // Look for the data anywhere inside of the history.
  for (const EntityInformation &info : history) {
    if (info.requested_id == entity_id || info.id == entity_id) {
      EntityInformation clone = info;
      AddToHistory(std::move(clone));
      return true;
    }
  }

  return false;
}

InformationExplorer::~InformationExplorer(void) {
  CancelRunningRequest();
}

InformationExplorer::InformationExplorer(
    const Index &index, const FileLocationCache &file_location_cache,
    QWidget *parent)
    : QWidget(parent),
      d(new PrivateData(index, file_location_cache)) {

  connect(&d->future_watcher,
          &QFutureWatcher<IDatabase::EntityInformationResult>::finished,
          this, &InformationExplorer::FutureResultStateChanged);

  InitializeWidgets();
}

void InformationExplorer::PrivateData::CullOldHistory(void) {

}

void InformationExplorer::PrivateData::Render(void) {

}

void InformationExplorer::InitializeWidgets(void) {
  auto layout = new QGridLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  setLayout(layout);

  d->history_view = std::make_unique<QComboBox>(this);
  d->prev_item = std::make_unique<QPushButton>(this);
  d->next_item = std::make_unique<QPushButton>(this);
  d->current_view = std::make_unique<QTreeView>(this);

  layout->addWidget(d->history_view.get(), 0, 0, 1, 1);
  layout->addWidget(d->prev_item.get(), 0, 1, 1, 1);
  layout->addWidget(d->next_item.get(), 0, 2, 1, 1);
  layout->addWidget(d->current_view.get(), 1, 0, 1, 3);
  layout->setColumnStretch(0, 1);

  d->prev_item->setEnabled(false);
  d->next_item->setEnabled(false);
}

void InformationExplorer::CancelRunningRequest(void) {
  if (!d->future_result.isRunning()) {
    return;
  }

  d->future_result.cancel();
  d->future_result.waitForFinished();

  d->future_result = {};
}

bool InformationExplorer::AddEntityId(RawEntityId entity_id) {

  VariantId vid = EntityId(entity_id).Unpack();

  // The view isn't synchronizing itself to whatever the most recently clicked
  // entity is.
  if (!d->update_on_focus_change) {
    return false;
  }

  // This isn't a valid request for us. It's possible that these checks are
  // insufficient, as what we really care about are *named* declarations, and
  // there are a lot of them underneat `DeclId`. In practice, we expect
  // information requests to come from other clickable things, and other
  // clickable things are clickable because there is something (i.e. a name)
  // to click!
  if (!std::holds_alternative<DeclId>(vid) &&
      !std::holds_alternative<FileId>(vid) &&
      !(std::holds_alternative<MacroId>(vid) &&
        std::get<MacroId>(vid).kind == MacroKind::DEFINE_DIRECTIVE)) {
    return false;
  }

  CancelRunningRequest();

  // We're already showing the right thing.
  if (d->current && (d->current->requested_id == entity_id ||
                     d->current->id == entity_id)) {
    return true;
  }

  emit BeginResetModel();

  // The next history item in the future is what we're looking for; use it.
  if (d->TryReuseHistoricalInfo(entity_id)) {
    d->Render();
    emit EndResetModel(ModelState::Ready);
    return true;
  }

  d->future_result = d->database->RequestEntityInformation(entity_id);
  d->future_watcher.setFuture(d->future_result);
  return true;
}

void InformationExplorer::FutureResultStateChanged(void) {
  if (d->future_result.isCanceled()) {
    emit EndResetModel(ModelState::UpdateCancelled);
    return;
  }

  auto future_result = d->future_result.takeResult();
  if (!future_result.Succeeded()) {
    emit EndResetModel(ModelState::UpdateFailed);
    return;
  }

  EntityInformation info = future_result.TakeValue();

  if (!d->current || d->current->id != info.id) {
    d->AddToHistory(std::move(info));
    d->Render();
  }

  emit EndResetModel(ModelState::Ready);
}

}  // namespace mx::gui
