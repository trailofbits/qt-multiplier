// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QMainWindow>
#include <QMenu>
#include <QModelIndex>

#include <multiplier/ui/ActionRegistry.h>
#include <optional>
#include <vector>

namespace mx::gui {

class Context;
class IMainWindowPlugin;

// Describes a plugin to the reference explorer.
class IReferenceExplorerPlugin : public QObject {
  Q_OBJECT

 public:
  virtual ~IReferenceExplorerPlugin(void);

  IReferenceExplorerPlugin(const Context &context, QObject *parent = nullptr);

  // If `reference_explorer` is a pointer to a reference explorer, then
  // invoke `create_plugin(reference_explorer)`, returning a raw pointer to
  // a created `IReferenceExplorerPlugin` to be owned by the reference explorer.
  static bool Register(
      IMainWindowPlugin *reference_explorer,
      std::function<IReferenceExplorerPlugin *(IMainWindowPlugin *)>
          create_plugin);

  virtual void ActOnMainWindowPrimaryClick(
      QMainWindow *window, const QModelIndex &index);

  virtual std::optional<NamedAction> ActOnMainWindowSecondaryClick(
      QMainWindow *window, const QModelIndex &index);

  virtual std::vector<NamedAction> ActOnMainWindowSecondaryClickEx(
      QMainWindow *window, const QModelIndex &index);

  virtual void ActOnMainWindowContextMenu(
      QMainWindow *window, QMenu *menu, const QModelIndex &index);

  // Allow a main window plugin to act on a long hover over something.
  virtual void ActOnMainWindowLongHover(
      QMainWindow *window, const QModelIndex &index);

  // Allow a main window plugin to act on a key sequence.
  virtual std::optional<NamedAction> ActOnMainWindowKeyPress(
      QMainWindow *window, const QKeySequence &keys, const QModelIndex &index);

  // Allow a main window plugin to provide one of several actions to be
  // performed on a key press.
  virtual std::vector<NamedAction> ActOnMainWindowKeyPressEx(
      QMainWindow *window, const QKeySequence &keys, const QModelIndex &index);
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::IReferenceExplorerPlugin, "com.trailofbits.IReferenceExplorerPlugin")
