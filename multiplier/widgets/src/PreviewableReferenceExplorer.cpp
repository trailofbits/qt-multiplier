// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "PreviewableReferenceExplorer.h"

#include <multiplier/Index.h>
#include <multiplier/ui/IGlobalHighlighter.h>
#include <multiplier/ui/IMacroExplorer.h>
#include <multiplier/ui/ITreeExplorer.h>
#include <multiplier/ui/ITreeExplorerModel.h>

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
  ITreeExplorer *reference_explorer{nullptr};
  QSplitter *splitter{nullptr};
};

PreviewableReferenceExplorer::PreviewableReferenceExplorer(
    const Index &index, const FileLocationCache &file_location_cache,
    ITreeExplorerModel *model, const bool &show_code_preview,
    IGlobalHighlighter &highlighter, IMacroExplorer &macro_explorer,
    QWidget *parent)
    : QWidget(parent),
      d(new PrivateData()) {

  InitializeWidgets(index, file_location_cache, model, show_code_preview,
                    highlighter, macro_explorer);
}

PreviewableReferenceExplorer::~PreviewableReferenceExplorer() {}

ITreeExplorerModel *PreviewableReferenceExplorer::Model() {
  return d->reference_explorer->Model();
}

void PreviewableReferenceExplorer::InitializeWidgets(
    mx::Index index, mx::FileLocationCache file_location_cache,
    ITreeExplorerModel *model, const bool &show_code_preview,
    IGlobalHighlighter &highlighter, IMacroExplorer &macro_explorer) {

  d->reference_explorer = ITreeExplorer::Create(model, this, &highlighter);

  connect(
      d->reference_explorer, &ITreeExplorer::SelectedItemChanged, this,
      &PreviewableReferenceExplorer::OnReferenceExplorerSelectedItemChanged);

  connect(d->reference_explorer, &ITreeExplorer::ItemActivated, this,
          &PreviewableReferenceExplorer::ItemActivated);

  connect(d->reference_explorer, &ITreeExplorer::ExtractSubtree, this,
          &PreviewableReferenceExplorer::ExtractSubtree);

  connect(model, &QAbstractItemModel::rowsInserted, this,
          &PreviewableReferenceExplorer::OnRowsInserted);

  connect(model, &ITreeExplorerModel::TreeNameChanged, this,
          &PreviewableReferenceExplorer::OnTreeNameChanged);

  OnTreeNameChanged();

  d->code_model =
      macro_explorer.CreateCodeModel(file_location_cache, index, true);

  auto model_proxy = highlighter.CreateModelProxy(
      d->code_model, ICodeModel::RealRelatedEntityIdRole);

  d->code_view = ICodeView::Create(model_proxy, this);

  connect(d->code_view, &ICodeView::TokenTriggered, this,
          &PreviewableReferenceExplorer::TokenTriggered);

  d->splitter = new QSplitter(Qt::Horizontal, this);
  d->splitter->setHandleWidth(6);
  d->splitter->addWidget(d->reference_explorer);
  d->splitter->addWidget(d->code_view);

  if (!show_code_preview) {
    const auto &primary_screen = *QGuiApplication::primaryScreen();
    auto screen_width = primary_screen.virtualSize().width();
    d->splitter->setSizes({screen_width, 0});
  }

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
  auto file_raw_entity_id_var = index.data(ITreeExplorerModel::EntityIdRole);
  if (!file_raw_entity_id_var.isValid()) {
    return;
  }

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

void PreviewableReferenceExplorer::OnRowsInserted() {
  if (!d->code_view->Text().isEmpty()) {
    return;
  }

  auto first_item_index = Model()->index(0, 0);
  UpdateCodePreview(first_item_index);
}


void PreviewableReferenceExplorer::OnTreeNameChanged() {
  auto model = d->reference_explorer->Model();

  QString tree_name;
  if (auto tree_name_var =
          model->data(QModelIndex(), ITreeExplorerModel::TreeNameRole);
      tree_name_var.canConvert<QString>()) {

    tree_name = tree_name_var.toString();
  }

  if (tree_name.isEmpty()) {
    tree_name = tr("Unnamed Tree");
  }

  setWindowTitle(tree_name);
}

void PreviewableReferenceExplorer::SetBrowserMode(const bool &enabled) {
  d->code_view->SetBrowserMode(enabled);
}

}  // namespace mx::gui
