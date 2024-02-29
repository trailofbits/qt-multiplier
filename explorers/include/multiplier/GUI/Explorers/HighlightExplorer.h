// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>

#include <multiplier/Index.h>

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
  void InitializeFromModelIndex(const QModelIndex &index);
  void InitializeFromVariantEntity(const VariantEntity &var_entity);
  void SetColor(const QColor &color);

 private slots:
  void OnIndexChanged(const ConfigManager &config_manager);
  void SetUserColor(void);
  void SetRandomColor(void);
  void RemoveColor(void);
  void ClearAllColors(void);
  void OnThemeChanged(const ThemeManager &theme_manager);
  void OnToggleHighlightColorAction(const QVariant &data);
};

}  // namespace mx::gui
