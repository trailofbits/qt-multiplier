// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QAbstractItemModel>

#include <multiplier/Entity.h>
#include <multiplier/Frontend/Token.h>

namespace mx::gui {

class IModel : public QAbstractItemModel {
 public:
  virtual ~IModel(void);

  using QAbstractItemModel::QAbstractItemModel;

  enum {
    // Returns a `VariantEntity`.
    EntityRole = Qt::UserRole,

    // Returns a `TokenRange` corresponding to the data that would be returned
    // by the `Qt::DisplayRole`.
    TokenRangeDisplayRole,

    // Returns a `QString` of the model name.
    ModelIdRole,

    MultiplierUserRole = Qt::UserRole + 100
  };

  static RawEntityId EntityId(const QModelIndex &index);
  static VariantId UnpackEntityId(const QModelIndex &index);
  static VariantEntity Entity(const QModelIndex &index);

  static RawEntityId EntityIdSkipThroughTokens(const QModelIndex &index);
  static VariantId UnpackEntityIdSkipThroughTokens(const QModelIndex &index);
  static VariantEntity EntitySkipThroughTokens(const QModelIndex &index);

  static TokenRange TokensToDisplay(const QModelIndex &index);
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::IModel,
                    "com.trailofbits.interface.IModel")
