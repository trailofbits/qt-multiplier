// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/ui/IMainWindowPlugin.h>

namespace mx::gui {

IMainWindowPlugin::IMainWindowPlugin(const Context &, QMainWindow *parent)
    : QObject(parent) {}

IMainWindowPlugin::~IMainWindowPlugin(void) {}

// Act on a primary click. For example, if browse mode is enabled, then this
// is a "normal" click, however, if browse mode is off, then this is a meta-
// click.
void IMainWindowPlugin::ActOnPrimaryClick(const QModelIndex &) {}

// Allow a main window plugin to act on, e.g. modify, a context menu.
void IMainWindowPlugin::ActOnSecondaryClick(QMenu *, const QModelIndex &) {}

// Allow a main window plugin to act on a long hover over something.
void IMainWindowPlugin::ActOnLongHover(const QModelIndex &) {}

// Allow a main window plugin to know when the theme is changed.
void IMainWindowPlugin::ActOnThemeChange(const QPalette &,
                                         const CodeViewTheme &) {}

// Requests a dock wiget from this plugin. Can return `nullptr`.
QWidget *IMainWindowPlugin::CreateDockWidget(QWidget *) {
  return nullptr;
}

}  // namespace mx::gui