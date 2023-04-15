
/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "InformationExplorer.h"

#include <multiplier/ui/Assert.h>

#include <QVBoxLayout>
#include <QTreeView>
#include <QSortFilterProxyModel>

namespace mx::gui {

struct InformationExplorer::PrivateData final {
  IInformationExplorerModel *model{nullptr};
  QTreeView *tree_view{nullptr};

  QSortFilterProxyModel *model_proxy{nullptr};
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

  d->tree_view = new QTreeView(this);
  d->tree_view->setHeaderHidden(true);
  d->tree_view->setAlternatingRowColors(true);
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

  d->model_proxy = new QSortFilterProxyModel(this);
  d->model_proxy->setRecursiveFilteringEnabled(true);
  d->model_proxy->setSourceModel(d->model);

  d->tree_view->setModel(d->model_proxy);

  connect(d->model, &QAbstractItemModel::modelReset, this,
          &InformationExplorer::OnModelReset);
}

void InformationExplorer::ExpandAllNodes() {
  d->tree_view->expandAll();
  d->tree_view->resizeColumnToContents(0);
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

}  // namespace mx::gui
