// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "CodeWidget.h"

#include <multiplier/ui/IDatabase.h>
#include <multiplier/ui/Icons.h>
#include <multiplier/ui/Util.h>

#include <QVBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QPushButton>
#include <QFutureWatcher>
#include <QSizeGrip>

#include <optional>

namespace mx::gui {

struct CodeWidget::PrivateData final {
  IDatabase::Ptr database;

  ICodeModel *model{nullptr};
  ICodeView *code_view{nullptr};

  QFuture<VariantEntity> entity_future;
  QFutureWatcher<VariantEntity> entity_future_watcher;
};

CodeWidget::CodeWidget(const Index &index,
                       const FileLocationCache &file_location_cache,
                       RawEntityId entity_id, IGlobalHighlighter &highlighter,
                       IMacroExplorer &macro_explorer, QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  d->database = IDatabase::Create(index, file_location_cache);

  connect(&d->entity_future_watcher,
          &QFutureWatcher<QFuture<VariantEntity>>::finished, this,
          &CodeWidget::OnEntityRequestFutureStatusChanged);

  InitializeWidgets(index, file_location_cache, entity_id, highlighter,
                    macro_explorer);
}

CodeWidget::~CodeWidget() {}

void CodeWidget::InitializeWidgets(const Index &index,
                                   const FileLocationCache &file_location_cache,
                                   RawEntityId entity_id,
                                   IGlobalHighlighter &highlighter,
                                   IMacroExplorer &macro_explorer) {

  // Use a temporary window name at first. This won't be shown at all if the
  // name resolution is fast enough
  auto window_name = tr("Entity ID #") + QString::number(entity_id);
  setWindowTitle(window_name);

  // Start a request to fetch the canonical entity.
  d->entity_future = d->database->RequestCanonicalEntity(entity_id);
  d->entity_future_watcher.setFuture(d->entity_future);

  d->model =
      macro_explorer.CreateCodeModel(file_location_cache, index, true, this);

  QAbstractItemModel *model_proxy = highlighter.CreateModelProxy(
      d->model, ICodeModel::RealRelatedEntityIdRole);

  d->code_view = ICodeView::Create(model_proxy, this);
  d->code_view->SetWordWrapping(true);

  connect(d->code_view, &ICodeView::TokenTriggered, this,
          &CodeWidget::OnTokenTriggered);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->code_view);
  setLayout(layout);
}

void CodeWidget::OnEntityRequestFutureStatusChanged() {
  if (d->entity_future.isCanceled()) {
    return;
  }

  VariantEntity ent = d->entity_future.takeResult();
  if (std::holds_alternative<NotAnEntity>(ent)) {
    return;
  }

  // Set the name.
  if (std::optional<QString> opt_entity_name = NameOfEntityAsString(ent)) {
    auto window_title =
        tr("Preview for") + " `" + opt_entity_name.value() + "`";

    setWindowTitle(window_title);
  }

  // Set the contents.
  EntityId eid(ent);
  d->model->SetEntity(eid.Pack());
}

void CodeWidget::OnTokenTriggered(const ICodeView::TokenAction &token_action,
                                  const QModelIndex &index) {

  switch (token_action.type) {
    case ICodeView::TokenAction::Type::Keyboard:
    case ICodeView::TokenAction::Type::Primary:
    case ICodeView::TokenAction::Type::Secondary:
      emit TokenTriggered(token_action, index);
      break;

    case ICodeView::TokenAction::Type::Hover: break;
  }
}

void CodeWidget::SetBrowserMode(const bool &enabled) {
  d->code_view->SetBrowserMode(enabled);
}

}  // namespace mx::gui
