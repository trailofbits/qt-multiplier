// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QSet>

#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/Frontend/Macro.h>

namespace mx::gui {

class ConfigManager;
class ThemeManager;

class ExpandedMacrosModel Q_DECL_FINAL : public IModel {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~ExpandedMacrosModel(void);

  ExpandedMacrosModel(const ConfigManager &config_manager,
                      QObject *parent = nullptr);

  void AddMacro(Macro macro);
  void RemoveMacro(Macro macro);

  QModelIndex index(
      int row, int column, const QModelIndex &parent) const Q_DECL_FINAL;
  QModelIndex parent(const QModelIndex &child) const Q_DECL_FINAL;
  int rowCount(const QModelIndex &parent) const Q_DECL_FINAL;
  int columnCount(const QModelIndex &parent) const Q_DECL_FINAL;
  QVariant data(const QModelIndex &index, int role) const Q_DECL_FINAL;
  QVariant headerData(int, Qt::Orientation, int role) const Q_DECL_FINAL;

 private slots:
  void OnIndexChanged(const ConfigManager &config_manager);
  void OnThemeChanged(const ThemeManager &theme_manager);

 signals:
  void ExpandMacros(const QSet<RawEntityId> &macros_to_expand);
};

}  // namespace mx::gui
