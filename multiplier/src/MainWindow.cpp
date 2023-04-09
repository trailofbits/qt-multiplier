// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MainWindow.h"
#include "InformationExplorer.h"
#include "PreviewableReferenceExplorer.h"
#include "QuickReferenceExplorer.h"
#include "SimpleTextInputDialog.h"

#include <multiplier/ui/IIndexView.h>
#include <multiplier/ui/IReferenceExplorer.h>
#include <multiplier/ui/IEntityExplorer.h>
#include <multiplier/ui/Util.h>
#include <multiplier/ui/Assert.h>
#include <multiplier/ui/IDatabase.h>

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

namespace mx::gui {

namespace {

const std::size_t kMaxHistorySize{30};

//! The file id associated with a code view.
static const char *kFileIdProperty = "mx:fileId";

//! The last cursor location in a code view.
static const char *kLastLocationProperty = "mx:lastLocation";

static const char * const kBackButtonToolTip =
    "Go back in the navigation history";

static const char * const kForwardButtonToolTip =
    "Go forward in the navigation history";


struct CodeViewContextMenu final {
  QMenu *menu{nullptr};
  QAction *show_ref_explorer_action{nullptr};
};

struct ToolBar final {
  QAction *history_back_action{nullptr};
  QAction *history_forward_action{nullptr};

  QToolButton *history_back_button{nullptr};
  QToolButton *history_forward_button{nullptr};
};

}  // namespace

struct MainWindow::PrivateData final {
  mx::Index index;
  mx::FileLocationCache file_location_cache;

  IDatabase::Ptr database;

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
  ToolBar toolbar;
  History history;
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
  d->toolbar.history_back_action = new QAction(tr("Back"), this);
  d->toolbar.history_forward_action = new QAction(tr("Forward"), this);

  d->toolbar.history_back_action->setToolTip(tr(kBackButtonToolTip));
  d->toolbar.history_forward_action->setToolTip(tr(kForwardButtonToolTip));

  d->toolbar.history_back_button = new QToolButton(this);
  d->toolbar.history_back_button->setPopupMode(QToolButton::MenuButtonPopup);
  d->toolbar.history_back_button->setDefaultAction(
      d->toolbar.history_back_action);

  d->toolbar.history_forward_button = new QToolButton(this);
  d->toolbar.history_forward_button->setPopupMode(QToolButton::MenuButtonPopup);
  d->toolbar.history_forward_button->setDefaultAction(
      d->toolbar.history_forward_action);

  d->toolbar.history_back_button->setIcon(
      QIcon(":/Icons/MainWindow/HistoryBack"));

  d->toolbar.history_forward_button->setIcon(
      QIcon(":/Icons/MainWindow/HistoryForward"));

  connect(d->toolbar.history_back_action, &QAction::triggered,
          this, &MainWindow::OnNavigateBack);

  connect(d->toolbar.history_forward_action, &QAction::triggered,
          this, &MainWindow::OnNavigateForward);

  auto toolbar = new QToolBar(tr("Main Toolbar"), this);
  toolbar->setIconSize(QSize(32, 32));
  d->view_menu->addAction(toolbar->toggleViewAction());

  toolbar->addWidget(d->toolbar.history_back_button);
  toolbar->addWidget(d->toolbar.history_forward_button);

  addToolBar(toolbar);

  // By default, there is no history, so there is nowhere for these buttons to
  // navigate.
  d->toolbar.history_back_button->setEnabled(false);
  d->toolbar.history_forward_button->setEnabled(false);
}

void MainWindow::CreateProjectExplorerDock() {
  auto file_tree_model = IFileTreeModel::Create(d->index, this);
  d->index_view = IIndexView::Create(file_tree_model, this);

  connect(d->index_view, &IIndexView::FileClicked,
          this, &MainWindow::OnIndexViewFileClicked);

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
          this, &MainWindow::OnPreviewCodeViewTokenTriggered);

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

  connect(tab_widget->tabBar(), &QTabBar::tabBarClicked,
          this, &MainWindow::OnCodeViewTabClicked);

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
      index.model->Data(index, ICodeModel::RelatedEntityIdRole);

  // Only enable the references browser if the token is related to an entity.
  d->code_view_context_menu.show_ref_explorer_action->setEnabled(
      related_entity_id_var.isValid());

  d->code_view_context_menu.menu->exec(QCursor::pos());
}

void MainWindow::OpenReferenceExplorer(
    RawEntityId entity_id,
    IReferenceExplorerModel::ExpansionMode expansion_mode) {
  CloseTokenReferenceExplorer();

  d->quick_ref_explorer = std::make_unique<QuickReferenceExplorer>(
      d->index, d->file_location_cache, entity_id, expansion_mode, this);

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
      index.model->Data(index, ICodeModel::RelatedEntityIdRole);

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
      index.model->Data(index, ICodeModel::RelatedEntityIdRole);

  if (related_entity_id_var.isValid()) {
    OpenReferenceExplorer(qvariant_cast<RawEntityId>(related_entity_id_var),
                          IReferenceExplorerModel::TaintMode);
    return;
  }

  CloseTokenReferenceExplorer();
}

void MainWindow::OpenTokenEntityInfo(CodeModelIndex index) {

  QVariant related_entity_id_var =
      index.model->Data(index, ICodeModel::RelatedEntityIdRole);

  if (!related_entity_id_var.isValid()) {
    return;
  }

  OpenEntityInfo(qvariant_cast<RawEntityId>(related_entity_id_var),
                 true /* force */);
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

  model->SetEntity(file_entity_id);

  code_view->SetWordWrapping(false);
  code_view->setAttribute(Qt::WA_DeleteOnClose);

  // Make it so that the initial last location for a given view is the file ID.
  code_view->setProperty(kLastLocationProperty, file_entity_id);
  code_view->setProperty(kFileIdProperty, file_entity_id);

  auto *central_tab_widget = dynamic_cast<QTabWidget *>(centralWidget());
  central_tab_widget->addTab(code_view, tab_name);

  connect(code_view, &ICodeView::TokenTriggered,
          this, &MainWindow::OnMainCodeViewTokenTriggered);

  connect(code_view, &ICodeView::CursorMoved,
          this, &MainWindow::OnMainCodeViewCursorMoved);

  return code_view;
}

//! Tells us that we're about to change to `next_widget` in the central tab
//! view of code views. If `next_widget` is different than the current widget
//! then we want to add whatever the last location in it is to the history.
void MainWindow::AboutToChangeToCodeView(QWidget *next_widget) {
  QTabWidget *central_tab_widget = dynamic_cast<QTabWidget *>(centralWidget());
  QWidget *current_widget = central_tab_widget->currentWidget();

  if (!current_widget || current_widget == next_widget) {
    return;
  }

  AddToHistory(current_widget->property(kLastLocationProperty));
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

void MainWindow::UpdateHistoryMenus() {

  if (d->history.item_list.empty()) {
    return;
  }

  // Seems like updating the existing menus does not always work. Sometimes
  // they show up out of date when clicking the buttons.
  //
  // Create them from scratch for the time being
  for (QToolButton *button :
       {d->toolbar.history_back_button, d->toolbar.history_forward_button}) {

    if (QMenu *menu = button->menu()) {
      button->setMenu(nullptr);
      menu->deleteLater();
    }
  }

  QMenu *history_back_menu = new QMenu(tr("Previous history menu"));
  connect(history_back_menu, &QMenu::triggered,
          this, &MainWindow::OnNavigateBackToHistoryItem);

  QMenu *history_forward_menu = new QMenu(tr("Next history menu"));
  connect(history_forward_menu, &QMenu::triggered,
          this, &MainWindow::OnNavigateForwardToHistoryItem);

  int num_back_actions = 0;
  int num_forward_actions = 0;
  int item_index = 0;

  // Populate everything up to the current item in the back button sub-menu.
  std::vector<QAction *> back_history_action_list;
  for (auto item_it = d->history.item_list.begin();
       item_it != d->history.current_item_it; ++item_it) {

    const History::Item &item = *item_it;
    QAction *action = new QAction(item.name);
    action->setData(item_index++);

    back_history_action_list.push_back(action);
    ++num_back_actions;
  }

  // NOTE(pag): Reverse the order of actions order so that the first added
  //            action, which will be triggered on button click, also
  //            corresponds to the most recent thing in the past.
  std::reverse(back_history_action_list.begin(),
               back_history_action_list.end());
  for (QAction *action : back_history_action_list) {
    history_back_menu->addAction(action);
  }

  // Populate everything after the current item into the forward button
  // sub-menu.
  if (d->history.current_item_it != d->history.item_list.end()) {
    for (auto item_it = std::next(d->history.current_item_it, 1);
         item_it != d->history.item_list.end(); ++item_it) {

      const History::Item &item = *item_it;
      QAction *action = new QAction(item.name);
      action->setData(++item_index);

      history_forward_menu->addAction(action);
      ++num_forward_actions;
    }
  }

  // Enable/disable and customize the tool tip for the backward button.
  d->toolbar.history_back_button->setMenu(history_back_menu);
  if (num_back_actions) {
    d->toolbar.history_back_action->setToolTip(
        tr("Go back to ") + history_back_menu->actions().first()->text());
  } else {
    d->toolbar.history_back_action->setToolTip(tr(kBackButtonToolTip));
  }

  // Enable/disable and customize the tool tip for the forward button.
  d->toolbar.history_forward_button->setMenu(history_forward_menu);
  if (num_forward_actions) {
    d->toolbar.history_forward_action->setToolTip(
        tr("Go forward to ") + history_forward_menu->actions().first()->text());
  } else {
    d->toolbar.history_forward_action->setToolTip(tr(kForwardButtonToolTip));
  }

  // NOTE(pag): For some reason, resetting the tooltips removes the icons,
  //            so add them back in.

  d->toolbar.history_back_button->setIcon(
      QIcon(":/Icons/MainWindow/HistoryBack"));

  d->toolbar.history_forward_button->setIcon(
      QIcon(":/Icons/MainWindow/HistoryForward"));

  // NOTE(pag): It seems like setting icones or tooltips also re-enables the
  //            buttons, so we possible disable them here.
  d->toolbar.history_back_button->setEnabled(0 < num_back_actions);
  d->toolbar.history_forward_button->setEnabled(0 < num_forward_actions);
}

void MainWindow::UpdateCurrentCodeTabLocation(QVariant entity_id) {
  if (entity_id.isValid()) {
    QTabWidget *tab_widget = static_cast<QTabWidget *>(centralWidget());
    if (QWidget *code_tab = tab_widget->currentWidget()) {
      code_tab->setProperty(kLastLocationProperty, entity_id);
    }
  }
}

std::optional<QString> MainWindow::HistoryLabel(
    const FileLocationCache &file_location_cache, const VariantEntity &entity) {

  std::optional<QString> entity_label;
  std::optional<QString> in_label;
  QString line_col_label;
  QString file_label;

  std::optional<File> maybe_file = File::containing(entity);
  if (!maybe_file.has_value()) {
    return std::nullopt;
  }

  for (std::filesystem::path path : maybe_file->paths()) {
    file_label = QString::fromStdString(path.filename().generic_string());
    break;
  }

  Token file_loc = FirstFileToken(entity);

  if (VariantEntity containing_entity = NamedDeclContaining(entity);
      IdOfEntity(containing_entity) != IdOfEntity(entity)) {
    in_label = NameOfEntity(containing_entity);
  }

  if (std::holds_alternative<Token>(entity)) {

    // Due to the nature of cursor setting in code views, it's possible that
    // our history stores more token IDs rather than entity IDs. If we can match
    // a token to be the "location" of the an entity, then use that entity's
    // name in our label.
    if (VariantEntity related_entity = std::get<Token>(entity).related_entity();
        file_loc == FirstFileToken(related_entity)) {
      entity_label = NameOfEntity(related_entity);
    }

  } else if (std::holds_alternative<Decl>(entity)) {
    entity_label = NameOfEntity(entity);

  } else if (std::holds_alternative<File>(entity)) {

  } else {
    return std::nullopt;
  }

  if (!std::holds_alternative<File>(entity)) {
    if (auto maybe_line_col = file_loc.location(file_location_cache)) {
      line_col_label = ":" + QString::number(maybe_line_col->first) +
                       ":" + QString::number(maybe_line_col->second);
    }
  }

  QString label = file_label + line_col_label;
  if (entity_label.has_value() && !entity_label->isEmpty()) {
    if (in_label.has_value() && !in_label->isEmpty() &&
        in_label.value() != entity_label.value()) {
      label = entity_label.value() + tr(" at ") + label + tr(" in ") +
              in_label.value();
    } else {
      label = entity_label.value() + tr(" at ") + label;
    }
  } else if (in_label.has_value() && !in_label->isEmpty()) {
    label += tr(" in ") + in_label.value();
  }

  if (label.isEmpty()) {
    return std::nullopt;
  }

  return label;
}

void MainWindow::AddToHistory(QVariant opt_entity_id) {

  if (!opt_entity_id.isValid()) {
    return;
  }

  RawEntityId entity_id = qvariant_cast<RawEntityId>(opt_entity_id);
  VariantId vid = EntityId(entity_id).Unpack();
  if (std::holds_alternative<InvalidId>(vid)) {
    return;
  }

  VariantEntity entity = d->index.entity(entity_id);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  std::optional<QString> maybe_label = HistoryLabel(
      d->file_location_cache, entity);

  Assert(maybe_label.has_value(),
         "Invalid parameter combination");

  History::ItemList &items = d->history.item_list;
  History::ItemList::iterator &curr_it = d->history.current_item_it;

  // Check to see if we should
  if (curr_it != items.end()) {

    // If the next item to add matches the next item in our history, then
    // advance us one spot through the history.
    if (curr_it->entity_id == entity_id) {
      curr_it = std::next(curr_it, 1);
      return;
    }

    // Otherwise, truncate the "previous future" history.
    items.erase(curr_it, items.end());

  // Adding this item would cause a repeat of the most recently added item, so
  // ignore it.
  } else if (!items.empty() && items.back().entity_id == entity_id) {
    return;
  }

  // Add the new item.
  items.emplace_back(
      History::Item{entity_id, maybe_label.value()});
  curr_it = items.end();

  // Cull overly old history.
  if (auto num_items = items.size(); num_items > kMaxHistorySize) {
    auto num_items_to_delete = static_cast<int>(num_items - kMaxHistorySize);
    auto range_start = items.begin();
    auto range_end = std::next(range_start, num_items_to_delete);
    items.erase(range_start, range_end);
  }

  UpdateHistoryMenus();
}

void MainWindow::NavigateBackToHistoryItem(
    History::ItemList::iterator next_item_it) {

  RawEntityId entity_id = next_item_it->entity_id;

  // If we're going back to some place in the past, and if we're starting from
  // "the present," then we need to materialize a history item representing our
  // present location so that when we go backward, we can always return to our
  // current (soon to be former) present location.
  if (d->history.current_item_it == d->history.item_list.end()) {
    auto index = std::distance(d->history.item_list.begin(), next_item_it);
    AboutToChangeToCodeView(nullptr);
    next_item_it = std::next(d->history.item_list.begin(), index);
  }

  Assert(2u <= d->history.item_list.size(), "Invalid history list");

  d->history.current_item_it = next_item_it;

  UpdateHistoryMenus();
  OpenEntityCode(entity_id);
}

void MainWindow::NavigateForwardToHistoryItem(
    History::ItemList::iterator next_item_it) {

  Assert(2u <= d->history.item_list.size(), "Invalid history list");
  Assert(next_item_it != d->history.item_list.begin(), "Invalid history item");
  Assert(next_item_it != d->history.item_list.end(), "Invalid history item");

  RawEntityId entity_id = next_item_it->entity_id;

  // If we're back to the present, then take off the previously materialized
  // "present" value.
  if (std::next(next_item_it, 1) == d->history.item_list.end()) {
    d->history.item_list.pop_back();
    d->history.current_item_it = d->history.item_list.end();

  } else {
    d->history.current_item_it = next_item_it;
  }

  UpdateHistoryMenus();
  OpenEntityCode(entity_id);
}

void MainWindow::OpenEntityRelatedToToken(const CodeModelIndex &index) {
  auto entity_id_var =
      index.model->Data(index, ICodeModel::RelatedEntityIdRole);

  if (!entity_id_var.isValid()) {
    return;
  }

  // This function is called when a user does something like a primary click
  // on an entity, either from inside of the main file code view, or from a
  // reference browser code preview. Save wherever we were taken away from as
  // the last thing in history for the current view.
  AboutToChangeToCodeView(nullptr);

  auto entity_id = qvariant_cast<RawEntityId>(entity_id_var);
  OpenEntityInfo(entity_id);
  OpenEntityCode(entity_id);
}

void MainWindow::OpenEntityInfo(RawEntityId entity_id, bool force) {
  auto make_visible = !d->info_explorer_opened_before ||
                      d->info_explorer_dock->isVisible() || force;

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

  // We've asked to open a specific entity id. Record it as the last open
  // entity ID in this view.
  code_view->setProperty(kLastLocationProperty, entity_id);

  // Change the tab.
  if (current_tab != code_view) {
    central_tab_widget->setCurrentWidget(code_view);
  }

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

void MainWindow::OnIndexViewFileClicked(RawEntityId file_id, QString tab_name,
                                        Qt::KeyboardModifiers,
                                        Qt::MouseButtons) {
  CloseTokenReferenceExplorer();
  OpenEntityInfo(file_id);

  auto *central_tab_widget = dynamic_cast<QTabWidget *>(centralWidget());
  QWidget *next_tab = GetOrCreateFileCodeView(file_id, tab_name);
  if (!next_tab) {
    return;
  }

  // NOTE(pag): We *don't* change the `kLastLocationProperty` property. It is
  //            either initialized to the file ID, or it is whatever it was with
  //            the last click or cursor event.

  AboutToChangeToCodeView(next_tab);

  central_tab_widget->setCurrentWidget(next_tab);
}

void MainWindow::OnMainCodeViewCursorMoved(const CodeModelIndex &index) {
  // When we click or change position in the main code view, we want to
  // update the last clicked location for the active tab. If this click
  // goes anywhere, then that last click location becomes a history item.
  UpdateCurrentCodeTabLocation(
      index.model->Data(index, ICodeModel::TokenIdRole));
}

void MainWindow::OnMainCodeViewTokenTriggered(
    const ICodeView::TokenAction &token_action, const CodeModelIndex &index) {

  // When we click in the main code view, we want to update the last clicked
  // location for the active tab. If this click goes anywhere, then that last
  // click location becomes a history item.
  UpdateCurrentCodeTabLocation(
      index.model->Data(index, ICodeModel::TokenIdRole));

  OnTokenTriggered(token_action, index);
}

void MainWindow::OnPreviewCodeViewTokenTriggered(
      const ICodeView::TokenAction &token_action,
      const CodeModelIndex &index) {
  OnTokenTriggered(token_action, index);
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
  OpenEntityInfo(entity_id);
  OpenEntityCode(entity_id);
}

void MainWindow::OnNavigateBack() {
  Assert(d->history.current_item_it != d->history.item_list.begin(),
         "Too far back");

  NavigateBackToHistoryItem(std::prev(d->history.current_item_it, 1));
}

void MainWindow::OnNavigateForward() {
  Assert(d->history.current_item_it != d->history.item_list.end(),
         "Too far forward");

  NavigateForwardToHistoryItem(std::next(d->history.current_item_it, 1));
}

void MainWindow::OnNavigateBackToHistoryItem(QAction *action) {
  QVariant item_index_var = action->data();
  if (item_index_var.isValid()) {
    NavigateBackToHistoryItem(std::next(d->history.item_list.begin(),
                                        item_index_var.toInt()));
  }
}

void MainWindow::OnNavigateForwardToHistoryItem(QAction *action) {
  QVariant item_index_var = action->data();
  if (item_index_var.isValid()) {
    NavigateForwardToHistoryItem(std::next(d->history.item_list.begin(),
                                           item_index_var.toInt()));
  }
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

void MainWindow::OnCodeViewTabBarClose(int index) {
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
    AboutToChangeToCodeView(nullptr);
  }

  central_tab_widget->removeTab(index);
  closing_widget->close();
}

void MainWindow::OnCodeViewTabClicked(int index) {
  QTabWidget *central_tab_widget = dynamic_cast<QTabWidget *>(centralWidget());
  if (QWidget *next_widget = central_tab_widget->widget(index)) {
    AboutToChangeToCodeView(next_widget);
  }
}

}  // namespace mx::gui
