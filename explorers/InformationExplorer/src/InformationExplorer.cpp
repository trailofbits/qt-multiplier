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

#include <multiplier/GUI/Assert.h>
#include <multiplier/GUI/HistoryWidget.h>
#include <multiplier/GUI/IGlobalHighlighter.h>

#include <QVBoxLayout>
#include <QToolBar>
#include <QHeaderView>

namespace mx::gui {

namespace {

const std::size_t kMaxHistorySize{30};

}  // namespace

struct InformationExplorer::PrivateData final {
  InformationExplorerModel *model{nullptr};
  QAbstractItemModel *top_model{nullptr};
  InformationExplorerTreeView *tree_view{nullptr};

  SortFilterProxyModel *model_proxy{nullptr};
  ISearchWidget *search_widget{nullptr};

  HistoryWidget *history_widget{nullptr};
  bool enable_history_updates{true};
};

InformationExplorer::~InformationExplorer() {}

InformationExplorer::InformationExplorer(InformationExplorerModel *model,
                                         QWidget *parent,
                                         IGlobalHighlighter *global_highlighter,
                                         const bool &enable_history)
    : QWidget(parent),
      d(new PrivateData) {

  InitializeWidgets(model, enable_history);
  InstallModel(model, global_highlighter);
}

void InformationExplorer::InitializeWidgets(InformationExplorerModel *model,
                                            const bool &enable_history) {
  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  if (enable_history) {
    auto toolbar = new QToolBar(this);
    layout->addWidget(toolbar);

    d->history_widget =
        new HistoryWidget(model->GetIndex(), model->GetFileLocationCache(),
                          kMaxHistorySize, this, false);

    toolbar->addWidget(d->history_widget);
    toolbar->setIconSize(QSize(16, 16));

    d->history_widget->SetIconSize(toolbar->iconSize());

    connect(d->history_widget, &HistoryWidget::GoToEntity, this,
            &InformationExplorer::OnHistoryNavigationEntitySelected);
  }

  d->tree_view = new InformationExplorerTreeView(this);
  d->tree_view->setHeaderHidden(true);
  d->tree_view->setAlternatingRowColors(false);
  d->tree_view->setSelectionBehavior(QAbstractItemView::SelectRows);
  d->tree_view->setSelectionMode(QAbstractItemView::SingleSelection);
  d->tree_view->setAllColumnsShowFocus(true);
  d->tree_view->setTreePosition(0);
  d->tree_view->setTextElideMode(Qt::TextElideMode::ElideMiddle);
  d->tree_view->header()->setStretchLastSection(true);
  layout->addWidget(d->tree_view);

  d->search_widget = ISearchWidget::Create(ISearchWidget::Mode::Filter, this);
  connect(d->search_widget, &ISearchWidget::SearchParametersChanged, this,
          &InformationExplorer::OnSearchParametersChange);

  layout->addWidget(d->search_widget);

  setContentsMargins(0, 0, 0, 0);
  setLayout(layout);

  connect(&ThemeManager::Get(), &ThemeManager::ThemeChanged, this,
          &InformationExplorer::OnThemeChange);
}

void InformationExplorer::InstallModel(InformationExplorerModel *model,
                                       IGlobalHighlighter *global_highlighter) {

  d->model = model;
  d->top_model = model;

  if (global_highlighter) {
    d->top_model = global_highlighter->CreateModelProxy(
        d->top_model, InformationExplorerModel::EntityIdRole);
  }

  d->model_proxy = new SortFilterProxyModel(this);
  d->model_proxy->setRecursiveFilteringEnabled(true);
  d->model_proxy->setSourceModel(d->top_model);
  d->top_model = d->model_proxy;

  d->tree_view->setModel(d->top_model);

  connect(d->top_model, &QAbstractItemModel::dataChanged, this,
          &InformationExplorer::OnHighlightModelDataChange);

  connect(d->top_model, &QAbstractItemModel::modelReset, this,
          &InformationExplorer::OnModelReset);

  connect(d->top_model, &QAbstractItemModel::rowsInserted, this,
          &InformationExplorer::OnRowsInserted);

  auto tree_selection_model = d->tree_view->selectionModel();
  connect(tree_selection_model, &QItemSelectionModel::currentChanged, this,
          &InformationExplorer::OnCurrentItemChanged);

  OnModelReset();
}

void InformationExplorer::OnModelReset() {
  ExpandAllNodes(QModelIndex());

  auto current_entity_id = d->model->GetCurrentEntityID();
  if (current_entity_id == kInvalidEntityId) {
    return;
  }

  if (d->history_widget != nullptr) {
    if (d->enable_history_updates) {
      d->history_widget->CommitCurrentLocationToHistory();
    } else {
      d->enable_history_updates = true;
    }

    d->history_widget->SetCurrentLocation(current_entity_id);
  }
}

void InformationExplorer::OnRowsInserted(const QModelIndex &parent, int, int) {
  ExpandAllNodes(parent);
}

void InformationExplorer::OnHighlightModelDataChange(const QModelIndex &,
                                                     const QModelIndex &,
                                                     const QList<int> &) {
  d->tree_view->viewport()->repaint();
}

void InformationExplorer::OnSearchParametersChange(
    const ISearchWidget::SearchParameters &search_parameters) {

  QRegularExpression::PatternOptions options{
      QRegularExpression::NoPatternOption};


}

void InformationExplorer::OnCurrentItemChanged(const QModelIndex &current_index,
                                               const QModelIndex &) {
  emit SelectedItemChanged(current_index);
}

void InformationExplorer::OnHistoryNavigationEntitySelected(
    RawEntityId original_id, RawEntityId) {

  d->enable_history_updates = false;
  d->model->RequestEntityInformation(original_id);
}

void InformationExplorer::ExpandAllNodes(const QModelIndex &parent) {
  std::vector<QModelIndex> next_queue;
  next_queue.emplace_back(parent);

  while (!next_queue.empty()) {
    auto queue = std::move(next_queue);
    next_queue.clear();

    for (const auto &index : queue) {
      if (!ShouldAutoExpand(index)) {
        continue;
      }

      d->tree_view->expand(index);

      auto row_count = d->top_model->rowCount(index);
      for (int row{}; row < row_count; ++row) {
        auto child_index = d->top_model->index(row, 0, index);
        next_queue.push_back(child_index);
      }
    }
  }

  d->tree_view->resizeColumnToContents(0);
}

void InformationExplorer::OnThemeChange(const QPalette &,
                                        const CodeViewTheme &code_view_theme) {
  QFont font{code_view_theme.font_name};
  setFont(font);
}

}  // namespace mx::gui
