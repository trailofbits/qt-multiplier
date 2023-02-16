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

namespace mx::gui {

struct ReferenceExplorer::PrivateData final {
  IReferenceExplorerModel *model{nullptr};
  QTreeView *tree_view{nullptr};
};

ReferenceExplorer::~ReferenceExplorer() {}

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

  connect(d->tree_view, &QTreeView::clicked, this,
          &ReferenceExplorer::OnItemClick);

  d->tree_view->setDragEnabled(true);
  d->tree_view->setAcceptDrops(true);
  d->tree_view->setDropIndicatorShown(true);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->tree_view);
  setLayout(layout);

  setAcceptDrops(true);
}

void ReferenceExplorer::InstallModel(IReferenceExplorerModel *model) {
  d->model = model;
  d->tree_view->setModel(d->model);

  connect(d->model, &QAbstractItemModel::modelReset, this,
          &ReferenceExplorer::OnModelReset);

  OnModelReset();
}

void ReferenceExplorer::OnModelReset() {
  d->tree_view->expandRecursively(QModelIndex());
  d->tree_view->resizeColumnToContents(0);
}

void ReferenceExplorer::OnItemClick(const QModelIndex &index) {
  emit ItemClicked(index);
}

}  // namespace mx::gui
