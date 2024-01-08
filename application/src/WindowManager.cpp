// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "WindowManager.h"

#include <QDockWidget>
#include <QMap>
#include <QMenu>
#include <QMenuBar>

#include <multiplier/GUI/Interfaces/IWindowWidget.h>
#include <multiplier/GUI/Widgets/TabWidget.h>
#include <unordered_map>

#include "MainWindow.h"

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

  std::unordered_map<QDockWidget *, DockConfig> dock_configs;

  QMap<QString, QMenu *> app_menus;

  inline PrivateData(MainWindow *parent)
      : window(parent) {}
};

WindowManager::~WindowManager(void) {}

WindowManager::WindowManager(MainWindow *window)
    : IWindowManager(window),
      d(new PrivateData(window)) {

  window->setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  window->setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::West);
  window->setTabPosition(Qt::RightDockWidgetArea, QTabWidget::East);
  window->setTabPosition(Qt::TopDockWidgetArea, QTabWidget::North);
  window->setTabPosition(Qt::BottomDockWidgetArea, QTabWidget::North);
  window->setDocumentMode(false);

  auto tab_widget = new TabWidget(window);
  tab_widget->setTabsClosable(true);
  tab_widget->setDocumentMode(true);
  tab_widget->setTabBarAutoHide(false);
  window->setCentralWidget(tab_widget);
}

void WindowManager::AddDockWidget(IWindowWidget *widget,
                                  const DockConfig &config) {
  widget->setParent(d->window);

  auto dock_widget = new QDockWidget(widget->windowTitle(), d->window);
  dock_widget->setAllowedAreas(Qt::AllDockWidgetAreas);
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
          [=, this] (void) {
            RemoveDockWidget(dock_widget);
          });

  // If the dock's internal widget is shown, then show it.
  connect(widget, &IWindowWidget::Shown,
          [=] (void) {
            dock_widget->show();
            dock_widget->raise();
          });

  // If the dock's internal widget is hidden, then remove the dock item.
  connect(widget, &IWindowWidget::Hidden,
          [=] (void) {
            dock_widget->hide();
          });

  // If the widget requested a click, then do it.
  connect(widget, &IWindowWidget::RequestPrimaryClick,
          [this] (const QModelIndex &index) {
            d->window->OnRequestPrimaryClick(index);
          });

  // If the widget requested a context menu, then do it.
  connect(widget, &IWindowWidget::RequestSecondaryClick,
          [this] (const QModelIndex &index) {
            d->window->OnRequestSecondaryClick(index);
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
void WindowManager::PrimaryClick(const QModelIndex &index) {
  d->window->OnRequestPrimaryClick(index);
}

//! Invoked when a secondary click happens on an `IModel`-compatible index.
void WindowManager::SecondaryClick(const QModelIndex &index) {
  d->window->OnRequestSecondaryClick(index);
}

//! Return the application-level menu for a given menu name.
QMenu *WindowManager::Menu(const QString &menu_name) {
  auto it = d->app_menus.find(menu_name);
  if (it != d->app_menus.end()) {
    return it.value();
  }

  auto menu = new QMenu(menu_name);
  d->window->menuBar()->addMenu(menu);
  d->app_menus.insert(menu_name, menu);
  return menu;
}

QMainWindow *WindowManager::Window(void) const noexcept {
  return d->window;
}

}  // namespace mx::gui
