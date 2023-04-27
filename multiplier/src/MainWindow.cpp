// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MainWindow.h"
#include "PreviewableReferenceExplorer.h"
#include "QuickReferenceExplorer.h"
#include "SimpleTextInputDialog.h"
#include "QuickCodeView.h"

#include <multiplier/ui/Assert.h>
#include <multiplier/ui/HistoryWidget.h>
#include <multiplier/ui/IDatabase.h>
#include <multiplier/ui/IIndexView.h>
#include <multiplier/ui/IReferenceExplorer.h>
#include <multiplier/ui/IEntityExplorer.h>
#include <multiplier/ui/Util.h>
#include <multiplier/ui/IInformationExplorer.h>

#include <multiplier/Entities/StmtKind.h>

#include <QDockWidget>
#include <QFileDialog>
#include <QDir>
#include <QTabWidget>
#include <QMenu>
#include <QCursor>
#include <QTabBar>
#include <QToolBar>
#include <QMenuBar>
#include <QToolButton>
#include <QShortcut>

namespace mx::gui {

namespace {

const std::size_t kMaxHistorySize{30};

//! The file id associated with a code view.
static const char *kFileIdProperty = "mx:fileId";

//! The last cursor location in a code view.
static const char *kLastLocationProperty = "mx:lastLocation";

struct CodeViewContextMenu final {
  QMenu *menu{nullptr};
  QAction *show_ref_explorer_action{nullptr};
};

struct ToolBar final {
  HistoryWidget *back_forward{nullptr};
};

}  // namespace

struct MainWindow::PrivateData final {
  mx::Index index;
  mx::FileLocationCache file_location_cache;

  IDatabase::Ptr database;

  IIndexView *index_view{nullptr};
  IEntityExplorer *entity_explorer{nullptr};
  CodeViewContextMenu code_view_context_menu;

  std::unique_ptr<QuickReferenceExplorer> quick_ref_explorer;

  QAction *enable_code_preview_action{nullptr};
  std::unique_ptr<QuickCodeView> quick_code_view;

  QShortcut *close_active_ref_explorer_tab_shortcut{nullptr};
  QShortcut *close_active_code_tab_shortcut{nullptr};

  IReferenceExplorer::Mode ref_explorer_mode{
      IReferenceExplorer::Mode::TextView};
  QTabWidget *ref_explorer_tab_widget{nullptr};
  QDockWidget *reference_explorer_dock{nullptr};

  QDockWidget *project_explorer_dock{nullptr};
  QDockWidget *entity_explorer_dock{nullptr};
  QDockWidget *info_explorer_dock{nullptr};
  IInformationExplorerModel *info_explorer_model{nullptr};

  QMenu *view_menu{nullptr};
  ToolBar toolbar;
};

MainWindow::MainWindow() : QMainWindow(nullptr), d(new PrivateData) {
  setWindowTitle("Multiplier");
  setWindowIcon(QIcon(":/Icons/Multiplier"));

  auto database_path = QFileDialog::getOpenFileName(
      this, tr("Select a Multiplier database"), QDir::homePath());

  d->index = mx::EntityProvider::in_memory_cache(
      mx::EntityProvider::from_database(database_path.toStdString()));

  d->database = IDatabase::Create(d->index, d->file_location_cache);

  InitializeWidgets();
  InitializeToolBar();

  resize(1280, 800);
}

MainWindow::~MainWindow() {}

void MainWindow::InitializeWidgets() {
  d->view_menu = new QMenu(tr("View"));
  menuBar()->addMenu(d->view_menu);

  d->enable_code_preview_action = new QAction(tr("Code preview"));
  d->enable_code_preview_action->setCheckable(true);
  d->enable_code_preview_action->setChecked(false);
  d->view_menu->addAction(d->enable_code_preview_action);

  auto ref_explorer_view_menu = new QMenu(tr("Reference Explorer"));
  d->view_menu->addMenu(ref_explorer_view_menu);

  auto ref_explorer_text_mode = new QAction(tr("Text mode"));
  ref_explorer_text_mode->setData(
      static_cast<int>(IReferenceExplorer::Mode::TextView));
  ref_explorer_view_menu->addAction(ref_explorer_text_mode);

  auto ref_explorer_tree_mode = new QAction(tr("Tree view"));
  ref_explorer_tree_mode->setData(
      static_cast<int>(IReferenceExplorer::Mode::TreeView));
  ref_explorer_view_menu->addAction(ref_explorer_tree_mode);

  auto ref_explorer_split_mode = new QAction(tr("Split"));
  ref_explorer_split_mode->setData(
      static_cast<int>(IReferenceExplorer::Mode::Split));
  ref_explorer_view_menu->addAction(ref_explorer_split_mode);

  connect(ref_explorer_view_menu, &QMenu::triggered, this,
          &MainWindow::OnRefExplorerModeSelected);

  setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::West);
  setTabPosition(Qt::RightDockWidgetArea, QTabWidget::East);
  setTabPosition(Qt::TopDockWidgetArea, QTabWidget::North);
  setTabPosition(Qt::BottomDockWidgetArea, QTabWidget::North);

  CreateProjectExplorerDock();
  CreateEntityExplorerDock();
  CreateInfoExplorerDock();
  CreateCodeView();
  CreateReferenceExplorerDock();

  tabifyDockWidget(d->entity_explorer_dock, d->project_explorer_dock);
  setDocumentMode(false);
}

void MainWindow::InitializeToolBar() {
  QSize icon_size(32, 32);
  d->toolbar.back_forward =
      new HistoryWidget(d->index, d->file_location_cache, kMaxHistorySize);
  connect(d->toolbar.back_forward, &HistoryWidget::GoToEntity, this,
          &MainWindow::OnHistoryNavigationEntitySelected);

  auto toolbar = new QToolBar(tr("Main Toolbar"), this);
  toolbar->setIconSize(icon_size);
  d->toolbar.back_forward->SetIconSize(icon_size);
  d->view_menu->addAction(toolbar->toggleViewAction());

  toolbar->addWidget(d->toolbar.back_forward);

  addToolBar(toolbar);
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

  connect(d->entity_explorer, &IEntityExplorer::EntityAction, this,
          &MainWindow::OnEntityExplorerEntityClicked);

  d->view_menu->addAction(d->entity_explorer_dock->toggleViewAction());

  d->entity_explorer_dock->setWidget(d->entity_explorer);

  addDockWidget(Qt::LeftDockWidgetArea, d->entity_explorer_dock);
}

void MainWindow::CreateInfoExplorerDock() {
  d->info_explorer_model =
      IInformationExplorerModel::Create(d->index, d->file_location_cache, this);

  auto info_explorer =
      IInformationExplorer::Create(d->info_explorer_model, this);

  connect(info_explorer, &IInformationExplorer::SelectedItemChanged, this,
          &MainWindow::OnInformationExplorerSelectionChange);

  d->info_explorer_dock = new QDockWidget(tr("Information Explorer"), this);
  d->info_explorer_dock->setWidget(info_explorer);
  d->info_explorer_dock->setAllowedAreas(Qt::AllDockWidgetAreas);

  d->view_menu->addAction(d->info_explorer_dock->toggleViewAction());
  addDockWidget(Qt::LeftDockWidgetArea, d->info_explorer_dock);
}

void MainWindow::CreateReferenceExplorerDock() {
  d->ref_explorer_tab_widget = new QTabWidget(this);
  d->ref_explorer_tab_widget->setDocumentMode(true);
  d->ref_explorer_tab_widget->setTabsClosable(true);

  d->close_active_ref_explorer_tab_shortcut = new QShortcut(
      QKeySequence::Close, d->ref_explorer_tab_widget, this,
      &MainWindow::OnCloseActiveRefExplorerTab, Qt::WidgetWithChildrenShortcut);

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
      d->index, d->file_location_cache, ref_explorer_model,
      d->ref_explorer_mode, this);

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

void MainWindow::OnCloseActiveCodeViewTab() {
  auto &tab_widget = *static_cast<QTabWidget *>(centralWidget());
  if (tab_widget.count() == 0) {
    return;
  }

  auto current_index = tab_widget.currentIndex();
  OnCodeViewTabBarClose(current_index);

  tab_widget.setFocus();
}

void MainWindow::CreateCodeView() {
  auto tab_widget = new QTabWidget();
  tab_widget->setTabsClosable(true);
  tab_widget->setDocumentMode(true);
  tab_widget->setTabBarAutoHide(false);

  d->close_active_code_tab_shortcut = new QShortcut(
      QKeySequence::Close, tab_widget, this,
      &MainWindow::OnCloseActiveCodeViewTab, Qt::WidgetWithChildrenShortcut);

  setCentralWidget(tab_widget);

  connect(tab_widget->tabBar(), &QTabBar::tabCloseRequested, this,
          &MainWindow::OnCodeViewTabBarClose);

  connect(tab_widget->tabBar(), &QTabBar::tabBarClicked, this,
          &MainWindow::OnCodeViewTabClicked);

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

void MainWindow::OpenTokenContextMenu(CodeModelIndex index) {
  QVariant action_data;
  action_data.setValue(index);

  for (auto &action : d->code_view_context_menu.menu->actions()) {
    action->setData(action_data);
  }

  QVariant related_entity_id_var =
      index.model->Data(index, ICodeModel::RelatedEntityIdRole);

  // Only enable the references browser if the token is related to an entity.
  d->code_view_context_menu.show_ref_explorer_action->setEnabled(
      related_entity_id_var.isValid());

  d->code_view_context_menu.menu->exec(QCursor::pos());
}

void MainWindow::OpenReferenceExplorer(
    RawEntityId entity_id,
    IReferenceExplorerModel::ExpansionMode expansion_mode) {
  CloseAllPopups();

  d->quick_ref_explorer = std::make_unique<QuickReferenceExplorer>(
      d->index, d->file_location_cache, entity_id, expansion_mode,
      d->ref_explorer_mode, this);

  connect(d->quick_ref_explorer.get(), &QuickReferenceExplorer::SaveAll, this,
          &MainWindow::OnQuickRefExplorerSaveAllClicked);

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
      index.model->Data(index, ICodeModel::RealRelatedEntityIdRole);

  if (!related_entity_id_var.isValid()) {
    CloseAllPopups();
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
      index.model->Data(index, ICodeModel::RealRelatedEntityIdRole);

  if (related_entity_id_var.isValid()) {
    OpenReferenceExplorer(qvariant_cast<RawEntityId>(related_entity_id_var),
                          IReferenceExplorerModel::TaintMode);
    return;
  }

  CloseAllPopups();
}

void MainWindow::OpenTokenEntityInfo(CodeModelIndex index) {

  QVariant related_entity_id_var =
      index.model->Data(index, ICodeModel::RealRelatedEntityIdRole);

  if (!related_entity_id_var.isValid()) {
    return;
  }

  OpenEntityInfo(qvariant_cast<RawEntityId>(related_entity_id_var));
}

void MainWindow::OpenCodePreview(const CodeModelIndex &index) {
  CloseAllPopups();

  QVariant related_entity_id_var =
      index.model->Data(index, ICodeModel::RealRelatedEntityIdRole);

  if (!related_entity_id_var.isValid()) {
    return;
  }

  auto related_entity_id = qvariant_cast<RawEntityId>(related_entity_id_var);

  d->quick_code_view = std::make_unique<QuickCodeView>(
      d->index, d->file_location_cache, related_entity_id, this);

  connect(d->quick_code_view.get(), &QuickCodeView::TokenTriggered, this,
          &MainWindow::OnTokenTriggered);

  auto dialog_pos = QCursor::pos();
  d->quick_code_view->move(dialog_pos.x() - 20, dialog_pos.y() - 20);

  auto margin = fontMetrics().height();
  auto max_width = margin + (width() / 3);
  auto max_height = margin + (height() / 4);

  d->quick_code_view->resize(max_width, max_height);
  d->quick_code_view->show();
}

void MainWindow::CloseQuickRefExplorerPopup() {
  if (d->quick_ref_explorer == nullptr) {
    return;
  }

  d->quick_ref_explorer->close();
  d->quick_ref_explorer->deleteLater();
  d->quick_ref_explorer.release();
}

void MainWindow::CloseCodePreviewPopup() {
  if (d->quick_code_view == nullptr) {
    return;
  }

  d->quick_code_view->close();
  d->quick_code_view->deleteLater();
  d->quick_code_view.release();
}

void MainWindow::CloseAllPopups() {
  CloseQuickRefExplorerPopup();
  CloseCodePreviewPopup();
}

ICodeView *MainWindow::CreateNewCodeView(RawEntityId file_entity_id,
                                         QString tab_name) {
  auto model = ICodeModel::Create(d->file_location_cache, d->index, this);
  auto code_view = ICodeView::Create(model);

  model->SetEntity(file_entity_id);

  code_view->SetWordWrapping(false);
  code_view->setAttribute(Qt::WA_DeleteOnClose);

  // Make it so that the initial last location for a given view is the file ID.
  code_view->setProperty(kLastLocationProperty, file_entity_id);
  code_view->setProperty(kFileIdProperty, file_entity_id);

  auto *central_tab_widget = dynamic_cast<QTabWidget *>(centralWidget());
  central_tab_widget->addTab(code_view, tab_name);

  connect(code_view, &ICodeView::TokenTriggered, this,
          &MainWindow::OnTokenTriggered);

  connect(code_view, &ICodeView::CursorMoved, this,
          &MainWindow::OnMainCodeViewCursorMoved);

  return code_view;
}

ICodeView *
MainWindow::GetOrCreateFileCodeView(RawEntityId file_id,
                                    std::optional<QString> opt_tab_name) {
  QTabWidget *tab_widget = dynamic_cast<QTabWidget *>(centralWidget());

  for (auto i = 0; i < tab_widget->count(); ++i) {
    QWidget *nth_widget = tab_widget->widget(i);
    QVariant tab_file_id = nth_widget->property(kFileIdProperty);
    if (!tab_file_id.isValid()) {
      continue;
    }

    if (qvariant_cast<RawEntityId>(tab_file_id) == file_id) {
      return dynamic_cast<ICodeView *>(nth_widget);
    }
  }

  if (opt_tab_name.has_value()) {
    return CreateNewCodeView(file_id, opt_tab_name.value());
  }

  for (auto [path, id] : d->index.file_paths()) {
    if (id.Pack() != file_id) {
      continue;
    }

    QString tab_name = QString::fromStdString(path.filename().generic_string());
    return CreateNewCodeView(file_id, tab_name);
  }

  return nullptr;
}

void MainWindow::OpenEntityRelatedToToken(const CodeModelIndex &index) {
  auto entity_id_var =
      index.model->Data(index, ICodeModel::RelatedEntityIdRole);
  if (!entity_id_var.isValid()) {
    return;
  }

  RawEntityId entity_id = qvariant_cast<RawEntityId>(entity_id_var);
  VariantId vid = EntityId(entity_id).Unpack();
  if (std::holds_alternative<InvalidId>(vid)) {
    return;
  }

  // This function is called when a user does something like a primary click
  // on an entity, either from inside of the main file code view, or from a
  // reference browser code preview. Save wherever we were taken away from as
  // the last thing in history for the current view.
  d->toolbar.back_forward->CommitCurrentLocationToHistory();

  OpenEntityInfo(entity_id);
  OpenEntityCode(entity_id);
}

void MainWindow::OpenEntityInfo(RawEntityId entity_id) {
  d->info_explorer_model->RequestEntityInformation(entity_id);
}

void MainWindow::OpenEntityCode(RawEntityId entity_id, bool canonicalize) {
  // TODO(pag): Make this fetch the entity via a QFuture or something like that.
  VariantEntity entity = d->index.entity(entity_id);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  if (canonicalize && std::holds_alternative<Decl>(entity)) {
    Decl canon = std::get<Decl>(entity).canonical_declaration();
    entity_id = canon.id().Pack();
    entity = std::move(canon);
  }

  std::optional<File> opt_file = FileOfEntity(entity);
  if (!opt_file.has_value()) {
    return;
  }

  auto *central_tab_widget = dynamic_cast<QTabWidget *>(centralWidget());
  auto *current_tab = central_tab_widget->currentWidget();
  ICodeView *code_view = GetOrCreateFileCodeView(opt_file->id().Pack());
  if (!code_view) {
    return;
  }

  // Change the tab.
  //
  // NOTE(pag): We don't commit the current location to history here because
  //            this function is also used to handle opening up an entity from
  //            our history.
  if (current_tab != code_view) {
    central_tab_widget->setCurrentWidget(code_view);
  }

  // We've asked to open a specific entity id. Record it as the last open
  // entity ID in this view.
  code_view->setProperty(kLastLocationProperty, entity_id);
  d->toolbar.back_forward->SetCurrentLocation(entity_id);

  // Scroll to the appropriate line.
  //
  // TODO(pag): Eventually support scrolling to a particular token.
  if (Token opt_tok = FirstFileToken(entity)) {
    auto maybe_loc = opt_tok.location(d->file_location_cache);
    if (!maybe_loc.has_value()) {
      return;
    }

    auto [line, col] = maybe_loc.value();
    code_view->ScrollToLineNumber(line);
  }
}

//! Called when a file name in the project explorer (list of indexed files) is
//! clicked.
void MainWindow::OnIndexViewFileClicked(RawEntityId file_id, QString tab_name,
                                        Qt::KeyboardModifiers,
                                        Qt::MouseButtons) {
  CloseAllPopups();
  OpenEntityInfo(file_id);

  auto *central_tab_widget = dynamic_cast<QTabWidget *>(centralWidget());
  QWidget *current_tab = central_tab_widget->currentWidget();
  QWidget *next_tab = GetOrCreateFileCodeView(file_id, tab_name);
  if (!next_tab) {
    return;
  }

  // If we're opening a file view, and if there is at least one other file
  // view already open, then notify the system that we might need to add the
  // last open tab's current position to the history. If there were no tabs
  // open prior to this request, then we don't want to force the beginning of
  // file location into history.
  //
  // NOTE(pag): We *don't* change the `kLastLocationProperty` property. It is
  //            either initialized to the file ID, or it is whatever it was with
  //            the last click or cursor event.
  if (current_tab != next_tab) {
    d->toolbar.back_forward->CommitCurrentLocationToHistory();
  }

  central_tab_widget->setCurrentWidget(next_tab);

  // Now that we're removed a tab, we need to figure out what our new current
  // location is.
  UpdateLocatiomFromWidget(next_tab);
}

//! Called when we user moves their cursor, or when an action triggered by the
//! user causes the view itself to re-position the cursor in a file code view.
//! Wherever the new cursor position is, we capture the token under the cursor
//! and record it to the current file view's tab as its current location, so
//! that if we switch away from the tab, then we can store that current location
//! into the history.
void MainWindow::OnMainCodeViewCursorMoved(const CodeModelIndex &index) {
  QVariant entity_id_var = index.model->Data(index, ICodeModel::TokenIdRole);
  if (!entity_id_var.isValid()) {
    return;
  }

  RawEntityId entity_id = qvariant_cast<RawEntityId>(entity_id_var);
  d->toolbar.back_forward->SetCurrentLocation(entity_id);

  QTabWidget *tab_widget = dynamic_cast<QTabWidget *>(centralWidget());
  if (QWidget *code_tab = tab_widget->currentWidget()) {
    code_tab->setProperty(kLastLocationProperty, entity_id_var);
  }
}

//! Called when we interact with a token in a main file view, or in a code
//! preview, e.g. a reference browser code view.
void MainWindow::OnTokenTriggered(const ICodeView::TokenAction &token_action,
                                  const CodeModelIndex &index) {

  if (token_action.type == ICodeView::TokenAction::Type::Primary) {
    OpenEntityRelatedToToken(index);

  } else if (token_action.type == ICodeView::TokenAction::Type::Secondary) {
    OpenTokenContextMenu(index);

  } else if (token_action.type == ICodeView::TokenAction::Type::Hover) {
    if (d->enable_code_preview_action->isChecked()) {
      OpenCodePreview(index);
    }

  } else if (token_action.type == ICodeView::TokenAction::Type::Keyboard) {
    // Here to test keyboard events; Before we add more buttons, we should
    // find a better strategy to managed them.
    //
    // Ideally we should find a Qt-friendly method that the framework handles
    // well natively
    const auto &keyboard_button = token_action.opt_keyboard_button.value();
    if (keyboard_button.shift_modifier || keyboard_button.control_modifier) {
      return;
    }

    // Like in IDA Pro, pressing X while the cursor is on an entity shows us
    // its cross-references.
    if (keyboard_button.key == Qt::Key_X) {
      OpenTokenReferenceExplorer(index);

    } else if (keyboard_button.key == Qt::Key_P) {
      OpenCodePreview(index);

    } else if (keyboard_button.key == Qt::Key_T) {
      OpenTokenTaintExplorer(index);

    } else if (keyboard_button.key == Qt::Key_I) {
      d->info_explorer_dock->show();
      OpenTokenEntityInfo(index);

    } else if (keyboard_button.key == Qt::Key_Enter) {
      // Like in IDA Pro, pressing Enter while the cursor is on a use of that
      // entity will bring us to that entity.
      OpenEntityRelatedToToken(index);
    }
  }
}

//! Called when an entity in the entity explorer / filter, which allows us to
//! search for entities by name/type, is clicked.
void MainWindow::OnEntityExplorerEntityClicked(RawEntityId entity_id) {
  CloseAllPopups();
  d->toolbar.back_forward->CommitCurrentLocationToHistory();
  OpenEntityInfo(entity_id);
  OpenEntityCode(entity_id, false /* don't canonicalize entity IDs */);
}

//! Called when the history menu is used to go back/forward to some specific
//! entity ID.
void MainWindow::OnHistoryNavigationEntitySelected(RawEntityId,
                                                   RawEntityId canonical_id) {
  CloseAllPopups();
  OpenEntityInfo(canonical_id);
  OpenEntityCode(canonical_id);
}

void MainWindow::OnInformationExplorerSelectionChange(
    const QModelIndex &index) {
  auto entity_id_var = index.data(IInformationExplorerModel::EntityIdRole);
  if (!entity_id_var.isValid()) {
    return;
  }

  auto entity_id = qvariant_cast<RawEntityId>(entity_id_var);

  d->toolbar.back_forward->CommitCurrentLocationToHistory();
  OpenEntityCode(entity_id, false /* don't canonicalize entity IDs */);
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
  d->toolbar.back_forward->CommitCurrentLocationToHistory();
  OpenEntityInfo(entity_id);
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

//! Tell the history back/forward button widget that our current location has
//! changed.
void MainWindow::UpdateLocatiomFromWidget(QWidget *widget) {
  if (!widget) {
    return;
  }

  QVariant entity_id_var = widget->property(kLastLocationProperty);
  if (!entity_id_var.isValid()) {
    return;
  }

  d->toolbar.back_forward->SetCurrentLocation(
      qvariant_cast<RawEntityId>(entity_id_var));
}

void MainWindow::OnCodeViewTabBarClose(int index) {
  CloseAllPopups();

  QTabWidget *central_tab_widget = dynamic_cast<QTabWidget *>(centralWidget());
  QWidget *current_widget = central_tab_widget->currentWidget();
  QWidget *closing_widget = central_tab_widget->widget(index);
  if (!closing_widget) {
    return;
  }

  // If the tab we're closing is the currently open tab, then we want to add
  // its last location to the history. If they don't match, then it means we're
  // closing some non-active tab, i.e. we didn't switch to go see that code
  // first.
  if (closing_widget == current_widget) {
    d->toolbar.back_forward->CommitCurrentLocationToHistory();
  }

  central_tab_widget->removeTab(index);
  closing_widget->close();

  // Now that we're removed a tab, we need to figure out what our new current
  // location is.
  UpdateLocatiomFromWidget(central_tab_widget->currentWidget());
}

void MainWindow::OnCodeViewTabClicked(int index) {
  CloseAllPopups();

  QTabWidget *central_tab_widget = dynamic_cast<QTabWidget *>(centralWidget());
  if (central_tab_widget->currentWidget() !=
      central_tab_widget->widget(index)) {
    d->toolbar.back_forward->CommitCurrentLocationToHistory();
  }
}

void MainWindow::OnCloseActiveRefExplorerTab() {
  if (d->ref_explorer_tab_widget->count() == 0) {
    return;
  }

  auto current_index = d->ref_explorer_tab_widget->currentIndex();
  OnReferenceExplorerTabBarClose(current_index);

  d->ref_explorer_tab_widget->setFocus();
}

void MainWindow::OnRefExplorerModeSelected(QAction *action) {
  auto ref_explorer_int_mode = action->data().toInt();
  d->ref_explorer_mode =
      static_cast<IReferenceExplorer::Mode>(ref_explorer_int_mode);
}

}  // namespace mx::gui
