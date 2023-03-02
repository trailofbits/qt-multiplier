// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "PreviewableReferenceExplorer.h"

#include <multiplier/ui/ICodeView.h>
#include <multiplier/ui/IReferenceExplorer.h>

#include <QSplitter>
#include <QVBoxLayout>

#include <optional>

namespace mx::gui {

struct PreviewableReferenceExplorer::PrivateData final {
  ICodeModel *code_model{nullptr};
  ICodeView *code_view{nullptr};
  std::optional<unsigned> opt_scroll_to_line;

  IReferenceExplorer *reference_explorer{nullptr};

  QSplitter *splitter{nullptr};
};

PreviewableReferenceExplorer::PreviewableReferenceExplorer(
    mx::Index index, mx::FileLocationCache file_location_cache,
    IReferenceExplorerModel *model, QWidget *parent)
    : QWidget(parent),
      d(new PrivateData()) {

  InitializeWidgets(index, file_location_cache, model);
  UpdateLayout();
}

PreviewableReferenceExplorer::~PreviewableReferenceExplorer() {}

IReferenceExplorerModel *PreviewableReferenceExplorer::Model() {
  return d->reference_explorer->Model();
}

void PreviewableReferenceExplorer::resizeEvent(QResizeEvent *) {
  UpdateLayout();
}

void PreviewableReferenceExplorer::InitializeWidgets(
    mx::Index index, mx::FileLocationCache file_location_cache,
    IReferenceExplorerModel *model) {

  d->reference_explorer = IReferenceExplorer::Create(model, this);

  connect(d->reference_explorer, &IReferenceExplorer::ItemClicked, this,
          &PreviewableReferenceExplorer::OnReferenceExplorerItemClicked);

  connect(d->reference_explorer, &IReferenceExplorer::ItemClicked, this,
          &PreviewableReferenceExplorer::ItemClicked);

  d->code_model = ICodeModel::Create(file_location_cache, index, this);

  d->code_view = ICodeView::Create(d->code_model);
  d->code_view->hide();

  connect(d->code_view, &ICodeView::DocumentChanged, this,
          &PreviewableReferenceExplorer::OnCodeViewDocumentChange);

  d->splitter = new QSplitter(this);
  d->splitter->addWidget(d->reference_explorer);
  d->splitter->addWidget(d->code_view);

  setContentsMargins(0, 0, 0, 0);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->splitter);
  setLayout(layout);
}

void PreviewableReferenceExplorer::UpdateLayout() {
  auto character_length = fontMetrics().horizontalAdvance("_");
  auto minimum_size = character_length * 100;

  auto enable_code_view{false};
  if (width() >= minimum_size) {
    enable_code_view = true;
    d->splitter->setOrientation(Qt::Horizontal);

  } else if (height() >= minimum_size) {
    enable_code_view = true;
    d->splitter->setOrientation(Qt::Vertical);
  }

  if (enable_code_view) {
    enable_code_view = d->code_model->RowCount() != 0;
  }

  d->code_view->setVisible(enable_code_view);
}

void PreviewableReferenceExplorer::SchedulePostUpdateLineScrollCommand(
    unsigned line_number) {
  d->opt_scroll_to_line = line_number;
}

std::optional<unsigned>
PreviewableReferenceExplorer::GetScheduledPostUpdateLineScrollCommand() {
  auto opt_scroll_to_line = std::move(d->opt_scroll_to_line);
  d->opt_scroll_to_line = std::nullopt;

  return opt_scroll_to_line;
}

void PreviewableReferenceExplorer::OnReferenceExplorerItemClicked(
    const QModelIndex &index, const bool &middle_button) {

  if (middle_button) {
    return;
  }

  auto file_raw_entity_id_var =
      index.data(IReferenceExplorerModel::ReferencedEntityIdRole);

  if (!file_raw_entity_id_var.isValid()) {
    return;
  }

  auto line_number_var = index.data(IReferenceExplorerModel::LineNumberRole);
  if (!line_number_var.isValid()) {
    return;
  }

  auto line_number = qvariant_cast<unsigned>(line_number_var);
  SchedulePostUpdateLineScrollCommand(line_number);

  auto file_raw_entity_id = qvariant_cast<RawEntityId>(file_raw_entity_id_var);
  d->code_model->SetEntity(file_raw_entity_id);
}

void PreviewableReferenceExplorer::OnCodeViewDocumentChange() {
  if (auto opt_scroll_to_line = GetScheduledPostUpdateLineScrollCommand();
      opt_scroll_to_line.has_value()) {

    const auto &line_number = opt_scroll_to_line.value();
    d->code_view->ScrollToLineNumber(line_number);
  }

  UpdateLayout();
}

}  // namespace mx::gui
