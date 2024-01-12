// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QObject>

#include <multiplier/GUI/Managers/ActionManager.h>
#include <optional>
#include <vector>

QT_BEGIN_NAMESPACE
class QKeySequence;
class QMenu;
class QModelIndex;
QT_END_NAMESPACE

namespace mx::gui {

class ConfigManager;
class IWindowManager;
class IWindowWidget;
class MediaManager;
class ThemeManager;

class IMainWindowPlugin : public QObject {
  Q_OBJECT

 public:
  virtual ~IMainWindowPlugin(void);

  IMainWindowPlugin(ConfigManager &config, IWindowManager *parent = nullptr);

  //! Act on a primary click. For example, if browse mode is enabled, then this
  //! is a "normal" click, however, if browse mode is off, then this is a meta-
  //! click.
  virtual void ActOnPrimaryClick(
      IWindowManager *manager, const QModelIndex &index);

  //! Allow a main window to add an a named action to a context menu.
  virtual std::optional<NamedAction> ActOnSecondaryClick(
      IWindowManager *manager, const QModelIndex &index);

  //! Allow a main window to add an arbitrary number of named actions to a
  //! context menu.
  virtual std::vector<NamedAction> ActOnSecondaryClickEx(
      IWindowManager *manager, const QModelIndex &index);

  //! Allow a main window plugin to act on, e.g. modify, a context menu.
  virtual void ActOnContextMenu(
      IWindowManager *manager, QMenu *menu, const QModelIndex &index);

  //! Allow a main window plugin to act on a long hover over something.
  virtual void ActOnLongHover(
      IWindowManager *manager, const QModelIndex &index);

  //! Allow a main window plugin to act on a key sequence.
  virtual std::optional<NamedAction> ActOnKeyPress(
      IWindowManager *manager, const QKeySequence &keys,
      const QModelIndex &index);

  //! Allow a main window plugin to provide one of several actions to be
  //! performed on a key press.
  virtual std::vector<NamedAction> ActOnKeyPressEx(
      IWindowManager *manager, const QKeySequence &keys,
      const QModelIndex &index);

 signals:

  //! Signal emitted when some nested plugin widget wants to signal a click
  //! action for a `QModelIndex` whose model follows the `IModel` interface, and
  //! thus could benefit from allowing other plugins to see the index.
  void RequestPrimaryClick(const QModelIndex &index);

  //! Signal emitted when some nested plugin widget wants to open a context
  //! menu for a `QModelIndex` whose model follows the `IModel` interface, and
  //! thus could benefit from allowing other plugins to see the index.
  void RequestSecondaryClick(const QModelIndex &index);

  //! Signal emitted when some nested plugin widget wants to signal a key
  //! press action for a `QModelIndex` whose model follows the `IModel`
  //! interface, and thus could benefit from allowing other plugins to see the
  //! index.
  void RequestKeyPress(const QKeySequence &keys, const QModelIndex &index);
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::IMainWindowPlugin,
                    "com.trailofbits.interface.IMainWindowPlugin")
