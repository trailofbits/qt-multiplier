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
#include <QSplitter>
#include <QTabBar>

#include <iostream>

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
  ICodeModel *main_code_model{nullptr};
  CodeViewContextMenu code_view_context_menu;

  std::unique_ptr<QuickReferenceExplorer> quick_ref_explorer;

  ICodeModel *ref_explorer_code_model{nullptr};
  ICodeView *ref_explorer_code_view{nullptr};
  QTabWidget *ref_explorer_tab_widget{nullptr};
  QDockWidget *reference_explorer_dock{nullptr};

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

  d->view_menu->addAction(file_tree_dock->toggleViewAction());

  file_tree_dock->setWidget(d->index_view);

  addDockWidget(Qt::LeftDockWidgetArea, file_tree_dock);
}

void MainWindow::CreateReferenceExplorerDock() {
  d->ref_explorer_code_model =
      ICodeModel::Create(d->file_location_cache, d->index, this);

  d->ref_explorer_code_view = ICodeView::Create(d->ref_explorer_code_model);
  d->ref_explorer_code_view->SetWordWrapping(false);

  d->ref_explorer_tab_widget = new QTabWidget(this);
  d->ref_explorer_tab_widget->setTabsClosable(true);

  connect(d->ref_explorer_tab_widget->tabBar(), &QTabBar::tabCloseRequested,
          this, &MainWindow::OnReferenceExplorerTabBarClose);

  auto splitter = new QSplitter();
  splitter->setContentsMargins(0, 0, 0, 0);
  splitter->addWidget(d->ref_explorer_tab_widget);
  splitter->addWidget(d->ref_explorer_code_view);

  d->reference_explorer_dock = new QDockWidget(tr("Reference Explorer"), this);
  d->view_menu->addAction(d->reference_explorer_dock->toggleViewAction());
  d->reference_explorer_dock->toggleViewAction()->setEnabled(false);

  d->reference_explorer_dock->setAllowedAreas(
      Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea |
      Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);

  d->reference_explorer_dock->setWidget(splitter);
  addDockWidget(Qt::BottomDockWidgetArea, d->reference_explorer_dock);

  d->reference_explorer_dock->hide();
}

void MainWindow::CreateNewReferenceExplorer() {
  auto ref_explorer_model =
      IReferenceExplorerModel::Create(d->index, d->file_location_cache, this);

  auto ref_explorer = IReferenceExplorer::Create(ref_explorer_model, this);
  ref_explorer->setAttribute(Qt::WA_DeleteOnClose);

  connect(ref_explorer, &IReferenceExplorer::ItemClicked, this,
          &MainWindow::OnReferenceExplorerItemClicked);

  auto new_tab_index = d->ref_explorer_tab_widget->count();
  auto name = tr("Reference Explorer #") + QString::number(new_tab_index + 1);

  d->ref_explorer_tab_widget->addTab(ref_explorer, name);
  d->ref_explorer_tab_widget->setCurrentIndex(new_tab_index);

  d->reference_explorer_dock->toggleViewAction()->setEnabled(true);
  d->reference_explorer_dock->show();
}

void MainWindow::CreateCodeView() {
  auto tab_widget = new QTabWidget();
  setCentralWidget(tab_widget);

  d->main_code_model =
      ICodeModel::Create(d->file_location_cache, d->index, this);

  auto code_view = ICodeView::Create(d->main_code_model);
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

  // TODO(alessandro): Only show this when there is a related entity.
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
  auto related_entity_id_var =
      d->main_code_model->Data(index, ICodeModel::TokenRelatedEntityIdRole);

  if (!related_entity_id_var.isValid()) {
    return;
  }

  CloseTokenReferenceExplorer();

  d->quick_ref_explorer = std::make_unique<QuickReferenceExplorer>(
      d->index, d->file_location_cache,
      qvariant_cast<RawEntityId>(related_entity_id_var), this);

  connect(d->quick_ref_explorer.get(), &QuickReferenceExplorer::SaveAll, this,
          &MainWindow::OnQuickRefExplorerSaveAllClicked);

  auto dialog_pos = QCursor::pos();

  d->quick_ref_explorer->move(dialog_pos.x() - 20, dialog_pos.y() - 20);

  auto margin = fontMetrics().height();
  auto max_width = margin + (width() / 3);
  auto max_height = margin + (height() / 3);

  auto size_hint = d->quick_ref_explorer->sizeHint();
  auto width = std::min(max_width, size_hint.width());
  auto height = std::min(max_height, size_hint.height());

  d->quick_ref_explorer->resize(width, height);
  d->quick_ref_explorer->show();
}

void MainWindow::CloseTokenReferenceExplorer() {
  if (d->quick_ref_explorer != nullptr) {
    d->quick_ref_explorer->close();
    d->quick_ref_explorer.reset();
  }
}

void MainWindow::OnIndexViewFileClicked(const PackedFileId &file_id,
                                        const std::string &file_name,
                                        bool double_click) {

  if (double_click) {
    return;
  }

  auto &tab_widget = *static_cast<QTabWidget *>(centralWidget());
  tab_widget.setTabText(0, QString::fromStdString(file_name));

  d->main_code_model->SetEntity(file_id.Pack());
  CloseTokenReferenceExplorer();
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

void MainWindow::OnReferenceExplorerItemClicked(const QModelIndex &index) {
  auto file_raw_entity_id_var =
      index.data(IReferenceExplorerModel::EntityIdRole);

  if (!file_raw_entity_id_var.isValid()) {
    return;
  }

  auto file_raw_entity_id = qvariant_cast<RawEntityId>(file_raw_entity_id_var);
  d->ref_explorer_code_model->SetEntity(file_raw_entity_id);

  auto entity_id_var = index.data(IReferenceExplorerModel::EntityIdRole);
  if (!entity_id_var.isValid()) {
    return;
  }

  auto raw_entity_id = qvariant_cast<RawEntityId>(entity_id_var);
  d->ref_explorer_code_view->ScrollToEntityId(raw_entity_id);
}

void MainWindow::OnQuickRefExplorerSaveAllClicked(QMimeData *mime_data,
                                                  const bool &as_new_tab) {
  if (d->ref_explorer_tab_widget->count() == 0 || as_new_tab) {
    CreateNewReferenceExplorer();
  }

  auto current_tab = d->ref_explorer_tab_widget->currentIndex();
  auto reference_explorer = static_cast<IReferenceExplorer *>(
      d->ref_explorer_tab_widget->widget(current_tab));

  auto reference_explorer_model = reference_explorer->Model();
  reference_explorer_model->dropMimeData(mime_data, Qt::CopyAction, 0, 0,
                                         QModelIndex());
}

void MainWindow::OnReferenceExplorerTabBarClose(int index) {
  auto widget = d->ref_explorer_tab_widget->widget(index);
  d->ref_explorer_tab_widget->removeTab(index);

  widget->close();

  auto widget_visible = d->ref_explorer_tab_widget->count() != 0;
  d->reference_explorer_dock->setVisible(widget_visible);
  d->reference_explorer_dock->toggleViewAction()->setEnabled(widget_visible);
}

}  // namespace mx::gui
