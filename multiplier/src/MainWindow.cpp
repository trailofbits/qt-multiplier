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
#include "MxTabWidget.h"

#include <multiplier/ui/Assert.h>
#include <multiplier/ui/HistoryWidget.h>
#include <multiplier/ui/IProjectExplorer.h>
#include <multiplier/ui/IReferenceExplorer.h>
#include <multiplier/ui/IEntityExplorer.h>
#include <multiplier/ui/Util.h>
#include <multiplier/ui/IInformationExplorer.h>
#include <multiplier/ui/IGlobalHighlighter.h>
#include <multiplier/ui/IMacroExplorer.h>
#include <multiplier/ui/CodeViewTheme.h>
#include <multiplier/ui/IThemeManager.h>
#include <multiplier/ui/Icons.h>

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
#include <QTreeView>
#include <QColorDialog>
#include <QActionGroup>

namespace mx::gui {

namespace {

const std::size_t kMaxHistorySize{30};

//! The file id associated with a code view.
static const char *kFileIdProperty = "mx:fileId";

//! The last cursor location in a code view.
static const char *kLastLocationProperty = "mx:lastLocation";

static const auto kHighlightEntityIdRole = ICodeModel::RealRelatedEntityIdRole;

struct CodeViewContextMenu final {
  QMenu *menu{nullptr};
  QAction *show_ref_explorer_action{nullptr};

  QMenu *highlight_menu{nullptr};
  QAction *set_entity_highlight{nullptr};
  QAction *remove_entity_highlight{nullptr};
  QAction *reset_entity_highlights{nullptr};
};

struct ToolBar final {
  HistoryWidget *back_forward{nullptr};
  QAction *browser_mode{nullptr};
};

std::optional<QRect> GetRestoredPopupPlacement(const QWidget *widget) {
  if (widget == nullptr) {
    return std::nullopt;
  }

  auto widget_rect = widget->rect().translated(widget->pos());
  if (!widget_rect.contains(QCursor::pos())) {
    return std::nullopt;
  }

  return widget_rect;
}

}  // namespace

struct MainWindow::PrivateData final {
  mx::Index index;
  mx::FileLocationCache file_location_cache;

  IProjectExplorer *project_explorer{nullptr};
  IEntityExplorer *entity_explorer{nullptr};
  IMacroExplorer *macro_explorer{nullptr};
  CodeViewContextMenu code_view_context_menu;

  std::unique_ptr<QuickReferenceExplorer> quick_ref_explorer;

  QAction *enable_code_preview_action{nullptr};
  std::unique_ptr<QuickCodeView> quick_code_view;

  QShortcut *close_active_ref_explorer_tab_shortcut{nullptr};
  QShortcut *close_active_code_tab_shortcut{nullptr};

  bool enable_quick_ref_explorer_code_preview{false};
  QTabWidget *ref_explorer_tab_widget{nullptr};
  QDockWidget *reference_explorer_dock{nullptr};

  QDockWidget *project_explorer_dock{nullptr};
  QDockWidget *entity_explorer_dock{nullptr};
  QDockWidget *info_explorer_dock{nullptr};
  QDockWidget *macro_explorer_dock{nullptr};
  QDockWidget *highlight_explorer_dock{nullptr};
  IInformationExplorerModel *info_explorer_model{nullptr};

  IGlobalHighlighter *global_highlighter{nullptr};

  QMenu *view_menu{nullptr};
  ToolBar toolbar;
};

MainWindow::MainWindow() : QMainWindow(nullptr), d(new PrivateData) {
  setWindowTitle("Multiplier");
  setWindowIcon(QIcon(":/Icons/Multiplier"));

  QString database_path;
  const auto &arg_list = qApp->arguments();
  if (arg_list.size() == 2) {
    database_path = arg_list[1];

  } else {
    database_path = QFileDialog::getOpenFileName(
        this, tr("Select a Multiplier database"), QDir::homePath());
  }

  d->index = mx::Index::in_memory_cache(
      mx::Index::from_database(database_path.toStdString()));

  InitializeWidgets();
  InitializeToolBar();

  connect(&IThemeManager::Get(), &IThemeManager::ThemeChanged, this,
          &MainWindow::OnThemeChange);

  resize(1280, 800);
}

MainWindow::~MainWindow() {}

void MainWindow::InitializeWidgets() {
  d->view_menu = new QMenu(tr("View"));
  menuBar()->addMenu(d->view_menu);

  auto view_theme_menu = new QMenu(tr("Theme"));
  d->view_menu->addMenu(view_theme_menu);

  auto view_theme_dark_action = new QAction(tr("Dark"));
  view_theme_menu->addAction(view_theme_dark_action);

  auto view_theme_light_action = new QAction(tr("Light"));
  view_theme_menu->addAction(view_theme_light_action);

  connect(view_theme_dark_action, &QAction::triggered, this,
          &MainWindow::OnSetDarkTheme);

  connect(view_theme_light_action, &QAction::triggered, this,
          &MainWindow::OnSetLightTheme);

  d->enable_code_preview_action = new QAction(tr("Code preview"));
  d->enable_code_preview_action->setCheckable(true);
  d->enable_code_preview_action->setChecked(false);
  d->view_menu->addAction(d->enable_code_preview_action);

  setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::West);
  setTabPosition(Qt::RightDockWidgetArea, QTabWidget::East);
  setTabPosition(Qt::TopDockWidgetArea, QTabWidget::North);
  setTabPosition(Qt::BottomDockWidgetArea, QTabWidget::North);

  CreateGlobalHighlighter();
  CreateMacroExplorerDock();
  CreateRefExplorerMenuOptions();
  CreateProjectExplorerDock();
  CreateEntityExplorerDock();
  CreateInfoExplorerDock();
  CreateCodeView();
  CreateReferenceExplorerDock();

  tabifyDockWidget(d->project_explorer_dock, d->entity_explorer_dock);
  tabifyDockWidget(d->entity_explorer_dock, d->macro_explorer_dock);
  tabifyDockWidget(d->macro_explorer_dock, d->highlight_explorer_dock);

  d->project_explorer_dock->raise();

  setDocumentMode(false);
  mx::gui::IThemeManager::Get().SendGlobalUpdate();
}

void MainWindow::InitializeToolBar() {
  QSize icon_size(32, 32);

  d->toolbar.back_forward = new HistoryWidget(d->index, d->file_location_cache,
                                              kMaxHistorySize, this, true);

  connect(d->toolbar.back_forward, &HistoryWidget::GoToEntity, this,
          &MainWindow::OnHistoryNavigationEntitySelected);

  auto toolbar = new QToolBar(tr("Main Toolbar"), this);
  toolbar->setIconSize(icon_size);
  d->toolbar.back_forward->SetIconSize(icon_size);
  d->view_menu->addAction(toolbar->toggleViewAction());

  toolbar->addWidget(d->toolbar.back_forward);

  d->toolbar.browser_mode = new QAction(tr("Browser mode"), this);
  d->toolbar.browser_mode->setCheckable(true);
  d->toolbar.browser_mode->setChecked(true);
  connect(d->toolbar.browser_mode, &QAction::toggled, this,
          &MainWindow::OnBrowserModeToggled);
  toolbar->addAction(d->toolbar.browser_mode);

  addToolBar(toolbar);
  UpdateIcons();
}

void MainWindow::CreateProjectExplorerDock() {
  auto file_tree_model = IFileTreeModel::Create(d->index, this);
  d->project_explorer = IProjectExplorer::Create(file_tree_model, this);

  connect(d->project_explorer, &IProjectExplorer::FileClicked, this,
          &MainWindow::OnProjectExplorerFileClicked);

  d->project_explorer_dock = new QDockWidget(tr("Project Explorer"), this);
  d->project_explorer_dock->setAllowedAreas(Qt::AllDockWidgetAreas);
  d->view_menu->addAction(d->project_explorer_dock->toggleViewAction());
  d->project_explorer_dock->setWidget(d->project_explorer);
  addDockWidget(Qt::LeftDockWidgetArea, d->project_explorer_dock);
}

void MainWindow::CreateEntityExplorerDock() {
  auto entity_explorer_model =
      IEntityExplorerModel::Create(d->index, d->file_location_cache, this);
  d->entity_explorer = IEntityExplorer::Create(entity_explorer_model, this,
                                               d->global_highlighter);

  d->entity_explorer_dock = new QDockWidget(tr("Entity Explorer"), this);
  d->entity_explorer_dock->setAllowedAreas(Qt::AllDockWidgetAreas);

  connect(d->entity_explorer, &IEntityExplorer::EntityAction, this,
          &MainWindow::OnOpenEntity);

  d->view_menu->addAction(d->entity_explorer_dock->toggleViewAction());

  d->entity_explorer_dock->setWidget(d->entity_explorer);

  addDockWidget(Qt::LeftDockWidgetArea, d->entity_explorer_dock);
}

void MainWindow::CreateInfoExplorerDock() {
  d->info_explorer_model =
      IInformationExplorerModel::Create(d->index, d->file_location_cache, this);

  auto info_explorer = IInformationExplorer::Create(
      d->info_explorer_model, this, d->global_highlighter);

  connect(info_explorer, &IInformationExplorer::SelectedItemChanged, this,
          &MainWindow::OnInformationExplorerSelectionChange);

  d->info_explorer_dock = new QDockWidget(tr("Information Explorer"), this);
  d->info_explorer_dock->setWidget(info_explorer);
  d->info_explorer_dock->setAllowedAreas(Qt::AllDockWidgetAreas);

  d->view_menu->addAction(d->info_explorer_dock->toggleViewAction());
  addDockWidget(Qt::LeftDockWidgetArea, d->info_explorer_dock);
}

void MainWindow::CreateMacroExplorerDock() {
  d->macro_explorer =
      IMacroExplorer::Create(d->index, d->file_location_cache, this);

  d->macro_explorer_dock = new QDockWidget(tr("Macro Explorer"), this);
  d->macro_explorer_dock->setWidget(d->macro_explorer);
  d->macro_explorer_dock->setAllowedAreas(Qt::AllDockWidgetAreas);

  d->view_menu->addAction(d->macro_explorer_dock->toggleViewAction());

  addDockWidget(Qt::LeftDockWidgetArea, d->macro_explorer_dock);
}

void MainWindow::CreateReferenceExplorerDock() {
  d->ref_explorer_tab_widget = new MxTabWidget(this);
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
  auto tab_widget = new MxTabWidget();
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

  connect(d->code_view_context_menu.menu, &QMenu::triggered, this,
          &MainWindow::OnCodeViewContextMenuActionTriggered);

  d->code_view_context_menu.show_ref_explorer_action =
      new QAction(tr("Show Reference Explorer"));

  d->code_view_context_menu.menu->addAction(
      d->code_view_context_menu.show_ref_explorer_action);

  d->code_view_context_menu.highlight_menu = new QMenu(tr("Highlights"));
  d->code_view_context_menu.menu->addMenu(
      d->code_view_context_menu.highlight_menu);

  d->code_view_context_menu.set_entity_highlight = new QAction(tr("Set color"));

  d->code_view_context_menu.highlight_menu->addAction(
      d->code_view_context_menu.set_entity_highlight);

  d->code_view_context_menu.remove_entity_highlight = new QAction(tr("Remove"));

  d->code_view_context_menu.highlight_menu->addAction(
      d->code_view_context_menu.remove_entity_highlight);

  d->code_view_context_menu.reset_entity_highlights = new QAction(tr("Reset"));

  d->code_view_context_menu.highlight_menu->addAction(
      d->code_view_context_menu.reset_entity_highlights);
}

void MainWindow::CreateGlobalHighlighter() {
  d->global_highlighter =
      IGlobalHighlighter::Create(d->index, d->file_location_cache, this);

  connect(d->global_highlighter, &IGlobalHighlighter::EntityClicked, this,
          &MainWindow::OnOpenEntity);

  d->highlight_explorer_dock =
      new QDockWidget(d->global_highlighter->windowTitle(), this);
  d->highlight_explorer_dock->setWidget(d->global_highlighter);
  d->highlight_explorer_dock->setAllowedAreas(Qt::LeftDockWidgetArea);

  d->view_menu->addAction(d->highlight_explorer_dock->toggleViewAction());
}

void MainWindow::OpenTokenContextMenu(const QModelIndex &index) {
  QVariant action_data;
  action_data.setValue(index);

  std::vector<QAction *> action_list{
      d->code_view_context_menu.show_ref_explorer_action,
      d->code_view_context_menu.set_entity_highlight,
      d->code_view_context_menu.remove_entity_highlight,
      d->code_view_context_menu.reset_entity_highlights};

  for (auto &action : action_list) {
    action->setData(action_data);
  }

  QVariant related_entity_id_var = index.data(ICodeModel::RelatedEntityIdRole);

  // Only enable the references browser if the token is related to an entity.
  d->code_view_context_menu.show_ref_explorer_action->setEnabled(
      related_entity_id_var.isValid());

  d->code_view_context_menu.menu->exec(QCursor::pos());
}

void MainWindow::OpenReferenceExplorer(
    const RawEntityId &entity_id,
    const IReferenceExplorerModel::ReferenceType &reference_type) {

  QPoint dialog_pos;
  if (d->quick_ref_explorer != nullptr) {
    dialog_pos = d->quick_ref_explorer->pos();

  } else {
    auto cursor_pos{QCursor::pos()};
    dialog_pos = {cursor_pos.x() - 20, cursor_pos.y() - 20};
  }

  auto opt_popup_placement = GetPopupPlacement();
  CloseAllPopups();

  d->quick_ref_explorer = std::make_unique<QuickReferenceExplorer>(
      d->index, d->file_location_cache, entity_id,
      d->enable_quick_ref_explorer_code_preview, *d->global_highlighter,
      *d->macro_explorer, reference_type, this);

  d->quick_ref_explorer->SetBrowserMode(d->toolbar.browser_mode->isChecked());

  connect(d->quick_ref_explorer.get(),
          &QuickReferenceExplorer::SaveReferenceExplorer, this,
          &MainWindow::SaveReferenceExplorer);

  connect(d->quick_ref_explorer.get(), &QuickReferenceExplorer::ItemActivated,
          this, &MainWindow::OnReferenceExplorerItemActivated);

  connect(this, &MainWindow::BrowserModeToggled, d->quick_ref_explorer.get(),
          &QuickReferenceExplorer::SetBrowserMode);

  if (opt_popup_placement.has_value()) {
    const auto &popup_placement = opt_popup_placement.value();

    d->quick_ref_explorer->move(popup_placement.x(), popup_placement.y());
    d->quick_ref_explorer->resize(popup_placement.width(),
                                  popup_placement.height());

  } else {
    auto cursor_pos{QCursor::pos()};
    d->quick_ref_explorer->move(cursor_pos.x() - 20, cursor_pos.y() - 20);

    auto margin = fontMetrics().height();
    auto max_width = margin + (width() / 3);
    auto max_height = margin + (height() / 4);

    d->quick_ref_explorer->resize(max_width, max_height);
  }

  d->quick_ref_explorer->show();
}

void MainWindow::OpenTokenReferenceExplorer(const QModelIndex &index) {

  QVariant related_entity_id_var =
      index.data(ICodeModel::RealRelatedEntityIdRole);

  if (!related_entity_id_var.isValid()) {
    CloseAllPopups();
    return;
  }

  OpenReferenceExplorer(qvariant_cast<RawEntityId>(related_entity_id_var),
                        IReferenceExplorerModel::ReferenceType::Callers);
}

void MainWindow::OpenTokenEntityInfo(const QModelIndex &index) {

  QVariant related_entity_id_var =
      index.data(ICodeModel::RealRelatedEntityIdRole);

  if (!related_entity_id_var.isValid()) {
    return;
  }

  OpenEntityInfo(qvariant_cast<RawEntityId>(related_entity_id_var));
}

void MainWindow::ExpandMacro(const QModelIndex &index) {
  if (auto macro_token_opt = ICodeModel::MacroExpansionPoint(index)) {
    d->macro_explorer->AddMacro(macro_token_opt->first,
                                macro_token_opt->second);
  }
}

std::optional<QRect> MainWindow::GetPopupPlacement() {
  auto opt_popup_placement =
      GetRestoredPopupPlacement(d->quick_code_view.get());

  if (!opt_popup_placement.has_value()) {
    opt_popup_placement =
        GetRestoredPopupPlacement(d->quick_ref_explorer.get());
  }

  return opt_popup_placement;
}

void MainWindow::OpenCodePreview(const QModelIndex &index) {
  auto opt_popup_placement = GetPopupPlacement();
  CloseAllPopups();

  QVariant related_entity_id_var =
      index.data(ICodeModel::RealRelatedEntityIdRole);

  if (!related_entity_id_var.isValid()) {
    return;
  }

  auto related_entity_id = qvariant_cast<RawEntityId>(related_entity_id_var);

  d->quick_code_view = std::make_unique<QuickCodeView>(
      d->index, d->file_location_cache, related_entity_id,
      *d->global_highlighter, *d->macro_explorer, this);

  d->quick_code_view->SetBrowserMode(d->toolbar.browser_mode->isChecked());

  connect(d->quick_code_view.get(), &QuickCodeView::TokenTriggered, this,
          &MainWindow::OnTokenTriggered);

  connect(this, &MainWindow::BrowserModeToggled, d->quick_code_view.get(),
          &QuickCodeView::SetBrowserMode);

  if (opt_popup_placement.has_value()) {
    const auto &popup_placement = opt_popup_placement.value();

    d->quick_code_view->move(popup_placement.x(), popup_placement.y());
    d->quick_code_view->resize(popup_placement.width(),
                               popup_placement.height());

  } else {
    auto cursor_pos{QCursor::pos()};
    d->quick_code_view->move(cursor_pos.x() - 20, cursor_pos.y() - 20);

    auto margin = fontMetrics().height();
    auto max_width = margin + (width() / 3);
    auto max_height = margin + (height() / 4);

    d->quick_code_view->resize(max_width, max_height);
  }

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

void MainWindow::CreateRefExplorerMenuOptions() {
  auto main_menu = new QMenu(tr("Reference Explorer"));
  d->view_menu->addMenu(main_menu);

  auto code_preview_action = new QAction(tr("Enable code preview"));
  code_preview_action->setCheckable(true);
  code_preview_action->setChecked(d->enable_quick_ref_explorer_code_preview);
  main_menu->addAction(code_preview_action);

  connect(code_preview_action, &QAction::toggled, this,
          &MainWindow::OnRefExplorerCodePreviewToggled);
}

ICodeView *
MainWindow::CreateNewCodeView(RawEntityId file_entity_id, QString tab_name,
                              const std::optional<QString> &opt_file_path) {

  ICodeModel *code_model = d->macro_explorer->CreateCodeModel(
      d->file_location_cache, d->index, this);

  QAbstractItemModel *proxy_model = d->global_highlighter->CreateModelProxy(
      code_model, kHighlightEntityIdRole);

  ICodeView *code_view = ICodeView::Create(proxy_model);
  code_view->SetBrowserMode(d->toolbar.browser_mode->isChecked());
  connect(this, &MainWindow::BrowserModeToggled, code_view,
          &ICodeView::SetBrowserMode);

  code_model->SetEntity(file_entity_id);

  code_view->SetWordWrapping(false);
  code_view->setAttribute(Qt::WA_DeleteOnClose);

  // Make it so that the initial last location for a given view is the file ID.
  code_view->setProperty(kLastLocationProperty,
                         static_cast<quint64>(file_entity_id));

  code_view->setProperty(kFileIdProperty, static_cast<quint64>(file_entity_id));

  auto *central_tab_widget = dynamic_cast<QTabWidget *>(centralWidget());

  auto tab_index = central_tab_widget->count();
  central_tab_widget->addTab(code_view, tab_name);

  if (opt_file_path.has_value()) {
    central_tab_widget->setTabToolTip(tab_index, opt_file_path.value());
  }

  connect(code_view, &ICodeView::TokenTriggered, this,
          &MainWindow::OnTokenTriggered);

  connect(code_view, &ICodeView::CursorMoved, this,
          &MainWindow::OnMainCodeViewCursorMoved);

  return code_view;
}

ICodeView *MainWindow::GetOrCreateFileCodeView(
    RawEntityId file_id, std::optional<QString> opt_tab_name,
    const std::optional<QString> &opt_file_path) {

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
    return CreateNewCodeView(file_id, opt_tab_name.value(), opt_file_path);
  }

  for (auto [path, id] : d->index.file_paths()) {
    if (id.Pack() != file_id) {
      continue;
    }

    QString tab_name = QString::fromStdString(path.filename().generic_string());
    QString file_path = QString::fromStdString(path.generic_string());
    return CreateNewCodeView(file_id, tab_name, file_path);
  }

  return nullptr;
}

void MainWindow::OpenEntityRelatedToToken(const QModelIndex &index) {
  auto entity_id_var = index.data(ICodeModel::RelatedEntityIdRole);
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
  code_view->setProperty(kLastLocationProperty,
                         static_cast<quint64>(entity_id));
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

void MainWindow::OnProjectExplorerFileClicked(RawEntityId file_id,
                                              QString tab_name,
                                              const QString &file_path) {
  CloseAllPopups();
  OpenEntityInfo(file_id);

  auto *central_tab_widget = dynamic_cast<QTabWidget *>(centralWidget());
  QWidget *current_tab = central_tab_widget->currentWidget();
  QWidget *next_tab = GetOrCreateFileCodeView(file_id, tab_name, file_path);
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

void MainWindow::OnMainCodeViewCursorMoved(const QModelIndex &index) {
  QVariant entity_id_var = index.data(ICodeModel::TokenIdRole);
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

void MainWindow::OnTokenTriggered(const ICodeView::TokenAction &token_action,
                                  const QModelIndex &index) {

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

    if (keyboard_button.key == Qt::Key_X) {
      // Like in IDA Pro, pressing X while the cursor is on an entity shows us
      // its cross-references.

      OpenTokenReferenceExplorer(index);

    } else if (keyboard_button.key == Qt::Key_P) {
      OpenCodePreview(index);

    } else if (keyboard_button.key == Qt::Key_I) {
      d->info_explorer_dock->show();
      OpenTokenEntityInfo(index);

    } else if (keyboard_button.key == Qt::Key_E) {
      ExpandMacro(index);

    } else if (keyboard_button.key == Qt::Key_Enter) {
      // Like in IDA Pro, pressing Enter while the cursor is on a use of that
      // entity will bring us to that entity.
      OpenEntityRelatedToToken(index);
    }
  }
}

void MainWindow::OnOpenEntity(RawEntityId entity_id) {
  CloseAllPopups();
  d->toolbar.back_forward->CommitCurrentLocationToHistory();
  OpenEntityInfo(entity_id);
  OpenEntityCode(entity_id, false /* don't canonicalize entity IDs */);
}

void MainWindow::OnHistoryNavigationEntitySelected(RawEntityId,
                                                   RawEntityId canonical_id) {
  CloseAllPopups();
  // OpenEntityInfo(canonical_id);
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

  OpenEntityCode(entity_id);
}

void MainWindow::OnCodeViewContextMenuActionTriggered(QAction *action) {
  QVariant code_model_index_var = action->data();
  if (!code_model_index_var.isValid()) {
    return;
  }

  QModelIndex code_model_index =
      qvariant_cast<QModelIndex>(code_model_index_var);

  if (action == d->code_view_context_menu.show_ref_explorer_action) {
    OpenTokenReferenceExplorer(code_model_index);

  } else if (action == d->code_view_context_menu.set_entity_highlight) {
    QVariant entity_id_var = code_model_index.data(kHighlightEntityIdRole);
    if (!entity_id_var.isValid()) {
      return;
    }

    RawEntityId entity_id = qvariant_cast<RawEntityId>(entity_id_var);
    QColor color = QColorDialog::getColor();
    if (!color.isValid()) {
      return;
    }

    d->global_highlighter->SetEntityColor(entity_id, color);

  } else if (action == d->code_view_context_menu.remove_entity_highlight) {
    QVariant entity_id_var = code_model_index.data(kHighlightEntityIdRole);
    if (!entity_id_var.isValid()) {
      return;
    }

    RawEntityId entity_id = qvariant_cast<RawEntityId>(entity_id_var);
    d->global_highlighter->RemoveEntity(entity_id);

  } else if (action == d->code_view_context_menu.reset_entity_highlights) {
    d->global_highlighter->Clear();
  }
}

void MainWindow::OnToggleWordWrap(bool checked) {
  auto &tab_widget = *static_cast<QTabWidget *>(centralWidget());
  auto &code_view = *static_cast<ICodeView *>(tab_widget.widget(0));
  code_view.SetWordWrapping(checked);
}

void MainWindow::SaveReferenceExplorer(
    PreviewableReferenceExplorer *reference_explorer) {
  d->quick_ref_explorer.release();

  auto new_tab_index = d->ref_explorer_tab_widget->count();

  reference_explorer->setParent(this);
  reference_explorer->setAttribute(Qt::WA_DeleteOnClose);
  reference_explorer->SetBrowserMode(d->toolbar.browser_mode->isChecked());

  connect(reference_explorer, &PreviewableReferenceExplorer::ItemActivated,
          this, &MainWindow::OnReferenceExplorerItemActivated);

  connect(reference_explorer, &PreviewableReferenceExplorer::TokenTriggered,
          this, &MainWindow::OnTokenTriggered);

  connect(this, &MainWindow::BrowserModeToggled, reference_explorer,
          &PreviewableReferenceExplorer::SetBrowserMode);

  d->ref_explorer_tab_widget->addTab(reference_explorer,
                                     reference_explorer->windowTitle());
  d->ref_explorer_tab_widget->setCurrentIndex(new_tab_index);

  d->reference_explorer_dock->toggleViewAction()->setEnabled(true);
  d->reference_explorer_dock->show();
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

void MainWindow::OnSetDarkTheme() {
  mx::gui::IThemeManager::Get().SetTheme(true);
}

void MainWindow::OnSetLightTheme() {
  mx::gui::IThemeManager::Get().SetTheme(false);
}

void MainWindow::OnRefExplorerCodePreviewToggled(const bool &checked) {
  d->enable_quick_ref_explorer_code_preview = checked;
}

void MainWindow::OnThemeChange(const QPalette &, const CodeViewTheme &) {
  UpdateIcons();
}

void MainWindow::UpdateIcons() {
  auto is_checked = d->toolbar.browser_mode->isChecked();
  auto browser_mode_icon =
      GetIcon(":/Icons/ToolBar/BrowserMode",
              is_checked ? IconStyle::Highlighted : IconStyle::None);

  d->toolbar.browser_mode->setIcon(browser_mode_icon);
}

void MainWindow::OnBrowserModeToggled() {
  UpdateIcons();
  emit BrowserModeToggled(d->toolbar.browser_mode->isChecked());
}

}  // namespace mx::gui
