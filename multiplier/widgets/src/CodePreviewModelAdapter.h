/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ICodeModel.h>
#include <multiplier/ui/IReferenceExplorerModel.h>

#include <multiplier/Entities/TokenCategory.h>

#include <memory>

namespace mx::gui {

//! A code view model that adapts the normal code model so that all related
//! entity IDs are actually the normal token ids.
class CodePreviewModelAdapter final : public ICodeModel {
  Q_OBJECT

 public:
  //! Constructor
  CodePreviewModelAdapter(ICodeModel *model, QObject *parent);

  //! Destructor
  virtual ~CodePreviewModelAdapter(void) = default;

  //! Asks the model for the currently showing entity. This is usually a file
  //! id or a fragment id.
  virtual std::optional<RawEntityId> GetEntity(void) const Q_DECL_FINAL;

  //! Asks the model to fetch the specified entity.
  virtual void SetEntity(RawEntityId id) Q_DECL_FINAL;

  //! Returns true if this model is ready.
  virtual bool IsReady() const Q_DECL_FINAL;

  //! Creates a new Qt model index
  virtual QModelIndex index(int row, int column,
                            const QModelIndex &parent) const override;

  //! Returns the parent of the given model index
  virtual QModelIndex parent(const QModelIndex &child) const override;

  //! Returns the amount of rows in the given parent item
  virtual int rowCount(const QModelIndex &parent) const override;

  //! Returns the amount of columns for the given parent item
  virtual int columnCount(const QModelIndex &parent) const override;

  //! Returns the data for the specified role
  virtual QVariant data(const QModelIndex &index, int role) const override;

 private:
  ICodeModel *const next;
};

}  // namespace mx::gui
