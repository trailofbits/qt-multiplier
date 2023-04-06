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

  connect(next, &ICodeModel::ModelReset,
          this, &ICodeModel::ModelReset);

  connect(next, &ICodeModel::ModelResetSkipped,
          this, &ICodeModel::ModelResetSkipped);
}

//! Returns the internal mx::FileLocationCache object
const FileLocationCache &
CodePreviewModelAdapter::GetFileLocationCache(void) const {
  return next->GetFileLocationCache();
}

//! Returns the internal mx::Index object
Index &CodePreviewModelAdapter::GetIndex() {
  return next->GetIndex();
}

//! Asks the model for the currently showing entity. This is usually a file
//! id or a fragment id.
std::optional<RawEntityId> CodePreviewModelAdapter::GetEntity(void) const {
  return next->GetEntity();
}

//! Asks the model to fetch the specified entity.
void CodePreviewModelAdapter::SetEntity(RawEntityId id) {
  next->SetEntity(id);
}

//! How many rows are accessible from this model
Count CodePreviewModelAdapter::RowCount() const {
  return next->RowCount();
}

//! How many tokens are accessible on the specified column
Count CodePreviewModelAdapter::TokenCount(Count row) const {
  return next->TokenCount(row);
}

//! Returns the data role contents for the specified model index
QVariant CodePreviewModelAdapter::Data(const CodeModelIndex &index,
                                       int role = Qt::DisplayRole) const {
  if (role == ICodeModel::TokenRelatedEntityIdRole) {
    return next->Data(index, ICodeModel::TokenRawEntityIdRole);
  } else {
    return next->Data(index, role);
  }
}

//! Returns true if this model is ready.
bool CodePreviewModelAdapter::IsReady() const {
  return next->IsReady();
}

}  // namespace mx::gui
