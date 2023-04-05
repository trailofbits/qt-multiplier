// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MainWindow.h"

#include <multiplier/Entities/StmtKind.h>
#include <multiplier/ui/IIndexView.h>
#include <multiplier/ui/IReferenceExplorer.h>
#include <multiplier/ui/IEntityExplorer.h>
#include <multiplier/ui/Util.h>

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

#include "InformationExplorer.h"
#include "PreviewableReferenceExplorer.h"
#include "QuickReferenceExplorer.h"
#include "SimpleTextInputDialog.h"

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
  IEntityExplorer *entity_explorer{nullptr};
  CodeViewContextMenu code_view_context_menu;

  std::unique_ptr<InformationExplorer> info_explorer;
  std::unique_ptr<QuickReferenceExplorer> quick_ref_explorer;

  QTabWidget *ref_explorer_tab_widget{nullptr};
  QDockWidget *reference_explorer_dock{nullptr};

  QDockWidget *project_explorer_dock{nullptr};
  QDockWidget *entity_explorer_dock{nullptr};
  QDockWidget *info_explorer_dock{nullptr};

  //! Tracks whether or not the information explorer has ever been opened. If
  //! it has not been opened, then we make it visible on the first request to
  //! open it. However, if the user has closed it then we only want to re-open
  //! it if it was closed.
  bool info_explorer_opened_before{false};

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

  setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::West);
  setTabPosition(Qt::RightDockWidgetArea, QTabWidget::East);
  setTabPosition(Qt::TopDockWidgetArea, QTabWidget::North);
  setTabPosition(Qt::BottomDockWidgetArea, QTabWidget::South);
  CreateProjectExplorerDock();
  CreateEntityExplorerDock();
  CreateInfoExplorerDock();
  CreateCodeView();
  CreateReferenceExplorerDock();

  tabifyDockWidget(d->entity_explorer_dock, d->project_explorer_dock);
  setDocumentMode(false);
}

void MainWindow::CreateProjectExplorerDock() {
  auto file_tree_model = IFileTreeModel::Create(d->index, this);
  d->index_view = IIndexView::Create(file_tree_model, this);

  connect(d->index_view, &IIndexView::FileClicked, this,
          &MainWindow::OnIndexViewFileClicked);

  d->project_explorer_dock = new QDockWidget(tr("Project Explorer"), this);
  d->project_explorer_dock->setAllowedAreas(Qt::AllDockWidgetAreas);
  d->view_menu->addAction(d->project_explorer_dock->toggleViewAction());
  d->project_explorer_dock->setWidget(d->index_view);
  addDockWidget(Qt::LeftDockWidgetArea, d->project_explorer_dock);
}

void MainWindow::CreateEntityExplorerDock() {
  auto entity_explorer_model =
      IEntityExplorerModel::Create(d->index, d->file_location_cache, this);
  d->entity_explorer = IEntityExplorer::Create(entity_explorer_model, this);

  d->entity_explorer_dock = new QDockWidget(tr("Entity Explorer"), this);
  d->entity_explorer_dock->setAllowedAreas(Qt::AllDockWidgetAreas);

  connect(d->entity_explorer, &IEntityExplorer::EntityAction,
          this, &MainWindow::OnEntityExplorerEntityClicked);

  d->view_menu->addAction(d->entity_explorer_dock->toggleViewAction());

  d->entity_explorer_dock->setWidget(d->entity_explorer);

  addDockWidget(Qt::LeftDockWidgetArea, d->entity_explorer_dock);
}

void MainWindow::CreateInfoExplorerDock() {
  d->info_explorer_dock = new QDockWidget(tr("Information Explorer"), this);
  d->view_menu->addAction(d->info_explorer_dock->toggleViewAction());
  d->info_explorer_dock->toggleViewAction()->setEnabled(false);
  d->info_explorer_dock->setAllowedAreas(Qt::AllDockWidgetAreas);
  d->view_menu->addAction(d->info_explorer_dock->toggleViewAction());

  d->info_explorer = std::make_unique<InformationExplorer>(
      d->index, d->file_location_cache, this);

  d->info_explorer_dock->setWidget(d->info_explorer.get());

  addDockWidget(Qt::LeftDockWidgetArea, d->info_explorer_dock);

  // Default is hidden until we click on an entity.
  d->info_explorer_dock->hide();
}

void MainWindow::CreateReferenceExplorerDock() {
  d->ref_explorer_tab_widget = new QTabWidget(this);
  d->ref_explorer_tab_widget->setDocumentMode(true);
  d->ref_explorer_tab_widget->setTabsClosable(true);

  connect(d->ref_explorer_tab_widget->tabBar(), &QTabBar::tabCloseRequested,
          this, &MainWindow::OnReferenceExplorerTabBarClose);

  connect(d->ref_explorer_tab_widget->tabBar(), &QTabBar::tabBarDoubleClicked,
          this, &MainWindow::OnReferenceExplorerTabBarDoubleClick);

  d->reference_explorer_dock = new QDockWidget(tr("Reference Explorer"), this);
  d->view_menu->addAction(d->reference_explorer_dock->toggleViewAction());
  d->reference_explorer_dock->toggleViewAction()->setEnabled(false);
  d->reference_explorer_dock->setAllowedAreas(Qt::AllDockWidgetAreas);
  d->reference_explorer_dock->setWidget(d->ref_explorer_tab_widget);
  addDockWidget(Qt::BottomDockWidgetArea, d->reference_explorer_dock);

  // Default is hidden until we ask to see the references to something.
  d->reference_explorer_dock->hide();
}

void MainWindow::CreateNewReferenceExplorer(QString window_title) {
  auto new_tab_index = d->ref_explorer_tab_widget->count();

  if (window_title.isEmpty()) {
    window_title =
        tr("Reference Explorer #") + QString::number(new_tab_index + 1);
  }

  auto ref_explorer_model =
      IReferenceExplorerModel::Create(d->index, d->file_location_cache, this);

  auto reference_explorer = new PreviewableReferenceExplorer(
      d->index, d->file_location_cache, ref_explorer_model, this);

  reference_explorer->setAttribute(Qt::WA_DeleteOnClose);

  connect(reference_explorer, &PreviewableReferenceExplorer::ItemActivated,
          this, &MainWindow::OnReferenceExplorerItemActivated);

  connect(reference_explorer, &PreviewableReferenceExplorer::TokenTriggered,
          this, &MainWindow::OnTokenTriggered);

  d->ref_explorer_tab_widget->addTab(reference_explorer, window_title);
  d->ref_explorer_tab_widget->setCurrentIndex(new_tab_index);

  d->reference_explorer_dock->toggleViewAction()->setEnabled(true);
  d->reference_explorer_dock->show();
}

void MainWindow::CreateCodeView() {
  auto tab_widget = new QTabWidget();
  tab_widget->setTabsClosable(true);
  tab_widget->setDocumentMode(true);
  tab_widget->setTabBarAutoHide(false);

  setCentralWidget(tab_widget);

  connect(tab_widget->tabBar(), &QTabBar::tabCloseRequested,
          this, &MainWindow::OnCodeViewTabBarClose);

  auto toggle_word_wrap_action = new QAction(tr("Enable word wrap"));
  toggle_word_wrap_action->setCheckable(true);
  toggle_word_wrap_action->setChecked(false);

  connect(toggle_word_wrap_action, &QAction::triggered,
          this, &MainWindow::OnToggleWordWrap);

  d->view_menu->addAction(toggle_word_wrap_action);

  // Also create the custom context menu
  d->code_view_context_menu.menu = new QMenu(tr("Token menu"));

  // TODO(alessandro): Only show this when there is a related entity.
  connect(d->code_view_context_menu.menu, &QMenu::triggered,
          this, &MainWindow::OnCodeViewContextMenuActionTriggered);

  d->code_view_context_menu.show_ref_explorer_action =
      new QAction(tr("Show Reference Explorer"));

  d->code_view_context_menu.menu->addAction(
      d->code_view_context_menu.show_ref_explorer_action);
}

void MainWindow::OpenTokenContextMenu(CodeModelIndex index) {
  QVariant action_data;
  action_data.setValue(index);

  for (auto &action : d->code_view_context_menu.menu->actions()) {
    action->setData(action_data);
  }

  QVariant related_entity_id_var =
      index.model->Data(index, ICodeModel::TokenRelatedEntityIdRole);

  // Only enable the references browser if the token is related to an entity.
  d->code_view_context_menu.show_ref_explorer_action->setEnabled(
      related_entity_id_var.isValid());

  d->code_view_context_menu.menu->exec(QCursor::pos());
}

void MainWindow::OpenReferenceExplorer(
    RawEntityId entity_id, IReferenceExplorerModel::ExpansionMode mode) {
  CloseTokenReferenceExplorer();

  d->quick_ref_explorer = std::make_unique<QuickReferenceExplorer>(
      d->index, d->file_location_cache, entity_id, mode, this);

  connect(d->quick_ref_explorer.get(), &QuickReferenceExplorer::SaveAll,
          this, &MainWindow::OnQuickRefExplorerSaveAllClicked);

  connect(d->quick_ref_explorer.get(), &QuickReferenceExplorer::ItemActivated,
          this, &MainWindow::OnReferenceExplorerItemActivated);

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

void MainWindow::OpenTokenReferenceExplorer(CodeModelIndex index) {

  QVariant related_entity_id_var =
      index.model->Data(index, ICodeModel::TokenRelatedEntityIdRole);

  if (!related_entity_id_var.isValid()) {
    CloseTokenReferenceExplorer();
    return;
  }

  OpenReferenceExplorer(qvariant_cast<RawEntityId>(related_entity_id_var),
                        IReferenceExplorerModel::CallHierarchyMode);
}

void MainWindow::OpenTokenTaintExplorer(CodeModelIndex index) {

  QVariant related_stmt_id_var =
      index.model->Data(index, ICodeModel::EntityIdOfStmtContainingTokenRole);

  // If we clicked on a statement, then if it's a decl statement, it could be
  // of the form `int a = 1, b = 2;` and the taint tracker doesn't handle that
  // as well. But if there is a single associated declaration then it is fine
  // with it usually.
  if (related_stmt_id_var.isValid()) {
    OpenReferenceExplorer(qvariant_cast<RawEntityId>(related_stmt_id_var),
                          IReferenceExplorerModel::TaintMode);
    return;
  }

  QVariant related_entity_id_var =
      index.model->Data(index, ICodeModel::TokenRelatedEntityIdRole);

  if (related_entity_id_var.isValid()) {
    OpenReferenceExplorer(qvariant_cast<RawEntityId>(related_entity_id_var),
                          IReferenceExplorerModel::TaintMode);
    return;
  }

  CloseTokenReferenceExplorer();
}

void MainWindow::OpenTokenEntityInfo(CodeModelIndex index) {

  QVariant related_entity_id_var =
      index.model->Data(index, ICodeModel::TokenRelatedEntityIdRole);

  if (!related_entity_id_var.isValid()) {
    return;
  }

  OpenEntityInfo(qvariant_cast<RawEntityId>(related_entity_id_var),
                 true  /* force */);
}

void MainWindow::CloseTokenReferenceExplorer() {
  if (d->quick_ref_explorer != nullptr) {
    d->quick_ref_explorer->close();
    d->quick_ref_explorer.reset();
  }
}

ICodeView *MainWindow::CreateNewCodeView(RawEntityId file_entity_id,
                                         QString tab_name) {
  auto model = ICodeModel::Create(d->file_location_cache, d->index, this);
  auto code_view = ICodeView::Create(model);

  code_view->SetWordWrapping(false);
  code_view->setAttribute(Qt::WA_DeleteOnClose);

  auto &central_tab_widget = *static_cast<QTabWidget *>(centralWidget());
  central_tab_widget.addTab(code_view, tab_name);

  auto tab_count = central_tab_widget.count();
  central_tab_widget.setCurrentIndex(tab_count - 1);

  connect(code_view, &ICodeView::TokenTriggered,
          this, &MainWindow::OnTokenTriggered);

  model->SetEntity(file_entity_id);
  return code_view;
}

ICodeView *
MainWindow::GetOrCreateFileCodeView(RawEntityId file_id,
                                    std::optional<QString> opt_tab_name) {

  OpenEntityInfo(file_id);

  ICodeView *tab_code_view = nullptr;
  ICodeModel *tab_model = nullptr;
  QTabWidget &tab_widget = *static_cast<QTabWidget *>(centralWidget());

  for (auto i = 0; i < tab_widget.count(); ++i) {
    tab_code_view = dynamic_cast<ICodeView *>(tab_widget.widget(i));
    if (!tab_code_view) {
      continue;
    }

    tab_model = tab_code_view->Model();
    if (!tab_model) {
      continue;
    }

    std::optional<RawEntityId> tab_file_id = tab_model->GetEntity();
    if (!tab_file_id.has_value() || tab_file_id.value() != file_id) {
      continue;
    }

    tab_widget.setCurrentWidget(tab_code_view);
    return tab_code_view;
  }

  if (opt_tab_name.has_value()) {
    return CreateNewCodeView(file_id, opt_tab_name.value());
  }

  for (auto [path, id] : d->index.file_paths()) {
    if (id.Pack() != file_id) {
      continue;
    }

    return CreateNewCodeView(
        file_id, QString::fromStdString(path.filename().generic_string()));
  }

  return nullptr;
}

void MainWindow::OpenEntityRelatedToToken(const CodeModelIndex &index) {
  auto entity_id_var =
      index.model->Data(index, ICodeModel::TokenRelatedEntityIdRole);

  if (!entity_id_var.isValid()) {
    return;
  }

  auto entity_id = qvariant_cast<RawEntityId>(entity_id_var);
  OpenEntityInfo(entity_id);
  OpenEntityCode(entity_id);
}

void MainWindow::OpenEntityInfo(RawEntityId entity_id, bool force) {
  auto make_visible = !d->info_explorer_opened_before ||
                      d->info_explorer_dock->isVisible() ||
                      force;

  if (make_visible && d->info_explorer->AddEntityId(entity_id)) {
    d->info_explorer_dock->toggleViewAction()->setEnabled(true);
    d->info_explorer_dock->show();
    d->info_explorer_opened_before = true;
  }
}

void MainWindow::OpenEntityCode(RawEntityId entity_id) {
  // TODO(pag): Make this fetch the entity via a QFuture or something like that.
  VariantEntity entity = d->index.entity(entity_id);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  if (std::holds_alternative<Decl>(entity)) {
    entity = std::get<Decl>(entity).canonical_declaration();
  }

  std::optional<File> opt_file = FileOfEntity(entity);
  if (!opt_file.has_value()) {
    return;
  }

  ICodeView *code_view = GetOrCreateFileCodeView(opt_file->id().Pack());
  if (!code_view) {
    return;
  }

  ICodeModel *code_model = code_view->Model();
  if (!code_model) {
    return;
  }

  if (Token opt_tok = FirstFileToken(entity)) {
    const FileLocationCache loc_cache = code_model->GetFileLocationCache();
    auto maybe_loc = opt_tok.location(loc_cache);
    if (!maybe_loc.has_value()) {
      return;
    }

    auto [line, col] = maybe_loc.value();
    code_view->ScrollToLineNumber(line);
  }
}

void MainWindow::OnIndexViewFileClicked(RawEntityId file_id, QString tab_name,
                                        Qt::KeyboardModifiers,
                                        Qt::MouseButtons) {
  CloseTokenReferenceExplorer();
  static_cast<void>(GetOrCreateFileCodeView(file_id, tab_name));
}

void MainWindow::OnTokenTriggered(const ICodeView::TokenAction &token_action,
                                  const CodeModelIndex &index) {

  if (token_action.type == ICodeView::TokenAction::Type::Primary) {
    OpenEntityRelatedToToken(index);

  } else if (token_action.type == ICodeView::TokenAction::Type::Secondary) {
    OpenTokenContextMenu(index);

  } else if (token_action.type == ICodeView::TokenAction::Type::Keyboard) {
    // Here to test keyboard events; Before we add more buttons, we should
    // find a better strategy to managed them.
    //
    // Ideally we should find a Qt-friendly method that the framework handles
    // well natively
    const auto &keyboard_button = token_action.opt_keyboard_button.value();

    // Like in IDA Pro, pressing X while the cursor is on an entity shows us
    // its cross-references.
    if (keyboard_button.key == Qt::Key_X && !keyboard_button.shift_modifier &&
        !keyboard_button.control_modifier) {
      OpenTokenReferenceExplorer(index);
    }

    if (keyboard_button.key == Qt::Key_T && !keyboard_button.shift_modifier &&
        !keyboard_button.control_modifier) {
      OpenTokenTaintExplorer(index);
    }

    if (keyboard_button.key == Qt::Key_I && !keyboard_button.shift_modifier &&
        !keyboard_button.control_modifier) {
      OpenTokenEntityInfo(index);
    }

    // Like in IDA Pro, pressing Enter while the cursor is on a use of that
    // entity will bring us to that entity.
    if (keyboard_button.key == Qt::Key_Enter &&
        !keyboard_button.shift_modifier && !keyboard_button.control_modifier) {
      OpenEntityRelatedToToken(index);
    }
  }
}

void MainWindow::OnEntityExplorerEntityClicked(RawEntityId entity_id) {
  OpenEntityCode(entity_id);
}

void MainWindow::OnReferenceExplorerItemActivated(const QModelIndex &index) {
  auto entity_id_role =
      index.data(IReferenceExplorerModel::ReferencedEntityIdRole);
  if (!entity_id_role.isValid()) {
    entity_id_role = index.data(IReferenceExplorerModel::EntityIdRole);
    if (!entity_id_role.isValid()) {
      return;
    }
  }

  auto entity_id = qvariant_cast<RawEntityId>(entity_id_role);
  OpenEntityCode(entity_id);
}

void MainWindow::OnCodeViewContextMenuActionTriggered(QAction *action) {
  QVariant code_model_index_var = action->data();
  if (!code_model_index_var.isValid()) {
    return;
  }

  CodeModelIndex code_model_index =
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

void MainWindow::OnQuickRefExplorerSaveAllClicked(QMimeData *mime_data,
                                                  const QString &window_title,
                                                  const bool &as_new_tab) {
  if (d->ref_explorer_tab_widget->count() == 0 || as_new_tab) {
    CreateNewReferenceExplorer(window_title);
  }

  auto current_tab = d->ref_explorer_tab_widget->currentIndex();
  auto reference_explorer = static_cast<PreviewableReferenceExplorer *>(
      d->ref_explorer_tab_widget->widget(current_tab));

  auto reference_explorer_model = reference_explorer->Model();
  reference_explorer_model->dropMimeData(mime_data, Qt::CopyAction, -1, 0,
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

void MainWindow::OnReferenceExplorerTabBarDoubleClick(int index) {
  auto current_tab_name = d->ref_explorer_tab_widget->tabText(index);

  SimpleTextInputDialog dialog(tr("Insert the new tab name"), current_tab_name,
                               this);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  const auto &opt_tab_name = dialog.GetTextInput();

  QString new_tab_name;
  if (opt_tab_name.has_value()) {
    new_tab_name = opt_tab_name.value();
  } else {
    new_tab_name = tr("Reference browser #") + QString::number(index);
  }

  d->ref_explorer_tab_widget->setTabText(index, new_tab_name);
}

void MainWindow::OnCodeViewTabBarClose(int index) {
  auto &central_tab_widget = *static_cast<QTabWidget *>(centralWidget());

  auto widget = central_tab_widget.widget(index);
  central_tab_widget.removeTab(index);

  widget->close();
}

}  // namespace mx::gui
