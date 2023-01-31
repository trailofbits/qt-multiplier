/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "IndexView.h"

#include <QTreeView>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace mx::gui {

struct IndexView::PrivateData final {
  IFileTreeModel *model{nullptr};
  QTreeView *tree_view{nullptr};
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

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->tree_view);
  setLayout(layout);

  connect(d->tree_view, &QTreeView::clicked, this,
          &IndexView::OnFileTreeItemClicked);

  connect(d->tree_view, &QTreeView::doubleClicked, this,
          &IndexView::OnFileTreeItemDoubleClicked);
}

void IndexView::OnFileTreeItemActivated(const QModelIndex &index,
                                        bool double_click) {
  emit ItemClicked(index, double_click);

  auto opt_file_id = d->model->GetFileIdentifier(index);
  if (opt_file_id.has_value()) {
    const auto &file_id = opt_file_id.value();
    auto file_name_var = d->model->data(index);

    emit FileClicked(file_id, file_name_var.toString().toStdString(),
                     double_click);
  }
}

void IndexView::InstallModel(IFileTreeModel *model) {
  d->model = model;

  d->tree_view->setModel(d->model);
  d->tree_view->expandRecursively(QModelIndex());
  d->tree_view->resizeColumnToContents(0);
}

void IndexView::OnFileTreeItemClicked(const QModelIndex &index) {
  OnFileTreeItemActivated(index, false);
}

void IndexView::OnFileTreeItemDoubleClicked(const QModelIndex &index) {
  OnFileTreeItemActivated(index, true);
}

}  // namespace mx::gui
