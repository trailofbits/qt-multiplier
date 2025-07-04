// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QMainWindow>
#include <QMenu>
#include <QModelIndex>

#include <multiplier/GUI/Managers/ActionManager.h>
#include <optional>
#include <vector>

namespace mx::gui {

class ConfigManager;
class IMainWindowPlugin;
class IReferenceExplorerPlugin;
class IWindowManager;
class MediaManager;
class ThemeManager;

using IReferenceExplorerPluginPtr = std::unique_ptr<IReferenceExplorerPlugin>;

// Describes a plugin to the reference explorer.
class IReferenceExplorerPlugin : public QObject {
  Q_OBJECT

 public:
  virtual ~IReferenceExplorerPlugin(void);

  IReferenceExplorerPlugin(ConfigManager &config, QObject *parent = nullptr);

  // If `reference_explorer` is a pointer to a reference explorer, then
  // invoke `create_plugin(reference_explorer)`, returning a raw pointer to
  // a created `IReferenceExplorerPlugin` to be owned by the reference explorer.
  static bool Register(
      IMainWindowPlugin *reference_explorer,
      std::function<IReferenceExplorerPluginPtr(IMainWindowPlugin *)>
          create_plugin);

  virtual void ActOnPrimaryClick(
      IWindowManager *manager, const QModelIndex &index);

  virtual std::optional<NamedAction> ActOnSecondaryClick(
      IWindowManager *manager, const QModelIndex &index);

  virtual std::vector<NamedAction> ActOnSecondaryClickEx(
      IWindowManager *manager, const QModelIndex &index);

  virtual void ActOnContextMenu(
      IWindowManager *manager, QMenu *menu, const QModelIndex &index);

  // Allow a main window plugin to act on a long hover over something.
  virtual void ActOnLongHover(
      IWindowManager *manager, const QModelIndex &index);

  // Allow a main window plugin to act on a key sequence.
  virtual std::optional<NamedAction> ActOnKeyPress(
      IWindowManager *manager, const QKeySequence &keys,
      const QModelIndex &index);

  // Allow a main window plugin to provide one of several actions to be
  // performed on a key press.
  virtual std::vector<NamedAction> ActOnKeyPressEx(
      IWindowManager *manager, const QKeySequence &keys,
      const QModelIndex &index);

 public slots:
  virtual void OnThemeChanged(const ThemeManager &theme_manager);
  virtual void OnIconsChanged(const MediaManager &media_manager);
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::IReferenceExplorerPlugin,
                    "com.trailofbits.interface.IReferenceExplorerPlugin")
