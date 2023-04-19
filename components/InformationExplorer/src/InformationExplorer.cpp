
/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "InformationExplorer.h"
#include "InformationExplorerTreeView.h"
#include "Utils.h"
#include "SortFilterProxyModel.h"
#include "InformationExplorerModel.h"

#include <multiplier/ui/Assert.h>

#include <QVBoxLayout>

namespace mx::gui {

struct InformationExplorer::PrivateData final {
  IInformationExplorerModel *model{nullptr};
  InformationExplorerTreeView *tree_view{nullptr};

  SortFilterProxyModel *model_proxy{nullptr};
  ISearchWidget *search_widget{nullptr};
};

InformationExplorer::~InformationExplorer() {}

InformationExplorer::InformationExplorer(IInformationExplorerModel *model,
                                         QWidget *parent)
    : IInformationExplorer(parent),
      d(new PrivateData) {

  InitializeWidgets();
  InstallModel(model);
}

void InformationExplorer::InitializeWidgets() {
  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);

  d->tree_view = new InformationExplorerTreeView(this);
  d->tree_view->setHeaderHidden(true);
  d->tree_view->setAlternatingRowColors(false);
  d->tree_view->setSelectionBehavior(QAbstractItemView::SelectRows);
  d->tree_view->setSelectionMode(QAbstractItemView::SingleSelection);
  d->tree_view->setAllColumnsShowFocus(true);
  d->tree_view->setTreePosition(0);
  d->tree_view->setTextElideMode(Qt::TextElideMode::ElideMiddle);
  layout->addWidget(d->tree_view);

  d->search_widget = ISearchWidget::Create(ISearchWidget::Mode::Filter, this);
  connect(d->search_widget, &ISearchWidget::SearchParametersChanged, this,
          &InformationExplorer::OnSearchParametersChange);

  layout->addWidget(d->search_widget);

  setContentsMargins(0, 0, 0, 0);
  setLayout(layout);

  setEnabled(false);
}

void InformationExplorer::InstallModel(IInformationExplorerModel *model) {
  d->model = model;

  d->model_proxy = new SortFilterProxyModel(this);
  d->model_proxy->setRecursiveFilteringEnabled(true);
  d->model_proxy->setSourceModel(d->model);

  d->model_proxy->setSortRole(InformationExplorerModel::RawLocationRole);
  d->model_proxy->sort(0);
  d->model_proxy->setDynamicSortFilter(true);

  d->tree_view->setModel(d->model_proxy);

  connect(d->model, &QAbstractItemModel::modelReset, this,
          &InformationExplorer::OnModelReset);

  auto tree_selection_model = d->tree_view->selectionModel();
  connect(tree_selection_model, &QItemSelectionModel::currentChanged, this,
          &InformationExplorer::OnCurrentItemChanged);
}

void InformationExplorer::ExpandAllNodes() {
  std::vector<QModelIndex> next_queue{QModelIndex()};

  while (!next_queue.empty()) {
    auto queue = std::move(next_queue);
    next_queue.clear();

    for (const auto &index : queue) {
      if (!ShouldAutoExpand(index)) {
        continue;
      }

      d->tree_view->expand(index);

      auto row_count = d->model_proxy->rowCount(index);
      for (int row{}; row < row_count; ++row) {
        auto child_index = d->model_proxy->index(row, 0, index);
        next_queue.push_back(child_index);
      }
    }
  }
}

void InformationExplorer::OnModelReset() {
  auto enable = d->model->rowCount(QModelIndex()) != 0;
  setEnabled(enable);

  ExpandAllNodes();
}

void InformationExplorer::OnSearchParametersChange(
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
  ExpandAllNodes();
}

void InformationExplorer::OnCurrentItemChanged(const QModelIndex &current_index,
                                               const QModelIndex &) {
  emit SelectedItemChanged(current_index);
}

}  // namespace mx::gui
