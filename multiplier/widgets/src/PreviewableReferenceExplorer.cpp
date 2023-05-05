// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "PreviewableReferenceExplorer.h"
#include "CodePreviewModelAdapter.h"

#include <multiplier/Index.h>
#include <multiplier/ui/ICodeView.h>

#include <QSplitter>
#include <QVBoxLayout>
#include <QApplication>
#include <QScreen>

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
    const Index &index, const FileLocationCache &file_location_cache,
    IReferenceExplorerModel *model, const IReferenceExplorer::Mode &mode,
    IGlobalHighlighter &highlighter, QWidget *parent)
    : QWidget(parent),
      d(new PrivateData()) {

  InitializeWidgets(index, file_location_cache, model, mode, highlighter);
}

PreviewableReferenceExplorer::~PreviewableReferenceExplorer() {}

IReferenceExplorerModel *PreviewableReferenceExplorer::Model() {
  return d->reference_explorer->Model();
}

void PreviewableReferenceExplorer::InitializeWidgets(
    mx::Index index, mx::FileLocationCache file_location_cache,
    IReferenceExplorerModel *model, const IReferenceExplorer::Mode &mode,
    IGlobalHighlighter &highlighter) {

  d->reference_explorer = IReferenceExplorer::Create(model, mode, this);

  connect(
      d->reference_explorer, &IReferenceExplorer::SelectedItemChanged, this,
      &PreviewableReferenceExplorer::OnReferenceExplorerSelectedItemChanged);

  connect(d->reference_explorer, &IReferenceExplorer::ItemActivated, this,
          &PreviewableReferenceExplorer::ItemActivated);

  connect(model, &QAbstractItemModel::rowsInserted, this,
          &PreviewableReferenceExplorer::OnRowsInserted);

  d->code_model = new CodePreviewModelAdapter(
      ICodeModel::Create(file_location_cache, index), this);

  auto model_proxy =
      highlighter.CreateModelProxy(d->code_model, ICodeModel::TokenIdRole);

  d->code_view = ICodeView::Create(model_proxy);
  connect(d->code_view, &ICodeView::DocumentChanged, this,
          &PreviewableReferenceExplorer::OnCodeViewDocumentChange);

  connect(d->code_view, &ICodeView::TokenTriggered, this,
          &PreviewableReferenceExplorer::TokenTriggered);

  d->splitter = new QSplitter(Qt::Horizontal, this);
  d->splitter->addWidget(d->reference_explorer);
  d->splitter->addWidget(d->code_view);

  const auto &primary_screen = *QGuiApplication::primaryScreen();
  auto screen_width = primary_screen.virtualSize().width();
  d->splitter->setSizes({screen_width, 0});

  setContentsMargins(0, 0, 0, 0);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->splitter);
  setLayout(layout);
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

void PreviewableReferenceExplorer::UpdateCodePreview(const QModelIndex &index) {
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

void PreviewableReferenceExplorer::OnReferenceExplorerSelectedItemChanged(
    const QModelIndex &index) {

  UpdateCodePreview(index);

  if (d->code_view->visibleRegion().isEmpty()) {
    emit ItemActivated(index);
  }
}

void PreviewableReferenceExplorer::OnCodeViewDocumentChange() {
  if (auto opt_scroll_to_line = GetScheduledPostUpdateLineScrollCommand();
      opt_scroll_to_line.has_value()) {

    const auto &line_number = opt_scroll_to_line.value();
    d->code_view->ScrollToLineNumber(line_number);
  }
}

void PreviewableReferenceExplorer::OnRowsInserted() {
  if (!d->code_view->Text().isEmpty()) {
    return;
  }

  auto first_item_index = Model()->index(0, 0);
  UpdateCodePreview(first_item_index);
}

}  // namespace mx::gui
