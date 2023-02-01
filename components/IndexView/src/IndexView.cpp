/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "IndexView.h"

#include <multiplier/ui/Assert.h>

#include <QTreeView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSortFilterProxyModel>

namespace mx::gui {

struct IndexView::PrivateData final {
  IFileTreeModel *model{nullptr};
  QSortFilterProxyModel *model_proxy{nullptr};
  QTreeView *tree_view{nullptr};
  ISearchWidget *search_widget{nullptr};
};

IndexView::~IndexView() {}

IndexView::IndexView(IFileTreeModel *model, QWidget *parent)
    : IIndexView(parent),
      d(new PrivateData) {

  InitializeWidgets();
  InstallModel(model);
}

void IndexView::InitializeWidgets() {
  setContentsMargins(0, 0, 0, 0);

  d->tree_view = new QTreeView();
  d->tree_view->setHeaderHidden(true);
  d->tree_view->setSortingEnabled(true);
  d->tree_view->setAlternatingRowColors(true);

  auto indent_width = fontMetrics().horizontalAdvance("_");
  d->tree_view->setIndentation(indent_width);

  d->search_widget = ISearchWidget::Create(ISearchWidget::Mode::Filter, this);
  connect(d->search_widget, &ISearchWidget::SearchParametersChanged, this,
          &IndexView::OnSearchParametersChange);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->tree_view);
  layout->addWidget(d->search_widget);
  setLayout(layout);

  connect(d->tree_view, &QTreeView::clicked, this,
          &IndexView::OnFileTreeItemClicked);

  connect(d->tree_view, &QTreeView::doubleClicked, this,
          &IndexView::OnFileTreeItemDoubleClicked);
}

void IndexView::OnFileTreeItemActivated(const QModelIndex &index,
                                        bool double_click) {
  emit ItemClicked(index, double_click);

  auto opt_file_id_var =
      d->model_proxy->data(index, IFileTreeModel::OptionalPackedFileIdRole);
  if (!opt_file_id_var.isValid()) {
    return;
  }

  const auto &opt_file_id =
      qvariant_cast<std::optional<mx::PackedFileId>>(opt_file_id_var);

  if (!opt_file_id.has_value()) {
    return;
  }

  const auto &file_id = opt_file_id.value();

  auto file_name_var = d->model_proxy->data(index);
  emit FileClicked(file_id, file_name_var.toString().toStdString(),
                   double_click);
}

void IndexView::InstallModel(IFileTreeModel *model) {
  d->model = model;

  d->model_proxy = new QSortFilterProxyModel(this);
  d->model_proxy->setRecursiveFilteringEnabled(true);
  d->model_proxy->setSourceModel(d->model);

  d->tree_view->setModel(d->model_proxy);
  d->tree_view->expandRecursively(QModelIndex());
  d->tree_view->resizeColumnToContents(0);
}

void IndexView::OnFileTreeItemClicked(const QModelIndex &index) {
  OnFileTreeItemActivated(index, false);
}

void IndexView::OnFileTreeItemDoubleClicked(const QModelIndex &index) {
  OnFileTreeItemActivated(index, true);
}

void IndexView::OnSearchParametersChange(
    const ISearchWidget::SearchParameters &search_parameters) {

  QRegularExpression::PatternOptions options{
      QRegularExpression::NoPatternOption};

  if (!search_parameters.case_sensitive) {
    options |= QRegularExpression::CaseInsensitiveOption;
  }

  auto pattern = QString::fromStdString(search_parameters.pattern);

  if (search_parameters.type == ISearchWidget::SearchParameters::Type::Text) {
    pattern = QRegularExpression::escape(pattern);
    if (search_parameters.whole_word) {
      pattern = "\\b" + pattern + "\\b";
    }
  }

  QRegularExpression regex(pattern, options);

  // The regex is already validated by the search widget
  Assert(regex.isValid(),
         "Invalid regex found in CodeView::OnSearchParametersChange");

  d->model_proxy->setFilterRegularExpression(regex);

  d->tree_view->expandRecursively(QModelIndex());
  d->tree_view->resizeColumnToContents(0);
}

}  // namespace mx::gui
