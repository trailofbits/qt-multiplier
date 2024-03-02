// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>

#include <multiplier/Index.h>

#include <QColor>

#include <optional>

namespace mx::gui {

class HighlightExplorer Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~HighlightExplorer(void);

  HighlightExplorer(ConfigManager &config_manager,
                    IWindowManager *parent = nullptr);

  /// Responds to the context menu callback in order to generate the highlight actions
  void ActOnContextMenu(IWindowManager *manager, QMenu *menu,
                        const QModelIndex &index) Q_DECL_FINAL;

  /// Responds to the `com.trailofbits.action.ToggleHighlightColor` action
  virtual std::optional<NamedAction> ActOnKeyPress(
      IWindowManager *manager, const QKeySequence &keys,
      const QModelIndex &index) Q_DECL_FINAL;

 private:
  /// Creates the `Highlight Explorer` dock widget in the main window
  void CreateDockWidget(void);

  //
  // Internal API
  //

  /// Entity information used for highlight commands
  struct EntityInformation final {
    /// The original `VariantEntity`, as passed to the highlight explorer
    VariantEntity var_entity;

    /// The dereferenced `VariantEntity`. Could be the same as `var_entity`
    VariantEntity deref_var_entity;

    /// A list of all the entity IDs related to `var_entity`
    std::vector<RawEntityId> id_list;
  };

  /// Returns an `EntityInformation` object by quering the specified variant entity
  std::optional<EntityInformation>
  QueryEntityInformation(const VariantEntity &var_entity);

  /// Returns an `EntityInformation` object by quering the specified model index
  std::optional<EntityInformation>
  QueryEntityInformation(const QModelIndex &index);

  /// Returns an `EntityInformation` object by quering the specified QVariant
  std::optional<EntityInformation>
  QueryEntityInformation(const QVariant &var);

  /// Clears all highlights and schedules an update
  void ClearAllHighlights();

  /// Returns true if the specified entity is already highlighted
  bool IsEntityHighlighted(const EntityInformation &entity_info);

  /// Removes the specified entity and schedules an update
  void RemoveEntityHighlight(const EntityInformation &entity_info);

  /// Sets an highlight for the specified entity and schedules an update
  /// \param opt_color If this is std::nullopt, a random color will be generated
  void SetEntityHighlight(const EntityInformation &entity_info,
                          const std::optional<QColor> &opt_color = std::nullopt);

  /// Schedules a color update
  void ScheduleColorUpdate();

  /// Commits any scheduled color update
  void EmitColorUpdate();

 private slots:
  /// Received when the active project (Multiplier's Index) is changed. This
  /// will clear all highlights and issue a color update
  void OnIndexChanged(const ConfigManager &config_manager);

  /// Clears all updates, executing a color update
  void ClearAllColors(void);

  /// Used to (re)initialize the color generator, as well as updating all
  /// generated colors to match the new background color. A color update
  /// will be issued at the end of the update.
  void OnThemeChanged(const ThemeManager &theme_manager);

  /// Responds to the `com.trailofbits.action.ToggleHighlightColor` action.
  /// A color update will be issued at the end of the operation
  /// \param data The data parameter can either be a `VariantEntity` or a
  ///             a QModelIndex
  void OnToggleHighlightColorAction(const QVariant &data);
};

}  // namespace mx::gui
