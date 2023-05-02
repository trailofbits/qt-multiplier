/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CodePreviewModelAdapter.h"

namespace mx::gui {

CodePreviewModelAdapter::CodePreviewModelAdapter(ICodeModel *model,
                                                 QObject *parent)
    : ICodeModel(parent),
      next(model) {
  next->setParent(this);

  connect(next, &ICodeModel::modelReset, this, &ICodeModel::modelReset);
}

std::optional<RawEntityId> CodePreviewModelAdapter::GetEntity(void) const {
  return next->GetEntity();
}

void CodePreviewModelAdapter::SetEntity(RawEntityId id) {
  next->SetEntity(id);
}

bool CodePreviewModelAdapter::IsReady() const {
  return next->IsReady();
}

QModelIndex CodePreviewModelAdapter::index(int row, int column,
                                           const QModelIndex &parent) const {

  return next->index(row, column, parent);
}

QModelIndex CodePreviewModelAdapter::parent(const QModelIndex &child) const {

  return next->parent(child);
}

int CodePreviewModelAdapter::rowCount(const QModelIndex &parent) const {
  return next->rowCount(parent);
}

int CodePreviewModelAdapter::columnCount(const QModelIndex &parent) const {
  return next->columnCount(parent);
}

QVariant CodePreviewModelAdapter::data(const QModelIndex &index,
                                       int role) const {
  role = (role == ICodeModel::RelatedEntityIdRole) ? ICodeModel::TokenIdRole
                                                   : role;
  return next->data(index, role);
}


}  // namespace mx::gui
