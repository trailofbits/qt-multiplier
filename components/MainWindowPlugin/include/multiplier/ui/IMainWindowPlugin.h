// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QModelIndex>
#include <QPalette>
#include <QString>
#include <multiplier/ui/ActionRegistry.h>
#include <optional>
#include <vector>

namespace mx::gui {

class CodeViewTheme;
class Context;
class IGlobalHighlighter;

class IMainWindowPlugin : public QObject {
  Q_OBJECT

 public:
  virtual ~IMainWindowPlugin(void);

  IMainWindowPlugin(const Context &context, QMainWindow *parent = nullptr);

  // Act on a primary click. For example, if browse mode is enabled, then this
  // is a "normal" click, however, if browse mode is off, then this is a meta-
  // click.
  virtual void ActOnPrimaryClick(const QModelIndex &index);

  // Allow a main window to add an a named action to a context menu.
  virtual std::optional<NamedAction> ActOnSecondaryClick(
      const QModelIndex &index);

  // Allow a main window to add an arbitrary number of named actions to a
  // context menu.
  virtual std::vector<NamedAction> ActOnSecondaryClickEx(
      const QModelIndex &index);

  // Allow a main window plugin to act on, e.g. modify, a context menu.
  virtual void ActOnContextMenu(QMenu *menu, const QModelIndex &index);

  // Allow a main window plugin to act on a long hover over something.
  virtual void ActOnLongHover(const QModelIndex &index);

  // Allow a main window plugin to act on a key sequence.
  virtual std::optional<NamedAction> ActOnKeyPress(
      const QKeySequence &keys, const QModelIndex &index);

  // Allow a main window plugin to provide one of several actions to be
  // performed on a key press.
  virtual std::vector<NamedAction> ActOnKeyPressEx(
      const QKeySequence &keys, const QModelIndex &index);

  // Allow a main window plugin to know when the theme is changed.
  virtual void ActOnThemeChange(const QPalette &new_palette,
                                const CodeViewTheme &new_theme);

  // Requests a dock wiget from this plugin. Can return `nullptr`.
  virtual QWidget *CreateDockWidget(QWidget *parent);
 
 signals:
  // TODO(pag): Consider removing?
  void HideDockWidget(void);
  void ShowDockWidget(void);
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::IMainWindowPlugin, "com.trailofbits.IMainWindowPlugin")
