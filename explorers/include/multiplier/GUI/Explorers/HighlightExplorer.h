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

  void ActOnContextMenu(IWindowManager *manager, QMenu *menu,
                        const QModelIndex &index) Q_DECL_FINAL;

  virtual std::optional<NamedAction> ActOnKeyPress(
      IWindowManager *manager, const QKeySequence &keys,
      const QModelIndex &index) Q_DECL_FINAL;

 private:
  void CreateDockWidget(void);
  void ColorsUpdated(void);

  //
  // Internal API
  //

  struct EntityInformation final {
    VariantEntity var_entity;
    VariantEntity deref_var_entity;
    std::vector<RawEntityId> id_list;
  };

  std::optional<EntityInformation>
  QueryEntityInformation(const VariantEntity &var_entity);

  std::optional<EntityInformation>
  QueryEntityInformation(const QModelIndex &index);

  std::optional<EntityInformation>
  QueryEntityInformation(const QVariant &var);

  void ClearAllHighlights();
  bool IsEntityHighlighted(const EntityInformation &entity_info);
  void RemoveEntityHighlight(const EntityInformation &entity_info);
  void SetEntityHighlight(const EntityInformation &entity_info,
                          const std::optional<QColor> &opt_color = std::nullopt);

  void ScheduleColorUpdate();
  void EmitColorUpdate();


 private slots:
  void OnIndexChanged(const ConfigManager &config_manager);
  void ClearAllColors(void);
  void OnThemeChanged(const ThemeManager &theme_manager);
  void OnToggleHighlightColorAction(const QVariant &data);
};

}  // namespace mx::gui
