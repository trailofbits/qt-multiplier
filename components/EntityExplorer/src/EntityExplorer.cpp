/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "EntityExplorer.h"
#include "EntityExplorerItemDelegate.h"

#include <multiplier/ui/Assert.h>

#include <QBrush>
#include <QCheckBox>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPalette>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>

namespace mx::gui {

struct EntityExplorer::PrivateData final {
  ISearchWidget *filter_widget{nullptr};
  QSortFilterProxyModel *model_proxy{nullptr};

  IEntityExplorerModel *model{nullptr};
  QListView *list_view{nullptr};

  QLineEdit *search_input{nullptr};
  QCheckBox *exact_search{nullptr};
};

EntityExplorer::~EntityExplorer() {}

IEntityExplorerModel *EntityExplorer::Model() {
  return d->model;
}

EntityExplorer::EntityExplorer(IEntityExplorerModel *model, QWidget *parent)
    : IEntityExplorer(parent),
      d(new PrivateData) {

  InitializeWidgets();
  InstallModel(model);
}

void EntityExplorer::InitializeWidgets() {
  setContentsMargins(0, 0, 0, 0);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);

  static const auto kRequestDarkTheme{true};
  CodeViewTheme theme = GetDefaultCodeViewTheme(kRequestDarkTheme);

  auto list_view_item_delegate = new EntityExplorerItemDelegate(theme, this);

  d->list_view = new QListView(this);
  d->list_view->setSelectionMode(
      QAbstractItemView::SelectionMode::SingleSelection);
  d->list_view->setSelectionBehavior(
      QAbstractItemView::SelectionBehavior::SelectRows);
  d->list_view->setItemDelegate(list_view_item_delegate);
  layout->addWidget(d->list_view);

  QPalette p = d->list_view->palette();
  auto changed_palette = false;
  if (theme.selected_line_background_color.isValid() &&
      theme.selected_line_background_color != theme.default_background_color) {
    p.setColor(QPalette::ColorGroup::Normal, QPalette::ColorRole::Highlight,
               theme.selected_line_background_color);
    changed_palette = true;
  }

  if (theme.default_background_color.isValid()) {
    p.setColor(QPalette::ColorGroup::Normal, QPalette::ColorRole::Base,
               theme.default_background_color);
    changed_palette = true;
  }

  if (changed_palette) {
    d->list_view->setPalette(p);
  }

  d->filter_widget = ISearchWidget::Create(ISearchWidget::Mode::Filter, this);
  connect(d->filter_widget, &ISearchWidget::SearchParametersChanged, this,
          &EntityExplorer::OnSearchParametersChange);

  layout->addWidget(d->filter_widget);

  auto search_parameters_layout = new QHBoxLayout();

  d->search_input = new QLineEdit(this);
  d->search_input->setClearButtonEnabled(true);
  d->search_input->setPlaceholderText(tr("Search"));

  connect(d->search_input, &QLineEdit::textChanged, this,
          &EntityExplorer::QueryParametersChanged);

  search_parameters_layout->addWidget(d->search_input);

  d->exact_search = new QCheckBox(tr("Exact"), this);

  connect(d->exact_search, &QCheckBox::stateChanged, this,
          &EntityExplorer::QueryParametersChanged);

  search_parameters_layout->addWidget(d->exact_search);

  layout->addLayout(search_parameters_layout);

  setLayout(layout);
}

void EntityExplorer::InstallModel(IEntityExplorerModel *model) {
  d->model = model;

  d->model_proxy = new QSortFilterProxyModel(this);
  d->model->setParent(d->model_proxy);

  d->model_proxy->setSourceModel(d->model);
  d->model_proxy->setRecursiveFilteringEnabled(false);
  d->model_proxy->setFilterRole(Qt::DisplayRole);

  d->list_view->setModel(d->model_proxy);

  // Note: this needs to happen after the model has been set in the
  // list view!
  auto list_selection_model = d->list_view->selectionModel();

  connect(list_selection_model, &QItemSelectionModel::currentChanged,
          this, &EntityExplorer::SelectionChanged);

  connect(d->model_proxy, &QAbstractListModel::modelReset, this,
          &EntityExplorer::OnModelReset);

  OnModelReset();
}

//! Try to open the token related to a specific model index.
void EntityExplorer::SelectionChanged(const QModelIndex &index,
                                      const QModelIndex &) {
  if (!index.isValid()) {
    qDebug() << "invalid index";
    return;
  }

  auto model_index = d->model_proxy->mapToSource(index);
  if (!model_index.isValid()) {
    qDebug() << "invalid mapped index";
    return;
  }

  QVariant token_var = model_index.data(IEntityExplorerModel::TokenRole);
  if (!token_var.isValid()) {
    qDebug() << "invalid tok";
    return;
  }

  emit EntityAction(qvariant_cast<Token>(token_var).id().Pack());
}

void EntityExplorer::OnModelReset() {}

void EntityExplorer::OnSearchParametersChange(
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
}

void EntityExplorer::QueryParametersChanged() {
  d->model->CancelSearch();

  if (d->search_input->text().isEmpty()) {
    return;
  }

  d->model->Search(d->search_input->text(), d->exact_search->isChecked());
}


}  // namespace mx::gui
