// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/ui/PreviewableTreeExplorerView.h>
#include <multiplier/ui/IGlobalHighlighter.h>
#include <multiplier/ui/IMacroExplorer.h>
#include <multiplier/ui/ITreeExplorerView.h>
#include <multiplier/ui/IGeneratorModel.h>

#include <multiplier/Index.h>

#include <QSplitter>
#include <QVBoxLayout>
#include <QApplication>
#include <QScreen>

#include <optional>

namespace mx::gui {

struct PreviewableTreeExplorerView::PrivateData final {
  ICodeModel *code_model{nullptr};
  ICodeView *code_view{nullptr};
  std::optional<unsigned> opt_scroll_to_line;

  IGeneratorModel *ref_explorer_model{nullptr};
  ITreeExplorerView *reference_explorer{nullptr};

  QSplitter *splitter{nullptr};
};

PreviewableTreeExplorerView::PreviewableTreeExplorerView(
    const Index &index, const FileLocationCache &file_location_cache,
    IGeneratorModel *model, const bool &show_code_preview,
    IGlobalHighlighter &highlighter, IMacroExplorer &macro_explorer,
    QWidget *parent)
    : QWidget(parent),
      d(new PrivateData()) {

  InitializeWidgets(index, file_location_cache, model, show_code_preview,
                    highlighter, macro_explorer);
}

PreviewableTreeExplorerView::~PreviewableTreeExplorerView() {}

IGeneratorModel *PreviewableTreeExplorerView::Model() {
  return d->ref_explorer_model;
}

void PreviewableTreeExplorerView::InitializeWidgets(
    mx::Index index, mx::FileLocationCache file_location_cache,
    IGeneratorModel *model, const bool &show_code_preview,
    IGlobalHighlighter &highlighter, IMacroExplorer &macro_explorer) {

  d->ref_explorer_model = model;
  d->reference_explorer =
      ITreeExplorerView::Create(d->ref_explorer_model, &highlighter, this);

  connect(d->reference_explorer, &ITreeExplorerView::SelectedItemChanged, this,
          &PreviewableTreeExplorerView::OnTreeExplorerSelectedItemChanged);

  connect(d->reference_explorer, &ITreeExplorerView::ItemActivated, this,
          &PreviewableTreeExplorerView::ItemActivated);

  connect(d->reference_explorer, &ITreeExplorerView::ExtractSubtree, this,
          &PreviewableTreeExplorerView::ExtractSubtree);

  connect(model, &QAbstractItemModel::rowsInserted, this,
          &PreviewableTreeExplorerView::OnRowsInserted);

  connect(model, &IGeneratorModel::TreeNameChanged, this,
          &PreviewableTreeExplorerView::OnTreeNameChanged);

  OnTreeNameChanged();

  d->code_model =
      macro_explorer.CreateCodeModel(file_location_cache, index, true);

  auto model_proxy = highlighter.CreateModelProxy(
      d->code_model, ICodeModel::RealRelatedEntityIdRole);

  d->code_view = ICodeView::Create(model_proxy, this);

  connect(d->code_view, &ICodeView::TokenTriggered, this,
          &PreviewableTreeExplorerView::TokenTriggered);

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

void PreviewableTreeExplorerView::SchedulePostUpdateLineScrollCommand(
    unsigned line_number) {
  d->opt_scroll_to_line = line_number;
}

std::optional<unsigned>
PreviewableTreeExplorerView::GetScheduledPostUpdateLineScrollCommand() {
  auto opt_scroll_to_line = std::move(d->opt_scroll_to_line);
  d->opt_scroll_to_line = std::nullopt;

  return opt_scroll_to_line;
}

void PreviewableTreeExplorerView::UpdateCodePreview(const QModelIndex &index) {
  auto file_raw_entity_id_var = index.data(IGeneratorModel::EntityIdRole);
  if (!file_raw_entity_id_var.isValid()) {
    return;
  }

  auto file_raw_entity_id = qvariant_cast<RawEntityId>(file_raw_entity_id_var);
  d->code_model->SetEntity(file_raw_entity_id);
}

void PreviewableTreeExplorerView::OnTreeExplorerSelectedItemChanged(
    const QModelIndex &index) {

  UpdateCodePreview(index);

  if (d->code_view->visibleRegion().isEmpty()) {
    emit ItemActivated(index);
  }
}

void PreviewableTreeExplorerView::OnRowsInserted() {
  if (!d->code_view->Text().isEmpty()) {
    return;
  }

  auto first_item_index = d->ref_explorer_model->index(0, 0);
  UpdateCodePreview(first_item_index);
}

void PreviewableTreeExplorerView::OnTreeNameChanged() {
  QString tree_name;
  if (auto tree_name_var = d->ref_explorer_model->data(
          QModelIndex(), IGeneratorModel::TreeNameRole);
      tree_name_var.canConvert<QString>()) {

    tree_name = tree_name_var.toString();
  }

  if (tree_name.isEmpty()) {
    tree_name = tr("Unnamed Tree");
  }

  setWindowTitle(tree_name);
}

void PreviewableTreeExplorerView::SetBrowserMode(const bool &enabled) {
  d->code_view->SetBrowserMode(enabled);
}

}  // namespace mx::gui
