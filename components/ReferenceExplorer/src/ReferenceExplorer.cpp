/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ReferenceExplorer.h"
#include "ReferenceExplorerItemDelegate.h"
#include "FilterSettingsWidget.h"
#include "SearchFilterModelProxy.h"

#include <QTreeView>
#include <QVBoxLayout>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QLabel>
#include <QRadioButton>

#include <multiplier/ui/Assert.h>

namespace mx::gui {

namespace {

struct ContextMenu final {
  QMenu *menu{nullptr};
  QAction *copy_details_action{nullptr};
  QAction *expand_item_action{nullptr};
};

}  // namespace

struct ReferenceExplorer::PrivateData final {
  IReferenceExplorerModel *model{nullptr};
  SearchFilterModelProxy *model_proxy{nullptr};
  QTreeView *tree_view{nullptr};
  ISearchWidget *search_widget{nullptr};
  FilterSettingsWidget *filter_settings_widget{nullptr};
  ContextMenu context_menu;
};

ReferenceExplorer::~ReferenceExplorer() {}

IReferenceExplorerModel *ReferenceExplorer::Model() {
  return d->model;
}

ReferenceExplorer::ReferenceExplorer(IReferenceExplorerModel *model,
                                     QWidget *parent)
    : IReferenceExplorer(parent),
      d(new PrivateData) {

  InitializeWidgets();
  InstallModel(model);

  // Synchronize the search widget and its addon
  d->search_widget->Deactivate();
}

void ReferenceExplorer::InitializeWidgets() {
  // Initialize the tree view
  setAcceptDrops(true);

  d->tree_view = new QTreeView();
  d->tree_view->setHeaderHidden(true);
  d->tree_view->setSortingEnabled(true);
  d->tree_view->setAlternatingRowColors(true);
  d->tree_view->setItemDelegate(new ReferenceExplorerItemDelegate);
  d->tree_view->setSelectionMode(QAbstractItemView::SingleSelection);
  d->tree_view->setContextMenuPolicy(Qt::CustomContextMenu);

  connect(d->tree_view, &QTreeView::clicked, this,
          &ReferenceExplorer::OnItemLeftClick);

  connect(d->tree_view, &QTreeView::customContextMenuRequested, this,
          &ReferenceExplorer::OnOpenItemContextMenu);

  d->tree_view->setDragEnabled(true);
  d->tree_view->setAcceptDrops(true);
  d->tree_view->setDropIndicatorShown(true);

  // Create the search widget
  d->search_widget = ISearchWidget::Create(ISearchWidget::Mode::Filter, this);
  connect(d->search_widget, &ISearchWidget::SearchParametersChanged, this,
          &ReferenceExplorer::OnSearchParametersChange);

  // Create the search widget addon
  d->filter_settings_widget = new FilterSettingsWidget(this);

  connect(d->filter_settings_widget,
          &FilterSettingsWidget::FilterParametersChanged, this,
          &ReferenceExplorer::OnFilterParametersChange);

  connect(d->search_widget, &ISearchWidget::Activated,
          d->filter_settings_widget, &FilterSettingsWidget::Activate);

  connect(d->search_widget, &ISearchWidget::Deactivated,
          d->filter_settings_widget, &FilterSettingsWidget::Deactivate);

  // Setup the main layout
  setContentsMargins(0, 0, 0, 0);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->tree_view);
  layout->addWidget(d->filter_settings_widget);
  layout->addWidget(d->search_widget);
  setLayout(layout);

  // Setup che custom context menu
  d->context_menu.menu = new QMenu(tr("Reference Explorer menu"));
  d->context_menu.copy_details_action = new QAction(tr("Copy details"));
  d->context_menu.expand_item_action = new QAction(tr("Expand"));

  d->context_menu.menu->addAction(d->context_menu.copy_details_action);
  d->context_menu.menu->addAction(d->context_menu.expand_item_action);

  connect(d->context_menu.menu, &QMenu::triggered, this,
          &ReferenceExplorer::OnContextMenuActionTriggered);
}

void ReferenceExplorer::InstallModel(IReferenceExplorerModel *model) {
  d->model = model;
  d->model->setParent(this);

  d->model_proxy = new SearchFilterModelProxy(this);
  d->model_proxy->setRecursiveFilteringEnabled(true);
  d->model_proxy->setSourceModel(d->model);

  d->tree_view->setModel(d->model_proxy);

  connect(d->model_proxy, &QAbstractItemModel::modelReset, this,
          &ReferenceExplorer::OnModelReset);

  connect(d->model_proxy, &QAbstractItemModel::rowsInserted, this,
          &ReferenceExplorer::OnRowsInserted);

  OnModelReset();
}

void ReferenceExplorer::CopyRefExplorerItemDetails(const QModelIndex &index) {
  auto tooltip_var = index.data(Qt::ToolTipRole);
  if (!tooltip_var.isValid()) {
    return;
  }

  const auto &tooltip = tooltip_var.toString();

  auto &clipboard = *QGuiApplication::clipboard();
  clipboard.setText(tooltip);
}

void ReferenceExplorer::ExpandRefExplorerItem(const QModelIndex &index) {
  d->model->ExpandEntity(d->model_proxy->mapToSource(index));
}

void ReferenceExplorer::OnModelReset() {
  d->tree_view->expandRecursively(QModelIndex());
  d->tree_view->resizeColumnToContents(0);
}

void ReferenceExplorer::OnRowsInserted(const QModelIndex &, int, int) {
  // TODO: Switch to beginInsertRows/endInsertRows in order to preserve
  // the view state
  OnModelReset();
}

void ReferenceExplorer::OnItemLeftClick(const QModelIndex &index) {
  emit ItemClicked(index);
}

void ReferenceExplorer::OnOpenItemContextMenu(const QPoint &point) {
  auto index = d->tree_view->indexAt(point);
  if (!index.isValid()) {
    return;
  }

  QVariant action_data;
  action_data.setValue(index);

  for (auto &action : d->context_menu.menu->actions()) {
    action->setData(action_data);
  }

  auto menu_position = d->tree_view->viewport()->mapToGlobal(point);
  d->context_menu.menu->exec(menu_position);
}

void ReferenceExplorer::OnContextMenuActionTriggered(QAction *action) {
  auto index_var = action->data();
  if (!index_var.isValid()) {
    return;
  }

  const auto &index = qvariant_cast<QModelIndex>(index_var);
  if (!index.isValid()) {
    return;
  }

  if (action == d->context_menu.copy_details_action) {
    CopyRefExplorerItemDetails(index);

  } else if (action == d->context_menu.expand_item_action) {
    ExpandRefExplorerItem(index);
  }
}

void ReferenceExplorer::OnSearchParametersChange(
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

void ReferenceExplorer::OnFilterParametersChange() {
  d->model_proxy->SetPathFilterType(
      d->filter_settings_widget->GetPathFilterType());

  d->model_proxy->EnableEntityNameFilter(
      d->filter_settings_widget->FilterByEntityName());

  d->model_proxy->EnableEntityIDFilter(
      d->filter_settings_widget->FilterByEntityID());
}

}  // namespace mx::gui
