// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "WindowManager.h"
#include "MainWindow.h"

#include <multiplier/GUI/Interfaces/IWindowWidget.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Widgets/SimpleTextInputDialog.h>
#include <multiplier/GUI/Widgets/TabWidget.h>

#include <DockManager.h>
#include <DockAreaWidget.h>
#include <DockWidget.h>

#include <QDesktopServices>
#include <QDockWidget>
#include <QIcon>
#include <QLabel>
#include <QMap>
#include <QMenu>
#include <QMenuBar>
#include <QTabBar>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QClipboard>
#include <QAction>

#include <unordered_map>
#include <filesystem>

namespace mx::gui {
namespace {

// Convert out dock location to the ads one.
static  Qt::DockWidgetArea ConvertLocation(IWindowManager::DockLocation loc) {
  switch (loc) {
    case IWindowManager::DockLocation::Left: return Qt::LeftDockWidgetArea;
    case IWindowManager::DockLocation::Right: return Qt::RightDockWidgetArea;
    case IWindowManager::DockLocation::Top: return Qt::TopDockWidgetArea;
    case IWindowManager::DockLocation::Bottom: return Qt::BottomDockWidgetArea;
  }
}

}  // namespace

struct WindowManager::PrivateData {
  MainWindow * const window;
  QToolBar * toolbar{nullptr};
  ads::CDockManager * central_widget{nullptr};

  std::unordered_map<QDockWidget *, DockConfig> dock_configs;

  QMap<QString, QMenu *> app_menus;

  inline PrivateData(MainWindow *parent)
      : window(parent) {}
};

WindowManager::~WindowManager(void) {}

WindowManager::WindowManager(MainWindow *window)
    : IWindowManager(window),
      d(new PrivateData(window)) {

  // The `CDockManager` will automatically set itself as the
  // central widget in our `QMainWindow`-based class
  d->central_widget = new ads::CDockManager(window);
  d->central_widget->setConfigFlag(ads::CDockManager::EqualSplitOnInsertion, true);
  d->central_widget->setConfigFlag(ads::CDockManager::MiddleMouseButtonClosesTab, true);
  d->central_widget->setConfigFlag(ads::CDockManager::DisableTabTextEliding, true);

  window->setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  window->setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::West);
  window->setTabPosition(Qt::RightDockWidgetArea, QTabWidget::East);
  window->setTabPosition(Qt::TopDockWidgetArea, QTabWidget::North);
  window->setTabPosition(Qt::BottomDockWidgetArea, QTabWidget::North);
  window->setDocumentMode(false);

#ifdef MXQT_EVAL_COPY
  auto eval = new QDockWidget(window);
  auto label = new QLabel("<b>NOT DISTRIBUTION A.</b> <u>FOR EVALUATION PURPOSES ONLY.</u> Feedback or questions? Email <a href=\"mailto:peter@trailofbits.com\">peter@trailofbits.com</a>.");
  connect(label, &QLabel::linkActivated,
          [] (const QString &url) {
            QDesktopServices::openUrl(QUrl(url));
          });

  label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  label->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);

  eval->setFeatures(QDockWidget::NoDockWidgetFeatures);
  eval->setTitleBarWidget(new QWidget(eval));
  eval->setWidget(label);
  d->window->addDockWidget(Qt::TopDockWidgetArea, eval);
#endif
}

void WindowManager::AddCentralWidget(IWindowWidget *widget,
                                     const CentralConfig &config) {

  // Do not configure the dock widget with DockWidgetDeleteOnClose=true, because
  // the code explorer is not using WA_DeleteOnClose=true
	auto dock_widget = new ads::CDockWidget(widget->windowTitle());
  dock_widget->setToggleViewActionMode(ads::CDockWidget::ActionModeShow);
	dock_widget->setWidget(widget);

  ads::CDockAreaWidget *existing_dock_area_widget{nullptr};

  for (auto *widget : d->window->findChildren<QWidget *>(Qt::FindChildrenRecursively)) {
    auto dock_area_widget = dynamic_cast<ads::CDockAreaWidget *>(widget);
    if (dock_area_widget != nullptr) {
      existing_dock_area_widget = dock_area_widget;
      break;
    }
  }

  d->central_widget->addDockWidget(ads::CenterDockWidgetArea,
                                   dock_widget,
                                   existing_dock_area_widget);

  connect(widget, &IWindowWidget::RequestAttention, this, [=, this]() {
    dock_widget->toggleViewAction()->trigger();
  });

  // If the widget requested a click, then do it.
  connect(widget, &IWindowWidget::RequestPrimaryClick,
          d->window, &MainWindow::OnRequestPrimaryClick);

  // If the widget requested a context menu, then do it.
  connect(widget, &IWindowWidget::RequestSecondaryClick,
          d->window, &MainWindow::OnRequestSecondaryClick);

  // If the widget requested a context menu, then do it.
  connect(widget, &IWindowWidget::RequestKeyPress,
          d->window, &MainWindow::OnRequestKeyPress);
}

void WindowManager::CreateToolBarIfMissing(void) {
  if (!d->toolbar) {
    d->toolbar = new QToolBar(tr("Main Toolbar"), d->window);
    d->toolbar->setIconSize(QSize(24, 24));
    auto view_menu = Menu(tr("View"));
    view_menu->addAction(d->toolbar->toggleViewAction());
    d->window->addToolBar(d->toolbar);
  }
}

void WindowManager::AddToolBarWidget(QWidget *widget) {
  CreateToolBarIfMissing();

  for (auto button : widget->findChildren<QToolButton *>()) {
    button->setIconSize(d->toolbar->iconSize());
  }

  d->toolbar->addWidget(widget);
}

QAction *WindowManager::AddToolBarButton(
    const QIcon &icon, const NamedAction &action) {

  CreateToolBarIfMissing();

  auto tool_button = new QToolButton(d->toolbar);
  auto tool_action = new QAction(icon, action.name, tool_button);
  tool_button->setDefaultAction(tool_action);

  connect(tool_action, &QAction::toggled,
          [data = action.data, action = action.action] (bool toggled) {
            if (data.isValid()) {
              action.Trigger(data);
            } else {
              action.Trigger(QVariant::fromValue(toggled));
            }
          });

  d->toolbar->addWidget(tool_button);
  return tool_action;
}

//! Add a button to the toolbar, where the value passed to the trigger is the
//! toggled state of the button. This is a button that can stay depressed.
QAction *WindowManager::AddDepressableToolBarButton(
    const QIcon &icon, const QString &name, const TriggerHandle &trigger) {

  CreateToolBarIfMissing();

  auto tool_button = new QToolButton(d->toolbar);
  auto tool_action = new QAction(icon, name, tool_button);
  tool_action->setCheckable(true);
  tool_button->setDefaultAction(tool_action);
  
  connect(tool_action, &QAction::toggled,
          [action = trigger] (bool toggled) {
            action.Trigger(QVariant::fromValue(toggled));
          });

  d->toolbar->addWidget(tool_button);
  return tool_action;
}

void WindowManager::AddDockWidget(IWindowWidget *widget,
                                  const DockConfig &config) {
  widget->setParent(d->window);

  auto dock_widget = new QDockWidget(widget->windowTitle(), d->window);

#ifdef MXQT_EVAL_COPY
  dock_widget->setAllowedAreas(Qt::LeftDockWidgetArea |
                               Qt::RightDockWidgetArea |
                               Qt::BottomDockWidgetArea);
#else
  dock_widget->setAllowedAreas(Qt::AllDockWidgetAreas);
#endif

  dock_widget->setWidget(widget);

  d->dock_configs.emplace(dock_widget, config);

  // Build up an app menu toggler for this dock.
  auto view_action = dock_widget->toggleViewAction();
  auto menu_it = config.app_menu_location.begin();
  auto menu_end = config.app_menu_location.end();
  if (menu_it != menu_end) {

    QMenu *menu = Menu(*menu_it);

    for (++menu_it; menu_it != menu_end; ++menu_it) {
      QMenu *next_menu = nullptr;
      for (QAction *action : menu->actions()) {
        if (action->text() == *menu_it) {
          next_menu = action->menu();
          break;
        }
      }

      if (!next_menu) {
        next_menu = new QMenu(*menu_it, menu);
        menu->addMenu(next_menu);
      }

      menu = next_menu;
    }

    menu->addAction(view_action);
  }

  // If the dock's internal widget is closed, then remove the dock item.
  connect(widget, &IWindowWidget::Closed,
          dock_widget, [=, this] (void) {
                         RemoveDockWidget(dock_widget);
                       });

  // If the dock's internal widget is shown, then show it.
  connect(widget, &IWindowWidget::Shown,
          dock_widget, [=] (void) {
                          dock_widget->show();
                          dock_widget->raise();
                        });

  // If the dock's internal widget is hidden, then remove the dock item.
  connect(widget, &IWindowWidget::Hidden,
          dock_widget, [=] (void) {
                         dock_widget->hide();
                       });

  // If the widget requested a click, then do it.
  connect(widget, &IWindowWidget::RequestPrimaryClick,
          d->window, &MainWindow::OnRequestPrimaryClick);

  // If the widget requested a context menu, then do it.
  connect(widget, &IWindowWidget::RequestSecondaryClick,
          d->window, &MainWindow::OnRequestSecondaryClick);

  // If the widget requested a context menu, then do it.
  connect(widget, &IWindowWidget::RequestKeyPress,
          d->window, &MainWindow::OnRequestKeyPress);

  // Automatically show the dock container if the inner widget
  // requests attention
  connect(widget, &IWindowWidget::RequestAttention,
          dock_widget, [=] (void) {
            dock_widget->show();
            dock_widget->raise();
          });

  // If the dock wants to be removed when closed then delete it.
  if (config.delete_on_close) {
    dock_widget->setAttribute(Qt::WA_DeleteOnClose);
  }

  // Keep the dock title up-to-date w.r.t. the contained widget.
  if (config.keep_title_up_to_date) {
    connect(widget, &QWidget::windowTitleChanged,
            dock_widget, &QDockWidget::setWindowTitle);
  }

  auto area = ConvertLocation(config.location);
  d->window->addDockWidget(area, dock_widget);

  // Add this dock to a tab.
  if (config.tabify) {
    for (auto &[other_dock_widget, other_config] : d->dock_configs) {
      if (other_dock_widget != dock_widget && other_config.tabify &&
          d->window->dockWidgetArea(other_dock_widget) == area) {
        d->window->tabifyDockWidget(other_dock_widget, dock_widget);
        dock_widget->lower();
        break;
      }
    }
  }
}

//! Invoked when a `dock_widget`s internal widget does `->close()`.
void WindowManager::RemoveDockWidget(QDockWidget *dock_widget) {
  dock_widget->setAttribute(Qt::WA_DeleteOnClose);
  dock_widget->close();
  d->dock_configs.erase(dock_widget);
}

//! Invoked when a primary click happens on an `IModel`-compatible index.
void WindowManager::OnPrimaryClick(const QModelIndex &index) {
  d->window->OnRequestPrimaryClick(index);
}

//! Invoked when a secondary click happens on an `IModel`-compatible index.
void WindowManager::OnSecondaryClick(const QModelIndex &index) {
  d->window->OnRequestSecondaryClick(index);
}

//! Invoked when a key press happens on an `IModel`-compatible index.
void WindowManager::OnKeyPress(const QKeySequence &keys,
                               const QModelIndex &index) {
  d->window->OnRequestKeyPress(keys, index);
}

//! Return the application-level menu for a given menu name.
QMenu *WindowManager::Menu(const QString &menu_name) {
  auto it = d->app_menus.find(menu_name);
  if (it != d->app_menus.end()) {
    return it.value();
  }

  auto menu = new QMenu(menu_name, d->window);
  d->window->menuBar()->addMenu(menu);
  d->app_menus.insert(menu_name, menu);
  return menu;
}

QMainWindow *WindowManager::Window(void) const noexcept {
  return d->window;
}

}  // namespace mx::gui
