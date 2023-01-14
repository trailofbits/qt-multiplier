// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MainWindow.h"

#include <multiplier/ui/IFileTreeModel.h>
#include <multiplier/ui/ICodeModel.h>
#include <multiplier/ui/ICodeView.h>

#include <multiplier/Index.h>

#include <QDockWidget>
#include <QTreeView>
#include <QFileDialog>
#include <QDir>
#include <QTabWidget>

namespace mx::gui {

struct MainWindow::PrivateData final {
  mx::Index index;
  IFileTreeModel *file_tree_model{nullptr};

  mx::FileLocationCache file_loc_cache;
  ICodeModel *code_model{nullptr};
};

MainWindow::MainWindow() : QMainWindow(nullptr), d(new PrivateData) {
  setWindowTitle("Multiplier");
  setWindowIcon(QIcon(":/Icons/Multiplier"));

  auto database_path = QFileDialog::getOpenFileName(
      this, tr("Select a Multiplier database"), QDir::homePath());
  d->index = mx::EntityProvider::in_memory_cache(
      mx::EntityProvider::from_database(database_path.toStdString()));

  InitializeWidgets();
}

MainWindow::~MainWindow() {}

void MainWindow::InitializeWidgets() {
  CreateFileTreeDock();
  CreateCodeView();
}

void MainWindow::CreateFileTreeDock() {
  d->file_tree_model = IFileTreeModel::Create(d->index, this);

  auto tree_view = new QTreeView();
  tree_view->setModel(d->file_tree_model);
  tree_view->expandRecursively(QModelIndex());
  tree_view->setHeaderHidden(true);
  tree_view->resizeColumnToContents(0);

  auto file_tree_dock = new QDockWidget(tr("File tree"), this);
  file_tree_dock->setAllowedAreas(Qt::LeftDockWidgetArea |
                                  Qt::RightDockWidgetArea);
  file_tree_dock->setWidget(tree_view);

  addDockWidget(Qt::LeftDockWidgetArea, file_tree_dock);

  connect(tree_view, &QTreeView::clicked, this,
          &MainWindow::OnFileTreeItemClicked);
}

void MainWindow::CreateCodeView() {
  auto tab_widget = new QTabWidget();
  setCentralWidget(tab_widget);

  d->code_model = ICodeModel::Create(d->file_loc_cache, d->index, this);
  auto code_view = ICodeView::Create(d->code_model);

  tab_widget->addTab(code_view, tr("Empty"));
}

void MainWindow::OnFileTreeItemClicked(const QModelIndex &index) {
  auto opt_file_identifier = d->file_tree_model->GetFileIdentifier(index);
  if (!opt_file_identifier.has_value()) {
    return;
  }

  auto file_name_var = d->file_tree_model->data(index);
  if (!file_name_var.isValid()) {
    file_name_var = tr("Unnamed file");
  }

  auto &tab_widget = *static_cast<QTabWidget *>(centralWidget());
  tab_widget.setTabText(0, file_name_var.toString());

  const auto &file_identifier = opt_file_identifier.value();
  d->code_model->SetFile(file_identifier);
}

}  // namespace mx::gui
