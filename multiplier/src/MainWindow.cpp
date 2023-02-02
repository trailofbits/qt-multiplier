// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MainWindow.h"

#include <multiplier/ui/IIndexView.h>
#include <multiplier/ui/ICodeView.h>

#include <QDockWidget>
#include <QTreeView>
#include <QFileDialog>
#include <QDir>
#include <QTabWidget>
#include <QMenu>
#include <QMenuBar>
#include <QSortFilterProxyModel>

namespace mx::gui {

struct MainWindow::PrivateData final {
  mx::Index index;
  mx::FileLocationCache file_loc_cache;

  IIndexView *index_view{nullptr};
  ICodeModel *code_model{nullptr};

  QMenu *view_menu{nullptr};
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
  d->view_menu = new QMenu(tr("View"));
  menuBar()->addMenu(d->view_menu);

  CreateFileTreeDock();
  CreateCodeView();
}

void MainWindow::CreateFileTreeDock() {
  auto file_tree_model = IFileTreeModel::Create(d->index, this);
  d->index_view = IIndexView::Create(file_tree_model, this);

  connect(d->index_view, &IIndexView::FileClicked, this,
          &MainWindow::OnIndexViewFileClicked);

  auto file_tree_dock = new QDockWidget(tr("File tree"), this);
  file_tree_dock->setAllowedAreas(Qt::LeftDockWidgetArea |
                                  Qt::RightDockWidgetArea);

  file_tree_dock->setWidget(d->index_view);

  addDockWidget(Qt::LeftDockWidgetArea, file_tree_dock);
}

void MainWindow::CreateCodeView() {
  auto tab_widget = new QTabWidget();
  setCentralWidget(tab_widget);

  d->code_model = ICodeModel::Create(d->file_loc_cache, d->index, this);
  auto code_view = ICodeView::Create(d->code_model);
  code_view->SetWordWrapping(false);

  tab_widget->addTab(code_view, tr("Empty"));

  auto toggle_word_wrap_action = new QAction(tr("Enable word wrap"));
  toggle_word_wrap_action->setCheckable(true);
  toggle_word_wrap_action->setChecked(false);

  connect(toggle_word_wrap_action, &QAction::triggered, this,
          &MainWindow::OnToggleWordWrap);

  d->view_menu->addAction(toggle_word_wrap_action);
}

void MainWindow::OnIndexViewFileClicked(const PackedFileId &file_id,
                                        const std::string &file_name,
                                        bool double_click) {

  if (double_click) {
    return;
  }

  auto &tab_widget = *static_cast<QTabWidget *>(centralWidget());
  tab_widget.setTabText(0, QString::fromStdString(file_name));

  d->code_model->SetFile(file_id);
}

void MainWindow::OnToggleWordWrap(bool checked) {
  auto &tab_widget = *static_cast<QTabWidget *>(centralWidget());
  auto &code_view = *static_cast<ICodeView *>(tab_widget.widget(0));
  code_view.SetWordWrapping(checked);
}

}  // namespace mx::gui
