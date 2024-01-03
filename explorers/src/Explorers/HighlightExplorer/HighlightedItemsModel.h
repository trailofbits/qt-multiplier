// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/Entity.h>

#include <vector>

namespace mx::gui {

class HighlightedItemsModel Q_DECL_FINAL : public IModel {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~HighlightedItemsModel(void);

  HighlightedItemsModel(QObject *parent = nullptr);

  void AddEntity(const VariantEntity &entity);
  void RemoveEntity(const std::vector<RawEntityId> &eids);

  QModelIndex index(
      int row, int column, const QModelIndex &parent) const Q_DECL_FINAL;
  QModelIndex parent(const QModelIndex &child) const Q_DECL_FINAL;
  int rowCount(const QModelIndex &parent) const Q_DECL_FINAL;
  int columnCount(const QModelIndex &parent) const Q_DECL_FINAL;
  QVariant data(const QModelIndex &index, int role) const Q_DECL_FINAL;
};

}  // namespace mx::gui
