// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>

#include <QAction>
#include <QMainWindow>
#include <QMenu>

#include <multiplier/GUI/Interfaces/IWindowManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>

namespace mx::gui {

IMainWindowPlugin::IMainWindowPlugin(ConfigManager &, IWindowManager *parent)
    : QObject(parent->Window()) {}

IMainWindowPlugin::~IMainWindowPlugin(void) {}

// Act on a primary click. For example, if browse mode is enabled, then this
// is a "normal" click, however, if browse mode is off, then this is a meta-
// click.
void IMainWindowPlugin::ActOnPrimaryClick(
    IWindowManager *, const QModelIndex &) {}

// Allow a main window to add an a named action to a context menu.
std::optional<NamedAction> IMainWindowPlugin::ActOnSecondaryClick(
    IWindowManager *, const QModelIndex &) {
  return std::nullopt;
}

// Allow a main window to add an arbitrary number of named actions to a
// context menu.
std::vector<NamedAction> IMainWindowPlugin::ActOnSecondaryClickEx(
    IWindowManager *manager, const QModelIndex &index) {
  std::vector<NamedAction> ret;
  if (auto maybe_named_action = ActOnSecondaryClick(manager, index)) {
    ret.emplace_back(std::move(maybe_named_action.value()));
  }
  return ret;
}

// Allow a main window plugin to act on, e.g. modify, a context menu.
void IMainWindowPlugin::ActOnContextMenu(
    IWindowManager *manager, QMenu *menu, const QModelIndex &index) {
  for (auto named_action : ActOnSecondaryClickEx(manager, index)) {
    auto action = new QAction(named_action.name, menu);
    connect(
        action, &QAction::triggered,
        [action = std::move(named_action.action),
         data = std::move(named_action.data)] (void) {
          action.Trigger(data);
        });
    menu->addAction(action);
  }
}

// Allow a main window plugin to act on a long hover over something.
void IMainWindowPlugin::ActOnLongHover(IWindowManager *, const QModelIndex &) {}

// Allow a main window plugin to act on a key sequence.
std::optional<NamedAction> IMainWindowPlugin::ActOnKeyPress(
    IWindowManager *, const QKeySequence &, const QModelIndex &) {
  return std::nullopt;
}

// Allow a main window plugin to provide one of several actions to be
// performed on a key press.
std::vector<NamedAction> IMainWindowPlugin::ActOnKeyPressEx(
    IWindowManager *manager, const QKeySequence &keys,
    const QModelIndex &index) {
  std::vector<NamedAction> ret;
  if (auto maybe_named_action = ActOnKeyPress(manager, keys, index)) {
    ret.emplace_back(std::move(maybe_named_action.value()));
  }
  return ret;
}

}  // namespace mx::gui