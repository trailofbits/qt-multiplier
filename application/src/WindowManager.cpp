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
#include <QTabBar>

#include <multiplier/GUI/Interfaces/IWindowWidget.h>
#include <multiplier/GUI/Widgets/SimpleTextInputDialog.h>
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
  TabWidget * const tab_widget;

  std::unordered_map<QDockWidget *, DockConfig> dock_configs;

  QMap<QString, QMenu *> app_menus;

  inline PrivateData(MainWindow *parent)
      : window(parent),
        tab_widget(new TabWidget(parent)) {}
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

  connect(d->tab_widget->tabBar(), &QTabBar::tabCloseRequested,
          this, &WindowManager::OnTabBarClose);

  connect(d->tab_widget->tabBar(), &QTabBar::tabBarDoubleClicked,
          this, &WindowManager::OnTabBarDoubleClick);

  d->tab_widget->setTabsClosable(true);
  d->tab_widget->setDocumentMode(true);
  d->tab_widget->setTabBarAutoHide(false);
  window->setCentralWidget(d->tab_widget);
}

void WindowManager::OnTabBarClose(int i) {
  auto widget = d->tab_widget->widget(i);
  d->tab_widget->RemoveTab(i);
  widget->close();
}

void WindowManager::OnTabBarDoubleClick(int i) {
  auto current_tab_name = d->tab_widget->tabText(i);

  SimpleTextInputDialog dialog(tr("Insert the new tab name"), current_tab_name,
                               d->tab_widget);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  const auto &opt_tab_name = dialog.TextInput();

  QString new_tab_name;
  if (opt_tab_name.has_value()) {
    new_tab_name = opt_tab_name.value();
  } else {
    new_tab_name = tr("Reference Browser #") + QString::number(i);
  }

  d->tab_widget->setTabText(i, new_tab_name);
}

void WindowManager::AddCentralWidget(IWindowWidget *widget,
                                     const CentralConfig &config) {
  d->tab_widget->InsertTab(0, widget, config.keep_title_up_to_date);

  connect(widget, &IWindowWidget::Shown,
          d->tab_widget, [=, this] (void) {
                            d->tab_widget->setCurrentWidget(widget);
                          });

  connect(widget, &IWindowWidget::Closed,
          d->tab_widget, [=, this] (void) {
                            auto index = d->tab_widget->indexOf(widget);
                            if (index != -1) {
                              d->tab_widget->RemoveTab(index);
                            }
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

  auto menu = new QMenu(menu_name);
  d->window->menuBar()->addMenu(menu);
  d->app_menus.insert(menu_name, menu);
  return menu;
}

QMainWindow *WindowManager::Window(void) const noexcept {
  return d->window;
}

}  // namespace mx::gui
