// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Interfaces/IModel.h>

#include <multiplier/Index.h>

namespace mx::gui {

IModel::~IModel(void) {}

RawEntityId IModel::EntityId(const QModelIndex &index) {
  QVariant var = index.data(EntityRole);
  if (!var.isValid()) {
    return kInvalidEntityId;
  }

  if (!var.canConvert<VariantEntity>()) {
    return kInvalidEntityId;
  }

  return mx::EntityId(var.value<VariantEntity>()).Pack();
}

VariantId IModel::UnpackEntityId(const QModelIndex &index) {
  QVariant var = index.data(EntityRole);
  if (!var.isValid()) {
    return {};
  }

  if (!var.canConvert<VariantEntity>()) {
    return {};
  }

  return mx::EntityId(var.value<VariantEntity>()).Unpack();
}

VariantEntity IModel::Entity(const QModelIndex &index) {
  QVariant var = index.data(EntityRole);
  if (!var.isValid()) {
    return {};
  }

  if (!var.canConvert<VariantEntity>()) {
    return {};
  }

  return var.value<VariantEntity>();
}

RawEntityId IModel::EntityIdSkipThroughTokens(const QModelIndex &index) {
  QVariant var = index.data(EntityRole);
  if (!var.isValid()) {
    return kInvalidEntityId;
  }

  if (!var.canConvert<VariantEntity>()) {
    return kInvalidEntityId;
  }

  VariantEntity entity = var.value<VariantEntity>();
  if (!std::holds_alternative<Token>(entity)) {
    return mx::EntityId(entity).Pack();
  }

  return std::get<Token>(entity).related_entity_id().Pack();
}

VariantId IModel::UnpackEntityIdSkipThroughTokens(const QModelIndex &index) {
  QVariant var = index.data(EntityRole);
  if (!var.isValid()) {
    return {};
  }

  if (!var.canConvert<VariantEntity>()) {
    return {};
  }

  VariantEntity entity = var.value<VariantEntity>();
  if (!std::holds_alternative<Token>(entity)) {
    return mx::EntityId(entity).Unpack();
  }

  return std::get<Token>(entity).related_entity_id().Unpack();
}

VariantEntity IModel::EntitySkipThroughTokens(const QModelIndex &index) {
  QVariant var = index.data(EntityRole);
  if (!var.isValid()) {
    return {};
  }

  if (!var.canConvert<VariantEntity>()) {
    return {};
  }

  VariantEntity entity = var.value<VariantEntity>();
  if (!std::holds_alternative<Token>(entity)) {
    return entity;
  }

  return std::get<Token>(entity).related_entity();
}

TokenRange IModel::TokensToDisplay(const QModelIndex &index) {
  auto var = index.data(TokenRangeDisplayRole);
  if (!var.isValid()) {
    return {};
  }

  if (!var.canConvert<TokenRange>()) {
    return {};
  }

  return var.value<TokenRange>();
}

}  // namespace mx::gui