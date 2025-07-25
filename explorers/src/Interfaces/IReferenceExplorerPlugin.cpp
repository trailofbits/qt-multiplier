// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Interfaces/IReferenceExplorerPlugin.h>

#include <QAction>

#include <multiplier/GUI/Explorers/ReferenceExplorer.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>

namespace mx::gui {

IReferenceExplorerPlugin::IReferenceExplorerPlugin(
    ConfigManager &config, QObject *parent)
    : QObject(parent) {

  connect(&(config.ThemeManager()), &ThemeManager::ThemeChanged,
          this, &IReferenceExplorerPlugin::OnThemeChanged);

  connect(&(config.MediaManager()), &MediaManager::IconsChanged,
          this, &IReferenceExplorerPlugin::OnIconsChanged);
}

IReferenceExplorerPlugin::~IReferenceExplorerPlugin(void) {}

// If `reference_explorer` is a pointer to a reference explorer, then
// invoke `create_plugin(reference_explorer)`, returning a raw pointer to
// a created `IReferenceExplorerPlugin` to be owned by the reference explorer.
bool IReferenceExplorerPlugin::Register(
  IMainWindowPlugin *reference_explorer,
  std::function<IReferenceExplorerPluginPtr(IMainWindowPlugin *)> create_plugin) {

  auto parent = dynamic_cast<ReferenceExplorer *>(reference_explorer);
  if (!parent) {
    return false;
  }

  auto child = create_plugin(parent);
  if (!child) {
    return false;
  }

  parent->AddPlugin(std::move(child));
  return true;
}

// Act on a primary click. For example, if browse mode is enabled, then this
// is a "normal" click, however, if browse mode is off, then this is a meta-
// click.
void IReferenceExplorerPlugin::ActOnPrimaryClick(
    IWindowManager *, const QModelIndex &) {}

// Allow a main window to add an a named action to a context menu.
std::optional<NamedAction> IReferenceExplorerPlugin::ActOnSecondaryClick(
    IWindowManager *, const QModelIndex &) {
  return std::nullopt;
}

// Allow a main window to add an arbitrary number of named actions to a
// context menu.
std::vector<NamedAction> IReferenceExplorerPlugin::ActOnSecondaryClickEx(
    IWindowManager *manager, const QModelIndex &index) {
  std::vector<NamedAction> ret;
  if (auto maybe_named_action = ActOnSecondaryClick(manager, index)) {
    ret.emplace_back(std::move(maybe_named_action.value()));
  }
  return ret;
}

// Allow a main window plugin to act on, e.g. modify, a context menu.
void IReferenceExplorerPlugin::ActOnContextMenu(
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
void IReferenceExplorerPlugin::ActOnLongHover(
    IWindowManager *, const QModelIndex &) {}

// Allow a main window plugin to act on a key sequence.
std::optional<NamedAction> IReferenceExplorerPlugin::ActOnKeyPress(
    IWindowManager *, const QKeySequence &, const QModelIndex &) {
  return std::nullopt;
}

// Allow a main window plugin to provide one of several actions to be
// performed on a key press.
std::vector<NamedAction> IReferenceExplorerPlugin::ActOnKeyPressEx(
    IWindowManager *manager, const QKeySequence &keys, const QModelIndex &index) {
  std::vector<NamedAction> ret;
  if (auto maybe_named_action = ActOnKeyPress(manager, keys, index)) {
    ret.emplace_back(std::move(maybe_named_action.value()));
  }
  return ret;
}

void IReferenceExplorerPlugin::OnThemeChanged(const ThemeManager &) {}
void IReferenceExplorerPlugin::OnIconsChanged(const MediaManager &) {}

}  // namespace mx::gui