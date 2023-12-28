// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MainWindow.h"

#include <multiplier/GUI/CallHierarchyPlugin.h>
#include <multiplier/GUI/Context.h>
#include <multiplier/GUI/InformationExplorer.h>
#include <multiplier/GUI/ReferenceExplorer.h>

#include <multiplier/GUI/SimpleTextInputDialog.h>
// #include <multiplier/GUI/InformationExplorerWidget.h>
#include <multiplier/GUI/TabWidget.h>
#include <multiplier/GUI/CodeWidget.h>
#include <multiplier/GUI/PopupWidgetContainer.h>
#include <multiplier/GUI/DockWidgetContainer.h>
#include <multiplier/GUI/Assert.h>
#include <multiplier/GUI/CodeViewTheme.h>
#include <multiplier/GUI/HistoryWidget.h>
#include <multiplier/GUI/Icons.h>
#include <multiplier/GUI/IEntityExplorer.h>
#include <multiplier/GUI/IGlobalHighlighter.h>
#include <multiplier/GUI/IMacroExplorer.h>
#include <multiplier/GUI/IProjectExplorer.h>
#include <multiplier/GUI/IThemeManager.h>
#include <multiplier/GUI/IGeneratorModel.h>
#include <multiplier/GUI/Util.h>
#include <multiplier/GUI/IGeneratorView.h>

#include <multiplier/GUI/ActionRegistry.h>

#ifndef MX_DISABLE_PYTHON_BINDINGS
# include <multiplier/GUI/PythonConsoleWidget.h>
#endif

#include <multiplier/AST/StmtKind.h>

#include <QKeyCombination>
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
#include <QTextEdit>
#include <QMessageBox>

namespace mx::gui {

class PythonConsoleWidget;

namespace {

using CodeWidgetPopup = PopupWidgetContainer<CodeWidget>;

using DockableCodeWidget = DockWidgetContainer<CodeWidget>;
// using DockableInformationExplorer =
//     DockWidgetContainer<InformationExplorerWidget>;

const std::size_t kMaxHistorySize{30};

//! The file id associated with a code view.
static const char *kFileIdProperty = "mx:fileId";

//! The last cursor location in a code view.
static const char *kLastLocationProperty = "mx:lastLocation";

static const auto kHighlightEntityIdRole = ICodeModel::RealRelatedEntityIdRole;

struct CodeViewContextMenu final {
  QMenu *menu{nullptr};
  QAction *show_call_hierarchy{nullptr};

  QMenu *highlight_menu{nullptr};
  QAction *set_entity_highlight{nullptr};
  QAction *remove_entity_highlight{nullptr};
  QAction *reset_entity_highlights{nullptr};
};

struct ToolBar final {
  HistoryWidget *back_forward{nullptr};
  QAction *browser_mode{nullptr};
};

}  // namespace

struct MainWindow::PrivateData final {
  const Context &context;

  const mx::Index index;  // TODO(pag): Remove.
  const mx::FileLocationCache file_location_cache;  // TODO(pag): Remove.

  TriggerHandle open_entity;

  std::vector<std::unique_ptr<IMainWindowPlugin>> plugins;

  IProjectExplorer *project_explorer{nullptr};
  IEntityExplorer *entity_explorer{nullptr};
  IMacroExplorer *macro_explorer{nullptr};
  PythonConsoleWidget *python_console{nullptr};
  CodeViewContextMenu code_view_context_menu;

  std::vector<QWidget *> popup_widget_list;
  QAction *enable_code_preview_action{nullptr};

  QShortcut *close_active_ref_explorer_tab_shortcut{nullptr};
  QShortcut *close_active_code_tab_shortcut{nullptr};

  // bool enable_ref_explorer_code_preview{false};
  // QTabWidget *ref_explorer_tab_widget{nullptr};
  // QDockWidget *reference_explorer_dock{nullptr};

  QDockWidget *project_explorer_dock{nullptr};
  QDockWidget *entity_explorer_dock{nullptr};
  QDockWidget *macro_explorer_dock{nullptr};
  QDockWidget *highlight_explorer_dock{nullptr};
  QDockWidget *python_console_dock{nullptr};

  // DockableInformationExplorer *info_explorer_dock{nullptr};

  IGlobalHighlighter *global_highlighter{nullptr};

  QMenu *view_menu{nullptr};
  ToolBar toolbar;

  inline PrivateData(const Context &context_, MainWindow *self)
      : context(context_),
        index(context.Index()),
        file_location_cache(context.FileLocationCache()),
        open_entity(context.ActionRegistry().Register(
            self, "com.trailofbits.action.OpenEntity",
            [=] (const QVariant &data) {
              if (data.canConvert<VariantEntity>()) {
                self->OnOpenEntity(EntityId(data.value<VariantEntity>()).Pack());
              }
            })) {}
};

MainWindow::MainWindow(const Context &context)
    : QMainWindow(nullptr),
      d(new PrivateData(context, this)) {

  setWindowTitle("Multiplier");
  setWindowIcon(QIcon(":/Icons/Multiplier"));

  // RegisterReferenceExplorerActions();

  InitializeWidgets();
  InitializeToolBar();
  InitializePlugins();

  connect(&(context.ThemeManager()), &IThemeManager::ThemeChanged,
          this, &MainWindow::OnThemeChange);

  resize(1280, 800);
}

MainWindow::~MainWindow() {}

// void MainWindow::RegisterReferenceExplorerActions() {
//   static const auto L_entityIdFromVariant =
//       [](Index index, const QVariant &variant) -> std::optional<RawEntityId> {
//     if (!variant.isValid()) {
//       return std::nullopt;
//     }

//     RawEntityId entity_id{kInvalidEntityId};
//     if (variant.canConvert<std::uint64_t>()) {
//       entity_id = static_cast<RawEntityId>(variant.toULongLong());

//     } else if (variant.canConvert<RawEntityId>()) {
//       entity_id = qvariant_cast<RawEntityId>(variant);

//     } else if (variant.canConvert<QString>()) {
//       auto string_value = variant.toString().trimmed();

//       bool succeeded{false};
//       entity_id =
//           static_cast<RawEntityId>(string_value.toULongLong(&succeeded));

//       if (!succeeded) {
//         return std::nullopt;
//       }
//     }

//     auto entity = index.entity(entity_id);
//     if (std::holds_alternative<NotAnEntity>(entity)) {
//       return std::nullopt;
//     }

//     return entity_id;
//   };

//   static const IActionRegistry::Action reference_explorer_action{
//       tr("Call Hierarchy"),
//       "OpenCallHierarchy",

//       {IActionRegistry::Action::InputType::Integer,
//        IActionRegistry::Action::InputType::String,
//        IActionRegistry::Action::InputType::EntityIdentifier},

//       [](Index index, const QVariant &input) -> bool {
//         auto opt_entity_id = L_entityIdFromVariant(index, input);
//         return opt_entity_id.has_value();
//       },

//       [this](Index index, const QVariant &input, QWidget *parent) -> bool {
//         auto opt_entity_id = L_entityIdFromVariant(index, input);
//         if (!opt_entity_id.has_value()) {
//           return false;
//         }

//         auto generator = CallHierarchyGenerator::Create(
//             index, d->file_location_cache, opt_entity_id.value());

//         auto ref_explorer = new PopupReferenceExplorer(
//             index, d->file_location_cache, std::move(generator),
//             d->enable_ref_explorer_code_preview, *d->global_highlighter,
//             *d->macro_explorer, parent);

//         d->popup_widget_list.push_back(ref_explorer);

//         ref_explorer->GetWrappedWidget()->SetBrowserMode(
//             d->toolbar.browser_mode->isChecked());

//         ref_explorer->show();
//         return true;
//       },
//   };

//   d->action_registry->Register(reference_explorer_action);
// }

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

  auto help_menu = new QMenu(tr("Help"));
  menuBar()->addMenu(help_menu);

  auto about_action = new QAction(tr("About"));
  help_menu->addAction(about_action);

  connect(about_action, &QAction::triggered, this, [this](const bool &) {
    QMessageBox::about(this, windowTitle(),
                       QString("User interface: ") + QTMULTIPLIER_VERSION +
                           "\n" + "Library: " + MULTIPLIER_VERSION);
  });

  setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::West);
  setTabPosition(Qt::RightDockWidgetArea, QTabWidget::East);
  setTabPosition(Qt::TopDockWidgetArea, QTabWidget::North);
  setTabPosition(Qt::BottomDockWidgetArea, QTabWidget::North);

  CreateGlobalHighlighter();
  CreateMacroExplorerDock();
  // CreateReferenceExplorerMenuOptions();
  CreateProjectExplorerDock();
  CreateEntityExplorerDock();
  // CreateInfoExplorerDock();
  CreateCodeView();
  // CreateReferenceExplorerDock();
  CreatePythonConsoleDock();

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

void MainWindow::InitializePlugins(void) {
  auto ref_explorer_plugin = d->plugins.emplace_back(
      CreateReferenceExplorerMainWindowPlugin(d->context, this)).get();

  d->plugins.emplace_back(CreateInformationExplorerMainWindowPlugin(
      d->context, this));

  for (const auto &plugin : d->plugins) {
    QWidget *dockable_widget = plugin->CreateDockWidget(this);
    if (!dockable_widget) {
      continue;
    }

    auto dock = new QDockWidget(dockable_widget->windowTitle(), this);
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock->setWidget(dockable_widget);

    d->view_menu->addAction(dock->toggleViewAction());
    addDockWidget(Qt::LeftDockWidgetArea, dock);

    connect(
        plugin.get(), &IMainWindowPlugin::ShowDockWidget,
        [=] (void) {
          dock->show();
        });

    connect(
        plugin.get(), &IMainWindowPlugin::HideDockWidget,
        [=] (void) {
          dock->hide();
        });

    connect(
        plugin.get(), &IMainWindowPlugin::PopupOpened,
        [this] (QWidget *popup) {
          popup->installEventFilter(this);
          d->popup_widget_list.push_back(popup);
        });
  }

  CallHierarchyPlugin::Register(
      ref_explorer_plugin,
      [this] (IMainWindowPlugin *parent) {
        return new CallHierarchyPlugin(d->context, parent);
      });
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

void MainWindow::CreatePythonConsoleDock(void) {
#ifndef MX_DISABLE_PYTHON_BINDINGS
  d->python_console = new PythonConsoleWidget(d->index, this);

  d->python_console_dock = new QDockWidget(tr("Python Console"), this);
  d->python_console_dock->setAllowedAreas(Qt::TopDockWidgetArea |
                                          Qt::BottomDockWidgetArea);
  d->view_menu->addAction(d->python_console_dock->toggleViewAction());
  d->python_console_dock->setWidget(d->python_console);
  addDockWidget(Qt::BottomDockWidgetArea, d->python_console_dock);

  // Default is hidden until we ask to see the console.
  d->python_console_dock->hide();
#endif
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
  // d->info_explorer_dock = new DockableInformationExplorer(
  //     d->index, d->file_location_cache, d->global_highlighter, true, this);

  // connect(d->info_explorer_dock->GetWrappedWidget(),
  //         &InformationExplorerWidget::SelectedItemChanged, this,
  //         &MainWindow::OnInformationExplorerSelectionChange);

  // d->view_menu->addAction(d->info_explorer_dock->toggleViewAction());
  // addDockWidget(Qt::LeftDockWidgetArea, d->info_explorer_dock);
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

// void MainWindow::CreateReferenceExplorerDock() {
//   d->ref_explorer_tab_widget = new TabWidget(this);
//   d->ref_explorer_tab_widget->setDocumentMode(true);
//   d->ref_explorer_tab_widget->setTabsClosable(true);

//   d->close_active_ref_explorer_tab_shortcut =
//       new QShortcut(QKeySequence::Close, d->ref_explorer_tab_widget, this,
//                     &MainWindow::OnCloseActiveReferenceExplorerTab,
//                     Qt::WidgetWithChildrenShortcut);

//   connect(d->ref_explorer_tab_widget->tabBar(), &QTabBar::tabCloseRequested,
//           this, &MainWindow::OnReferenceExplorerTabBarClose);

//   connect(d->ref_explorer_tab_widget->tabBar(), &QTabBar::tabBarDoubleClicked,
//           this, &MainWindow::OnReferenceExplorerTabBarDoubleClick);

//   d->reference_explorer_dock = new QDockWidget(tr("Reference Explorer"), this);
//   d->view_menu->addAction(d->reference_explorer_dock->toggleViewAction());
//   d->reference_explorer_dock->toggleViewAction()->setEnabled(false);
//   d->reference_explorer_dock->setAllowedAreas(Qt::AllDockWidgetAreas);
//   d->reference_explorer_dock->setWidget(d->ref_explorer_tab_widget);
//   addDockWidget(Qt::BottomDockWidgetArea, d->reference_explorer_dock);

//   // Default is hidden until we ask to see the references to something.
//   d->reference_explorer_dock->hide();
// }

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
  auto tab_widget = new TabWidget();
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

  // // Also create the custom context menu
  // d->code_view_context_menu.menu = new QMenu(tr("Token menu"));

  // connect(d->code_view_context_menu.menu, &QMenu::triggered, this,
  //         &MainWindow::OnCodeViewContextMenuActionTriggered);

  // d->code_view_context_menu.show_call_hierarchy =
  //     new QAction(tr("Show Call Hierarchy"));

  // d->code_view_context_menu.menu->addAction(
  //     d->code_view_context_menu.show_call_hierarchy);

  // d->code_view_context_menu.highlight_menu = new QMenu(tr("Highlights"));
  // d->code_view_context_menu.menu->addMenu(
  //     d->code_view_context_menu.highlight_menu);

  // d->code_view_context_menu.set_entity_highlight = new QAction(tr("Set color"));

  // d->code_view_context_menu.highlight_menu->addAction(
  //     d->code_view_context_menu.set_entity_highlight);

  // d->code_view_context_menu.remove_entity_highlight = new QAction(tr("Remove"));

  // d->code_view_context_menu.highlight_menu->addAction(
  //     d->code_view_context_menu.remove_entity_highlight);

  // d->code_view_context_menu.reset_entity_highlights = new QAction(tr("Reset"));

  // d->code_view_context_menu.highlight_menu->addAction(
  //     d->code_view_context_menu.reset_entity_highlights);
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
  (void) index;
  // connect(d->code_view_context_menu.menu, &QMenu::triggered, this,
  //         &MainWindow::OnCodeViewContextMenuActionTriggered);

  // QVariant action_data;
  // action_data.setValue(index);

  // std::vector<QAction *> action_list{
  //     d->code_view_context_menu.show_call_hierarchy,
  //     d->code_view_context_menu.set_entity_highlight,
  //     d->code_view_context_menu.remove_entity_highlight,
  //     d->code_view_context_menu.reset_entity_highlights};

  // for (auto &action : action_list) {
  //   action->setData(action_data);
  // }

  // QVariant related_entity_id_var = index.data(ICodeModel::RelatedEntityIdRole);

  // // Only enable the references browser if the token is related to an entity.
  // d->code_view_context_menu.show_call_hierarchy->setEnabled(
  //     related_entity_id_var.isValid());

  // d->code_view_context_menu.menu->exec(QCursor::pos());
}

void MainWindow::OpenCallHierarchy(const QModelIndex &index) {
  (void) index;
  // d->action_registry->Execute("OpenCallHierarchy",
  //                             index.data(ICodeModel::RealRelatedEntityIdRole),
  //                             this);
}

void MainWindow::OpenTokenEntityInfo(const QModelIndex &index,
                                     const bool &new_window) {

  QVariant related_entity_id_var =
      index.data(ICodeModel::RealRelatedEntityIdRole);

  if (!related_entity_id_var.isValid()) {
    return;
  }

  auto entity_id = qvariant_cast<RawEntityId>(related_entity_id_var);

  if (new_window) {
    // auto info_explorer_dock = new DockableInformationExplorer(
    //     d->index, d->file_location_cache, d->global_highlighter, false, this);

    // connect(info_explorer_dock->GetWrappedWidget(),
    //         &InformationExplorerWidget::SelectedItemChanged, this,
    //         &MainWindow::OnInformationExplorerSelectionChange);

    // info_explorer_dock->GetWrappedWidget()->DisplayEntity(entity_id);
    // info_explorer_dock->GetWrappedWidget()->setAttribute(Qt::WA_DeleteOnClose);
    // info_explorer_dock->GetWrappedWidget()->show();

    // addDockWidget(Qt::RightDockWidgetArea, info_explorer_dock);

  } else {
    OpenEntityInfo(entity_id);
  }
}

void MainWindow::ExpandMacro(const QModelIndex &index) {
  if (auto macro_token_opt = ICodeModel::MacroExpansionPoint(index)) {
    d->macro_explorer->AddMacro(macro_token_opt->first,
                                macro_token_opt->second);
  }
}

void MainWindow::OpenCodePreview(const QModelIndex &index,
                                 const bool &as_new_window) {
  QVariant related_entity_id_var =
      index.data(ICodeModel::RealRelatedEntityIdRole);

  if (!related_entity_id_var.isValid()) {
    return;
  }

  auto related_entity_id = qvariant_cast<RawEntityId>(related_entity_id_var);

  CodeWidget *code_widget{nullptr};

  if (!as_new_window) {
    auto code_widget_popup =
        new CodeWidgetPopup(d->index, d->file_location_cache, related_entity_id,
                            *d->global_highlighter, *d->macro_explorer, this);

    d->popup_widget_list.push_back(code_widget_popup);

    auto cursor_pos{QCursor::pos()};
    code_widget_popup->move(cursor_pos.x() - 20, cursor_pos.y() - 20);

    auto margin = fontMetrics().height();
    auto max_width = margin + (width() / 3);
    auto max_height = margin + (height() / 4);

    code_widget_popup->resize(max_width, max_height);

    code_widget_popup->show();

    code_widget = code_widget_popup->GetWrappedWidget();

  } else {
    auto code_widget_dock = new DockableCodeWidget(
        d->index, d->file_location_cache, related_entity_id,
        *d->global_highlighter, *d->macro_explorer, this);

    code_widget_dock->setParent(this);
    addDockWidget(Qt::RightDockWidgetArea, code_widget_dock);

    code_widget = code_widget_dock->GetWrappedWidget();
  }

  code_widget->SetBrowserMode(d->toolbar.browser_mode->isChecked());

  connect(code_widget, &CodeWidget::TokenTriggered, this,
          &MainWindow::OnTokenTriggered);

  connect(this, &MainWindow::BrowserModeToggled, code_widget,
          &CodeWidget::SetBrowserMode);
}

void MainWindow::CloseAllPopups() {
  for (auto &popup_widget : d->popup_widget_list) {
    popup_widget->close();
    popup_widget->deleteLater();
  }

  d->popup_widget_list.clear();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::Close) {
    (void) std::erase(d->popup_widget_list, obj);
  }

  return QMainWindow::eventFilter(obj, event);
}

// void MainWindow::CreateReferenceExplorerMenuOptions() {
//   auto main_menu = new QMenu(tr("Reference Explorer"));
//   d->view_menu->addMenu(main_menu);

//   auto code_preview_action = new QAction(tr("Enable code preview"));
//   code_preview_action->setCheckable(true);
//   code_preview_action->setChecked(d->enable_ref_explorer_code_preview);
//   main_menu->addAction(code_preview_action);

//   connect(code_preview_action, &QAction::toggled, this,
//           &MainWindow::OnReferenceExplorerCodePreviewToggled);
// }

ICodeView *
MainWindow::CreateNewCodeView(RawEntityId file_entity_id, QString tab_name,
                              const std::optional<QString> &opt_file_path) {

  ICodeModel *code_model = d->macro_explorer->CreateCodeModel(
      d->file_location_cache, d->index, false, this);

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

void MainWindow::SetHere(RawEntityId eid) {
#ifndef MX_DISABLE_PYTHON_BINDINGS
  if (d->python_console) {
    d->python_console->SetHere(d->index.entity(eid));
  }
#endif
}

void MainWindow::SetHere(const QModelIndex &index) {
  auto entity_id_var = index.data(ICodeModel::RelatedEntityIdRole);
  if (entity_id_var.isValid()) {
    SetHere(qvariant_cast<RawEntityId>(entity_id_var));
    return;
  }

  entity_id_var = index.data(ICodeModel::TokenIdRole);
  if (entity_id_var.isValid()) {
    SetHere(qvariant_cast<RawEntityId>(entity_id_var));
    return;
  }

  // TODO(pag): Should have a way of getting a `Token` from a `QModelIndex`
  //            because stuff from the `TokenTree` might not have IDs but are
  //            live/living objects.

  SetHere(kInvalidEntityId);
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
  (void) entity_id;
  // d->info_explorer_dock->show();
  // d->info_explorer_dock->GetWrappedWidget()->DisplayEntity(entity_id);
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

  SetHere(entity_id);
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

  SetHere(index);
}

void MainWindow::OnTokenTriggered(const ICodeView::TokenAction &token_action,
                                  const QModelIndex &index) {

  if (token_action.type == ICodeView::TokenAction::Type::Primary) {
    for (const auto &plugin : d->plugins) {
      plugin->ActOnPrimaryClick(index);
    }

  } else if (token_action.type == ICodeView::TokenAction::Type::Secondary) {
    QMenu token_menu(tr("Token menu"));
    for (const auto &plugin : d->plugins) {
      plugin->ActOnContextMenu(&token_menu, index);
    }
    token_menu.exec(QCursor::pos());

  } else if (token_action.type == ICodeView::TokenAction::Type::Hover) {
    for (const auto &plugin : d->plugins) {
      plugin->ActOnLongHover(index);
    }

    // if (d->enable_code_preview_action->isChecked()) {
    //   OpenCodePreview(index, false /* as_new_window */);
    // }

  } else if (token_action.type == ICodeView::TokenAction::Type::Keyboard) {
    // Here to test keyboard events; Before we add more buttons, we should
    // find a better strategy to managed them.
    //
    // Ideally we should find a Qt-friendly method that the framework handles
    // well natively
    const auto &keyboard_button = token_action.opt_keyboard_button.value();
    if (keyboard_button.control_modifier) {
      return;
    }

    if (!keyboard_button.key) {
      return;
    }

    QKeySequence key_seq(QKeyCombination(
        keyboard_button.shift_modifier ? Qt::ShiftModifier : Qt::NoModifier,
        static_cast<Qt::Key>(keyboard_button.key)));

    std::vector<NamedAction> actions;
    for (const auto &plugin : d->plugins) {
      auto plugin_actions = plugin->ActOnKeyPressEx(key_seq, index);
      actions.insert(actions.end(),
                     std::make_move_iterator(plugin_actions.begin()),
                     std::make_move_iterator(plugin_actions.end()));
    }

    if (actions.empty()) {
      return;
    }

    if (actions.size() == 1u) {
      actions[0].action.Trigger(actions[0].data);
      return;
    }

    QMenu token_menu(tr("Token menu"));
    for (auto &plugin_action : actions) {
      auto action = new QAction(plugin_action.name, &token_menu);
      connect(
          action, &QAction::triggered,
          [trigger = std::move(plugin_action.action),
           data = std::move(plugin_action.data)] (void) {
            trigger.Trigger(data);
          });
      token_menu.addAction(action);
    }
    token_menu.exec(QCursor::pos());

    // if (keyboard_button.key == Qt::Key_X) {
    //   // Like in IDA Pro, pressing X while the cursor is on an entity shows us
    //   // its cross-references.
    //   OpenCallHierarchy(index);

    // } else if (keyboard_button.key == Qt::Key_P) {
    //   const auto &as_new_window{keyboard_button.shift_modifier};
    //   OpenCodePreview(index, as_new_window);

    // } else if (keyboard_button.key == Qt::Key_I) {
    //   const auto &as_new_window{keyboard_button.shift_modifier};
    //   OpenTokenEntityInfo(index, as_new_window);

    // } else if (keyboard_button.key == Qt::Key_E) {
    //   ExpandMacro(index);

    // } else if (keyboard_button.key == Qt::Key_Enter) {
    //   // Like in IDA Pro, pressing Enter while the cursor is on a use of that
    //   // entity will bring us to that entity.
    //   OpenEntityRelatedToToken(index);
    // }
  }
}

// TODO(pag): Migrate to `VariantEntity`.
void MainWindow::OnOpenEntity(RawEntityId entity_id) {
  CloseAllPopups();
  d->toolbar.back_forward->CommitCurrentLocationToHistory();
  OpenEntityInfo(entity_id);
  OpenEntityCode(entity_id, false /* don't canonicalize entity IDs */);
}

void MainWindow::OnHistoryNavigationEntitySelected(RawEntityId,
                                                   RawEntityId canonical_id) {
  CloseAllPopups();
  OpenEntityCode(canonical_id);
}

void MainWindow::OnInformationExplorerSelectionChange(
    const QModelIndex &index) {
  (void) index;
  // auto entity_id_var = index.data(IInformationExplorerModel::EntityIdRole);
  // if (!entity_id_var.isValid()) {
  //   return;
  // }

  // auto entity_id = qvariant_cast<RawEntityId>(entity_id_var);

  // d->toolbar.back_forward->CommitCurrentLocationToHistory();
  // OpenEntityCode(entity_id, false /* don't canonicalize entity IDs */);
}

// void MainWindow::OnReferenceExplorerItemActivated(const QModelIndex &index) {
//   auto entity_id_role = index.data(IGeneratorModel::EntityIdRole);
//   if (!entity_id_role.isValid()) {
//     return;
//   }

//   auto entity_id = qvariant_cast<RawEntityId>(entity_id_role);
//   d->toolbar.back_forward->CommitCurrentLocationToHistory();

//   OpenEntityCode(entity_id);
// }

void MainWindow::OnCodeViewContextMenuActionTriggered(QAction *action) {
  (void) action;
  // QVariant code_model_index_var = action->data();
  // if (!code_model_index_var.isValid()) {
  //   return;
  // }

  // QModelIndex code_model_index =
  //     qvariant_cast<QModelIndex>(code_model_index_var);

  // if (action == d->code_view_context_menu.show_call_hierarchy) {
  //   OpenCallHierarchy(code_model_index);

  // } else if (action == d->code_view_context_menu.set_entity_highlight) {
  //   QVariant entity_id_var = code_model_index.data(kHighlightEntityIdRole);
  //   if (!entity_id_var.isValid()) {
  //     return;
  //   }

  //   RawEntityId entity_id = qvariant_cast<RawEntityId>(entity_id_var);
  //   QColor color = QColorDialog::getColor();
  //   if (!color.isValid()) {
  //     return;
  //   }

  //   d->global_highlighter->SetEntityColor(entity_id, color);

  // } else if (action == d->code_view_context_menu.remove_entity_highlight) {
  //   QVariant entity_id_var = code_model_index.data(kHighlightEntityIdRole);
  //   if (!entity_id_var.isValid()) {
  //     return;
  //   }

  //   RawEntityId entity_id = qvariant_cast<RawEntityId>(entity_id_var);
  //   d->global_highlighter->RemoveEntity(entity_id);

  // } else if (action == d->code_view_context_menu.reset_entity_highlights) {
  //   d->global_highlighter->Clear();
  // }
}

void MainWindow::OnToggleWordWrap(bool checked) {
  auto &tab_widget = *static_cast<QTabWidget *>(centralWidget());
  auto &code_view = *static_cast<ICodeView *>(tab_widget.widget(0));
  code_view.SetWordWrapping(checked);
}

// void MainWindow::SaveReferenceExplorer(ReferenceExplorer *) {
//   /*d->popup_reference_explorer.release();

//   auto new_tab_index = d->ref_explorer_tab_widget->count();

//   reference_explorer->setParent(this);
//   reference_explorer->setAttribute(Qt::WA_DeleteOnClose);
//   reference_explorer->SetBrowserMode(d->toolbar.browser_mode->isChecked());

//   connect(reference_explorer, &ReferenceExplorer::ItemActivated, this,
//           &MainWindow::OnReferenceExplorerItemActivated);

//   connect(reference_explorer, &ReferenceExplorer::TokenTriggered, this,
//           &MainWindow::OnTokenTriggered);

//   connect(this, &MainWindow::BrowserModeToggled, reference_explorer,
//           &ReferenceExplorer::SetBrowserMode);

//   d->ref_explorer_tab_widget->addTab(reference_explorer,
//                                      reference_explorer->windowTitle());
//   d->ref_explorer_tab_widget->setCurrentIndex(new_tab_index);

//   d->reference_explorer_dock->toggleViewAction()->setEnabled(true);
//   d->reference_explorer_dock->show();*/
// }

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

// void MainWindow::OnCloseActiveReferenceExplorerTab() {
//   if (d->ref_explorer_tab_widget->count() == 0) {
//     return;
//   }

//   auto current_index = d->ref_explorer_tab_widget->currentIndex();
//   OnReferenceExplorerTabBarClose(current_index);

//   d->ref_explorer_tab_widget->setFocus();
// }

void MainWindow::OnSetDarkTheme() {
  mx::gui::IThemeManager::Get().SetTheme(true);
}

void MainWindow::OnSetLightTheme() {
  mx::gui::IThemeManager::Get().SetTheme(false);
}

// void MainWindow::OnReferenceExplorerCodePreviewToggled(const bool &checked) {
//   d->enable_ref_explorer_code_preview = checked;
// }

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
