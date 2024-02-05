// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QObject>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QAction;
class QIcon;
class QKeySequence;
class QMainWindow;
class QMenu;
class QModelIndex;
class QWidget;
QT_END_NAMESPACE

namespace mx::gui {

class IWindowWidget;
class NamedAction;
class TriggerHandle;

//! Manages widgets in a window, including the menu, the docks, etc.
class IWindowManager : public QObject {
  Q_OBJECT

 public:
  virtual ~IWindowManager(void);

  using QObject::QObject;

  enum class DockLocation {
    Left,
    Right,
    Top,
    Bottom,
  };

  struct DockConfig {
    //! ID of this dock, e.g. `com.trailofbits.dock.EntityExplorer`. This is
    //! optional and can be left empty.
    QString id;

    //! Default location of the dock.
    DockLocation location{DockLocation::Left};

    //! Try to "tabify" this dock widget.
    bool tabify{false};

    //! Should we delete this dock on close? This makes the dock showable again
    //! from the `View` menu.
    bool delete_on_close{false};

    //! Should the dock title change with the widget title?
    bool keep_title_up_to_date{true};

    //! If non-empty, this is the menu location where this dock widget should
    //! show up.
    QStringList app_menu_location;
  };

  struct CentralConfig {
    //! ID of this central widget, e.g. `com.trailofbits.dock.EntityExplorer`.
    //! This is optional and can be left empty.
    QString id;

    //! Tooltip for this item.
    QString tooltip;

    //! Should the dock title change with the widget title?
    bool keep_title_up_to_date{true};
  };

  //! Add a widget to the toolbar.
  virtual void AddToolBarWidget(QWidget *widget) = 0;

  //! Add a button to the toolbar.
  virtual QAction *AddToolBarButton(
      const QIcon &icon, const NamedAction &action) = 0;

  //! Add a button to the toolbar, where the value passed to the trigger is the
  //! toggled state of the button. This is a button that can stay depressed.
  virtual QAction *AddDepressableToolBarButton(
      const QIcon &icon, const QString &name, const TriggerHandle &trigger) = 0;

  //! Add a widget to the central tab view.
  virtual void AddCentralWidget(IWindowWidget *widget,
                                const CentralConfig &config) = 0;

  //! Adds a dock widget to the window manager.
  virtual void AddDockWidget(IWindowWidget *widget,
                             const DockConfig &config) = 0;

  //! Return the main window of the application.
  virtual QMainWindow *Window(void) const noexcept = 0;

  //! Return the application-level menu for a given menu name.
  virtual QMenu *Menu(const QString &menu_name) = 0;

 public slots:

  //! Invoked when a primary click happens on an `IModel`-compatible index.
  virtual void OnPrimaryClick(const QModelIndex &index) = 0;

  //! Invoked when a secondary click happens on an `IModel`-compatible index.
  virtual void OnSecondaryClick(const QModelIndex &index) = 0;

  //! Invoked when a key press happens on an `IModel`-compatible index.
  virtual void OnKeyPress(const QKeySequence &keys,
                          const QModelIndex &index) = 0;
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::IWindowManager,
                    "com.trailofbits.interface.IWindowManager")
