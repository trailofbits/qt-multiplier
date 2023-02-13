// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MainWindow.h"
#include "QuickReferenceExplorer.h"

#include <multiplier/ui/IIndexView.h>
#include <multiplier/ui/ICodeView.h>
#include <multiplier/ui/IReferenceExplorer.h>

#include <QDockWidget>
#include <QTreeView>
#include <QFileDialog>
#include <QDir>
#include <QTabWidget>
#include <QMenu>
#include <QMenuBar>
#include <QSortFilterProxyModel>
#include <QCursor>

namespace mx::gui {

namespace {

struct CodeViewContextMenu final {
  QMenu *menu{nullptr};
  QAction *show_ref_explorer_action{nullptr};
};

}  // namespace

struct MainWindow::PrivateData final {
  mx::Index index;
  mx::FileLocationCache file_location_cache;

  IIndexView *index_view{nullptr};
  ICodeModel *code_model{nullptr};
  CodeViewContextMenu code_view_context_menu;

  IReferenceExplorerModel *reference_explorer_model{nullptr};
  IReferenceExplorer *reference_explorer{nullptr};

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
  resize(1280, 800);
}

MainWindow::~MainWindow() {}

void MainWindow::InitializeWidgets() {
  d->view_menu = new QMenu(tr("View"));
  menuBar()->addMenu(d->view_menu);

  CreateFileTreeDock();
  CreateCodeView();
  CreateReferenceExplorerDock();
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

void MainWindow::CreateReferenceExplorerDock() {
  d->reference_explorer_model =
      IReferenceExplorerModel::Create(d->index, d->file_location_cache, this);
  auto reference_explorer =
      IReferenceExplorer::Create(d->reference_explorer_model, this);

  auto reference_explorer_dock =
      new QDockWidget(tr("Reference Explorer"), this);

  reference_explorer_dock->setAllowedAreas(
      Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea |
      Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);

  reference_explorer_dock->setWidget(reference_explorer);
  addDockWidget(Qt::BottomDockWidgetArea, reference_explorer_dock);
}

void MainWindow::CreateCodeView() {
  auto tab_widget = new QTabWidget();
  setCentralWidget(tab_widget);

  d->code_model = ICodeModel::Create(d->file_location_cache, d->index, this);
  auto code_view = ICodeView::Create(d->code_model);
  code_view->SetWordWrapping(false);

  tab_widget->addTab(code_view, tr("Empty"));

  connect(code_view, &ICodeView::TokenClicked, this,
          &MainWindow::OnTokenClicked);

  auto toggle_word_wrap_action = new QAction(tr("Enable word wrap"));
  toggle_word_wrap_action->setCheckable(true);
  toggle_word_wrap_action->setChecked(false);

  connect(toggle_word_wrap_action, &QAction::triggered, this,
          &MainWindow::OnToggleWordWrap);

  d->view_menu->addAction(toggle_word_wrap_action);

  // Also create the custom context menu
  d->code_view_context_menu.menu = new QMenu(tr("Token menu"));

  connect(d->code_view_context_menu.menu, &QMenu::triggered, this,
          &MainWindow::OnCodeViewContextMenuActionTriggered);

  d->code_view_context_menu.show_ref_explorer_action =
      new QAction(tr("Show Reference Explorer"));

  d->code_view_context_menu.menu->addAction(
      d->code_view_context_menu.show_ref_explorer_action);
}

void MainWindow::OpenTokenContextMenu(const CodeModelIndex &index) {
  QVariant action_data;
  action_data.setValue(index);

  for (auto &action : d->code_view_context_menu.menu->actions()) {
    action->setData(action_data);
  }

  d->code_view_context_menu.menu->exec(QCursor::pos());
}

void MainWindow::OpenTokenReferenceExplorer(const CodeModelIndex &index) {
  auto entity_id_var =
      d->code_model->Data(index, ICodeModel::TokenRawEntityIdRole);

  if (!entity_id_var.isValid()) {
    return;
  }

  auto entity_id = static_cast<RawEntityId>(entity_id_var.toULongLong());

  QuickReferenceExplorer quick_ref_explorer(d->index, d->file_location_cache,
                                            entity_id);

  auto dialog_pos = QCursor::pos();

  quick_ref_explorer.move(dialog_pos.x() - 20, dialog_pos.y() - 20);
  quick_ref_explorer.resize(width() / 3, height() / 3);
  quick_ref_explorer.exec();
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

void MainWindow::OnTokenClicked(const CodeModelIndex &index,
                                const Qt::MouseButton &mouse_button,
                                const Qt::KeyboardModifiers &modifiers,
                                bool double_click) {

  if (!double_click && modifiers == Qt::NoModifier &&
      mouse_button == Qt::RightButton) {
    OpenTokenContextMenu(index);
  }
}

void MainWindow::OnCodeViewContextMenuActionTriggered(QAction *action) {
  auto code_model_index_var = action->data();
  if (!code_model_index_var.isValid()) {
    return;
  }

  const auto &code_model_index =
      qvariant_cast<CodeModelIndex>(code_model_index_var);

  if (action == d->code_view_context_menu.show_ref_explorer_action) {
    OpenTokenReferenceExplorer(code_model_index);
  }
}

void MainWindow::OnToggleWordWrap(bool checked) {
  auto &tab_widget = *static_cast<QTabWidget *>(centralWidget());
  auto &code_view = *static_cast<ICodeView *>(tab_widget.widget(0));
  code_view.SetWordWrapping(checked);
}

}  // namespace mx::gui
