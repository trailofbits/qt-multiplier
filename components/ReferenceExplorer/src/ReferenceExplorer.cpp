/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ReferenceExplorer.h"
#include "ReferenceExplorerItemDelegate.h"

#include <QTreeView>
#include <QVBoxLayout>
#include <QMenu>
#include <QApplication>
#include <QClipboard>

#include <iostream>

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
  QTreeView *tree_view{nullptr};
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
}

void ReferenceExplorer::InitializeWidgets() {
  setContentsMargins(0, 0, 0, 0);

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

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->tree_view);
  setLayout(layout);

  setAcceptDrops(true);

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

  d->tree_view->setModel(d->model);

  connect(d->model, &QAbstractItemModel::modelReset, this,
          &ReferenceExplorer::OnModelReset);

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
  d->model->ExpandEntity(index);
}

void ReferenceExplorer::OnModelReset() {
  d->tree_view->expandRecursively(QModelIndex());
  d->tree_view->resizeColumnToContents(0);
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

}  // namespace mx::gui
