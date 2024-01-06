// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Interfaces/IInfoGenerator.h>

#include "EntityInformationRunnable.h"

namespace mx::gui {

class ConfigManager;

class EntityInformationModel Q_DECL_FINAL : public IModel {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:

  enum {
    // Returns `true` if this node should be auto-expanded.
    AutoExpandRole = IModel::MultiplierUserRole,
  };

  virtual ~EntityInformationModel(void);

  EntityInformationModel(const FileLocationCache &cache,
                         AtomicU64Ptr version_number,
                         QObject *parent = nullptr);

  QModelIndex index(
      int row, int column, const QModelIndex &parent) const Q_DECL_FINAL;

  QModelIndex parent(const QModelIndex &child) const Q_DECL_FINAL;

  int rowCount(const QModelIndex &parent) const Q_DECL_FINAL;

  int columnCount(const QModelIndex &parent) const Q_DECL_FINAL;

  QVariant data(const QModelIndex &index, int role) const Q_DECL_FINAL;

  void Clear(void);

 public slots:
  void OnIndexChanged(const ConfigManager &config_manager);

  void AddData(
      uint64_t version_number, QVector<IInfoGenerator::Item> child_items);

  void ProcessData(void);
};

}  // namespace mx::gui
