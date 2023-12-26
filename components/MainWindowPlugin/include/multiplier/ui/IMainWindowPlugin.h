// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QMainWindow>
#include <QMenu>
#include <QModelIndex>
#include <QPalette>
#include <QString>

namespace mx {
class FileLocationCache;
class Index;
}  // namespace mx
namespace mx::gui {

class ActionRegistry;
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

  // Allow a main window plugin to act on, e.g. modify, a context menu.
  virtual void ActOnSecondaryClick(QMenu *menu, const QModelIndex &index);

  // Allow a main window plugin to act on a long hover over something.
  virtual void ActOnLongHover(const QModelIndex &index);

  // Allow a main window plugin to know when the theme is changed.
  virtual void ActOnThemeChange(const QPalette &new_palette,
                                const CodeViewTheme &new_theme);

  // Requests a dock wiget from this plugin. Can return `nullptr`.
  virtual QWidget *CreateDockWidget(QWidget *parent);
 
 signals:
  void HideDockWidget(void);
  void ShowDockWidget(void);
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::IMainWindowPlugin, "com.trailofbits.IMainWindowPlugin")
